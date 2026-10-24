using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using UnityEngine;
using UnityEngine.SceneManagement;

// Reuse original level renderers, preserving static-batch ranges and transforms.
// Only the camera and recorded markers are new. Every temporary style, active
// state and paused game routine is restored on exit or failure.
public static class Replay3D
{
    private sealed class Loaded
    {
        public PracticeMapData Map;
        public ReplayMotionData Motion;
        public string Error, AudioPath, AudioError;
        public int Generation;
    }

    private sealed class Marker
    {
        public GameObject Object;
        public ReplayMonster Track;
    }

    private static string directory;
    private static GameBuildInfo build;
    private static Loaded pending, current;
    private static readonly object loadGate = new object();
    private static double loadDeadline;
    private static int generation, state, loadFrame, layer, wallLayer = -1, sceneMask,
                                                            visibilityRevision = -1, swallowFrame = -1;
    private static bool changingScene, playing, observer;
    private static string message = "";
    private static double seconds;
    private static float speed = 1;
    private static ReplayRestoreScope restore;
    private static MouseLook player;
    private static Camera view;
    private static GameObject root;
    private static readonly List<Marker> monsters = new List<Marker>();
    private static readonly List<GameObject> crystals = new List<GameObject>();
    private static readonly ReplayPlaybackClock clock = new ReplayPlaybackClock();
    private static bool clockReady;
    private static Renderer[] renderers;
    private static int rendererCursor, geometryCount, batchCount, revealedCount;
    private static Material[] palette;
    private static Material playerMaterial, monsterMaterial, crystalMaterial;
    private static GameObject playerMarker;
    private static bool nativeStyle;
    private static ReplayNativeGraphics native;
    private static ReplayScenePlayback scenePlayback;
    private static ReplayAudioPlayback audio;
    private static double audioPreviewAt, audioPreviewClock, audioPreviewDeadline;
    private static float audioPreviewRate, audioPreviewVolume;

    public static void PreviewAudio(double at, float rate, float volume)
    {
        audioPreviewAt = at;
        audioPreviewRate = rate;
        audioPreviewVolume = volume;
        audioPreviewClock = Now;
        audioPreviewDeadline = rate > 0 ? Now + .3 : 0;
        if (audio != null)
            audio.Tick(at, rate, rate > 0, volume);
    }

    private static SceneBrightness replayBrightness;
    private static readonly Dictionary<Light, bool> originalLights = new Dictionary<Light, bool>();
    private static Replay3DOverlay analysis;
    private static ReplaySceneVisibility visibility;
    private static Material[] transparentPalette;
    private static Material hiddenMaterial;
    private static uint options;
    private static double loopA, loopB;
    private static float trailSeconds = 3;
    private static readonly ReplayFollowOrbit followOrbit = new ReplayFollowOrbit();
    private static double[] crystalTimes;
    private static string sceneMessage = "", sceneStatus = "";
    private static double Now
    {
        get {
            return System.Diagnostics.Stopwatch.GetTimestamp() /
                   (double)System.Diagnostics.Stopwatch.Frequency;
        }
    }
    public static bool Reserved
    {
        get {
            return state == 1 || state == 2 || state == 3;
        }
    }

    public static void Initialize(string dataDirectory, GameBuildInfo identity)
    {
        directory = Path.Combine(dataDirectory, "RunReviews", "ReplayInbox");
        build = identity;
    }

    public static bool AllowPlayerUpdate()
    {
        if (Reserved)
            return false;
        if (swallowFrame < 0)
            return true;
        if (Time.frameCount == swallowFrame)
            return false;
        swallowFrame = -1;
        return true;
    }

    public static bool AllowPlayerTrigger()
    {
        return !Reserved;
    }

    public static void SceneLoaded(int index)
    {
        if (changingScene && current != null && current.Map.Scene == index)
        {
            changingScene = false;
            loadFrame = Time.frameCount;
            loadDeadline = Time.realtimeSinceStartupAsDouble + 10;
            return;
        }
        if (Reserved)
            Stop();
    }

    private static byte[] Read(string file, int maximum)
    {
        var info = new FileInfo(file);
        if (info.Length > maximum)
            throw new InvalidDataException("Replay file exceeds limit");
        return File.ReadAllBytes(file);
    }

    public static void Start(long id, double at, uint options, float rate)
    {
        if (SpeedrunManager.Instance != null && SpeedrunManager.Instance.Running)
            return;
        Stop();
        if (id <= 0 || directory == null)
        {
            Fail("Replay source unavailable");
            return;
        }
        if (SpeedrunManager.Instance != null)
            SpeedrunManager.Instance.PrepareReplay();
        state = 1;
        message = "Loading verified replay files";
        seconds = SafeTime(at, 315360000);
        playing = (options & 1) != 0;
        observer = (options & 2) != 0;
        speed = SafeSpeed(rate);
        int token = ++generation;
        loadDeadline = Time.realtimeSinceStartupAsDouble + 15;
        new Thread(() =>
                   {
                       var loaded = new Loaded { Generation = token };
                       try
                       {
                           var prefix = Path.Combine(directory, "run-" + id);
                           loaded.Map = PracticeMapArchive.Decode(
                               Read(prefix + ".isrun", PracticeMapChannel.Capacity - 64));
                           loaded.Motion =
                               ReplayMotionData.Read(Read(prefix + ".isinput", ReplayMotionData.MaxBytes));
                           if (loaded.Map.Review.Id != id || !loaded.Motion.Matches(loaded.Map) ||
                               loaded.Motion.Frames.Count == 0)
                               throw new InvalidDataException("Camera recording does not match this run");
                           if (loaded.Motion.Experience.Audio.Recorded)
                           {
                               try
                               {
                                   string path = prefix + ".isaudio";
                                   ReplayAudioFile.Verify(path, id, loaded.Motion.Scene, loaded.Motion.Seed,
                                                          loaded.Motion.Hash, loaded.Motion.Experience.Audio);
                                   loaded.AudioPath = path;
                               }
                               catch (Exception e)
                               {
                                   loaded.AudioError = e.Message;
                               }
                           }
                       }
                       catch (Exception e)
                       {
                           loaded.Error = e.Message;
                       }
                       lock (loadGate) if (token == generation) Volatile.Write(ref pending, loaded);
                   }) { IsBackground = true, Name = "ItSteals.Replay3DLoad",
                        Priority = System.Threading.ThreadPriority.BelowNormal }
            .Start();
    }

    private static double SafeTime(double value, double duration)
    {
        return double.IsNaN(value) || double.IsInfinity(value) ? 0 : Math.Max(0, Math.Min(duration, value));
    }

    private static float SafeSpeed(float value)
    {
        return float.IsNaN(value) || float.IsInfinity(value) ? 1 : Mathf.Clamp(value, .1f, 8);
    }

    private static float Safe(float value, float min, float max, float fallback)
    {
        return float.IsNaN(value) || float.IsInfinity(value) ? fallback : Mathf.Clamp(value, min, max);
    }

    private static void Configure(TrainerCommands c)
    {
        options = c.Replay3dOptions & Replay3DOptions.Mask;
        if ((options & Replay3DOptions.Free) != 0)
            options &= ~Replay3DOptions.Follow;
        loopA = SafeTime(c.Replay3dLoopA, 315360000);
        loopB = SafeTime(c.Replay3dLoopB, 315360000);
        trailSeconds = Safe(c.Replay3dTrailSeconds, .25f, 10, 3);
    }

    private static bool SameLayout(PracticeMapData a, PracticeMapData b)
    {
        if (a == null || b == null || a.Scene != b.Scene || a.Hash != b.Hash || a.Spawn < 0 || b.Spawn < 0 ||
            a.Spawn >= a.Spawns.Length || b.Spawn >= b.Spawns.Length ||
            a.Selected.Length != b.Selected.Length ||
            (a.Spawns[a.Spawn] - b.Spawns[b.Spawn]).sqrMagnitude > .0004f)
            return false;
        var expected = new Vector3[a.Selected.Length];
        var actual = new Vector3[b.Selected.Length];
        for (int i = 0; i < expected.Length; i++)
        {
            int x = a.Selected[i], y = b.Selected[i];
            if (x < 0 || x >= a.Points.Length || y < 0 || y >= b.Points.Length)
                return false;
            expected[i] = a.Points[x];
            actual[i] = b.Points[y];
        }
        return SeedMapPlanner.SamePositions(expected, actual);
    }

    public static void Tick(TrainerCommands commands, bool request, bool connected)
    {
        try
        {
            if (!connected && Reserved)
            {
                Stop();
                return;
            }
            if (commands.Replay3dCmd == 2)
            {
                Stop();
                return;
            }
            if (request && commands.Replay3dCmd == 1)
            {
                Start(commands.Replay3dRunId, commands.Replay3dSeconds, commands.Replay3dOptions,
                      commands.Replay3dSpeed);
                Configure(commands);
            }
            if (request && commands.Replay3dCmd == 3 && current != null &&
                commands.Replay3dRunId == current.Map.Review.Id &&
                ((commands.Replay3dOptions ^ options) & Replay3DOptions.Simplified) != 0)
            {
                Start(commands.Replay3dRunId, commands.Replay3dSeconds, commands.Replay3dOptions,
                      commands.Replay3dSpeed);
                Configure(commands);
            }
            else if (request && commands.Replay3dCmd == 3 && current != null &&
                     commands.Replay3dRunId == current.Map.Review.Id)
            {
                if (audio != null && (commands.Replay3dOptions & Replay3DOptions.Seek) != 0)
                    audio.Seek();
                seconds = (commands.Replay3dOptions & Replay3DOptions.Seek) != 0
                              ? SafeTime(commands.Replay3dSeconds, current.Motion.Seconds)
                          : clockReady ? clock.At(Now)
                                       : seconds;
                playing = (commands.Replay3dOptions & 1) != 0;
                SetObserver((commands.Replay3dOptions & 2) != 0);
                Configure(commands);
                speed = SafeSpeed(commands.Replay3dSpeed);
                if (clockReady)
                {
                    clock.Set(Now, seconds, current.Motion.Seconds, playing, speed);
                    clock.Loop((options & Replay3DOptions.Loop) != 0, loopA, loopB);
                }
            }
            if (!Reserved)
                return;
            if (MenuCursor.GameHotkeysAllowed && Input.GetKeyDown(KeyCode.Escape))
            {
                Stop();
                return;
            }
            if (state == 1 && Time.realtimeSinceStartupAsDouble > loadDeadline)
            {
                Fail("Replay loading timed out; gameplay was restored");
                return;
            }
            if (SpeedrunManager.Instance != null && SpeedrunManager.Instance.Running)
            {
                Fail("Replay stopped because a run became active");
                return;
            }
            var value = Interlocked.Exchange(ref pending, null);
            if (value != null && value.Generation == generation)
            {
                if (value.Error != null)
                {
                    Fail(value.Error);
                    return;
                }
                current = value;
                if (build == null || !build.Supported || current.Map.Hash != build.Hash)
                {
                    Fail("3D replay requires the recorded supported game build");
                    return;
                }
                seconds = SafeTime(seconds, current.Motion.Seconds);
                loadFrame = Time.frameCount;
                if ((options & Replay3DOptions.Simplified) == 0 ||
                    SceneManager.GetActiveScene().buildIndex != current.Map.Scene ||
                    !SameLayout(current.Map, SeedMapPlanner.RunSnapshot()))
                {
                    message = "Loading recorded level and checking its layout";
                    changingScene = true;
                    if (SpeedrunManager.Instance != null)
                        SpeedrunManager.Instance.BeforeReload(current.Map.Scene);
                    PracticeRandom.Begin(current.Map.Seed, current.Map.Scene, true, true);
                    SceneManager.LoadScene(current.Map.Scene);
                    return;
                }
            }
            if (current == null || changingScene)
                return;
            if (root == null)
            {
                if (Time.frameCount - loadFrame < 2)
                    return;
                player = PlayerContext.Find();
                var map = SeedMapPlanner.RunSnapshot();
                if (player == null || map == null)
                {
                    if (Time.frameCount - loadFrame > 300)
                        Fail("Recorded level did not initialize");
                    return;
                }
                if (!SameLayout(current.Map, map))
                {
                    Fail("Loaded spawn/crystal layout differs from the recording; 3D replay was stopped");
                    return;
                }
                BeginStage();
            }
            if (renderers != null)
            {
                BuildStage();
                return;
            }
        }
        catch (Exception e)
        {
            Fail("3D replay: " + e.Message);
        }
    }

    private static bool Dynamic(Transform t)
    {
        return ReplayVisualCapture.Dynamic(t);
    }

    private static bool Point(Transform t)
    {
        for (var p = t; p != null; p = p.parent)
            if (p.CompareTag("Point"))
                return true;
        return false;
    }

    private static void BeginStage()
    {
        nativeStyle = (options & Replay3DOptions.Simplified) == 0;
        FreeCam.DisableActive();
        var original = PlayerContext.View(player);
        if (original == null)
            throw new InvalidOperationException("Gameplay camera unavailable");
        renderers = UnityEngine.Object.FindObjectsOfType<Renderer>(true);
        var used = new bool[32];
        foreach (var renderer in renderers)
            if (renderer.gameObject.scene.IsValid())
                used[renderer.gameObject.layer] = true;
        layer = -1;
        for (int i = 31; i >= 8; i--)
            if (!used[i])
            {
                layer = i;
                break;
            }
        if (layer < 0)
            throw new InvalidOperationException("No marker render layer available");
        restore = new ReplayRestoreScope();
        float oldScale = Time.timeScale, oldVolume = AudioListener.volume;
        bool oldAudio = AudioListener.pause, oldBackground = Application.runInBackground,
             oldPlayer = player.enabled,
             oldHeadBob = player.HeadBobAnimation != null && player.HeadBobAnimation.enabled,
             oldFog = RenderSettings.fog;
        var originalPlayer = player;
        restore.Add(() => Time.timeScale = oldScale);
        restore.Add(() => AudioListener.pause = oldAudio);
        restore.Add(() => AudioListener.volume = oldVolume);
        restore.Add(() => Application.runInBackground = oldBackground);
        restore.Add(() =>
                    {
                        if (originalPlayer != null)
                            originalPlayer.enabled = oldPlayer;
                    });
        restore.Add(() =>
                    {
                        if (originalPlayer != null && originalPlayer.HeadBobAnimation != null)
                            originalPlayer.HeadBobAnimation.enabled = oldHeadBob;
                    });
        restore.Add(() => RenderSettings.fog = oldFog);
        var ambientMode = RenderSettings.ambientMode;
        var ambient = RenderSettings.ambientLight;
        var ambientProbe = RenderSettings.ambientProbe;
        float reflection = RenderSettings.reflectionIntensity;
        restore.Add(() =>
                    {
                        RenderSettings.ambientMode = ambientMode;
                        RenderSettings.ambientLight = ambient;
                        RenderSettings.reflectionIntensity = reflection;
                        RenderSettings.ambientProbe = ambientProbe;
                    });
        if (!nativeStyle)
        {
            RenderSettings.ambientMode = UnityEngine.Rendering.AmbientMode.Flat;
            RenderSettings.ambientLight = new Color(.47f, .52f, .60f);
            RenderSettings.reflectionIntensity = 0;
        }
        ReplayGamePause.Frozen = true;
        Time.timeScale = 0;
        AudioListener.pause = true;
        Application.runInBackground = true;
        if (!nativeStyle)
            RenderSettings.fog = false;
        player.enabled = false;
        if (player.HeadBobAnimation != null)
            player.HeadBobAnimation.enabled = false;
        foreach (var animator in UnityEngine.Object.FindObjectsOfType<Animator>(true))
        {
            var saved = animator;
            bool enabled = animator.enabled;
            restore.Add(() =>
                        {
                            if (saved != null)
                                saved.enabled = enabled;
                        });
            animator.enabled = false;
        }
        foreach (var camera in UnityEngine.Object.FindObjectsOfType<Camera>())
        {
            var saved = camera;
            bool enabled = camera.enabled;
            restore.Add(() =>
                        {
                            if (saved != null)
                                saved.enabled = enabled;
                        });
            camera.enabled = false;
        }
        originalLights.Clear();
        foreach (var light in UnityEngine.Object.FindObjectsOfType<Light>(true))
        {
            var saved = light;
            bool enabled = light.enabled;
            originalLights[light] = enabled;
            restore.Add(() =>
                        {
                            if (saved != null)
                                saved.enabled = enabled;
                        });
            light.enabled = false;
        }
        // Re-enable only the game's own distance-culling catalogs, not arbitrary
        // inactive variants. Their game coroutines are suspended, not destroyed.
        var seen = new HashSet<int>();
        int scene = player.gameObject.scene.handle;
        revealedCount = 0;
        foreach (var actor in UnityEngine.Object.FindObjectsOfType<HideAndSeekIt>(true))
            if (actor.gameObject.scene.handle == scene)
                foreach (var obj in ReplayGamePause.CulledGeometry(actor))
                    if (obj.scene.handle == scene && seen.Add(obj.GetInstanceID()) && !obj.activeSelf)
                    {
                        ReplayRendererScope.Reveal(restore, obj);
                        revealedCount++;
                    }
        root = new GameObject("PracticeSuite replay camera and markers");
        root.layer = layer;
        GameObject cameraObject;
        if (nativeStyle)
        {
            var backupObject = new GameObject("Saved game camera settings");
            backupObject.SetActive(false);
            backupObject.transform.SetParent(root.transform, false);
            var backup = backupObject.AddComponent<Camera>();
            backup.CopyFrom(original);
            var saved = original;
            var savedPosition = original.transform.localPosition;
            var savedRotation = original.transform.localRotation;
            var savedScale = original.transform.localScale;
            restore.Add(() =>
                        {
                            if (saved != null && backup != null)
                            {
                                saved.CopyFrom(backup);
                                saved.transform.localPosition = savedPosition;
                                saved.transform.localRotation = savedRotation;
                                saved.transform.localScale = savedScale;
                            }
                        });
            view = original;
            cameraObject = original.gameObject;
            sceneMask = original.cullingMask;
            view.cullingMask = sceneMask | (1 << layer);
            view.targetTexture = null;
            view.rect = new Rect(0, 0, 1, 1);
            view.enabled = false;
        }
        else
        {
            cameraObject = new GameObject("Recorded camera");
            cameraObject.transform.SetParent(root.transform, false);
            view = cameraObject.AddComponent<Camera>();
            view.CopyFrom(original);
            view.targetTexture = null;
            view.rect = new Rect(0, 0, 1, 1);
            sceneMask = original.cullingMask;
            view.cullingMask = sceneMask | (1 << layer);
            view.depth = 100;
            view.enabled = false;
            view.clearFlags = CameraClearFlags.SolidColor;
            view.backgroundColor = new Color(.04f, .055f, .075f, 1);
            view.renderingPath = RenderingPath.Forward;
            view.allowHDR = false;
            view.allowMSAA = false;
            view.useOcclusionCulling = false;
            view.depthTextureMode = DepthTextureMode.None;
            view.nearClipPlane = .03f;
            view.farClipPlane = Math.Max(250, original.farClipPlane);
        }
        var first = current.Motion.Frames[0];
        view.transform.SetPositionAndRotation(first.Camera, first.CameraRotation);
        view.fieldOfView = first.Fov;
        var shader = FindShader(true);
        var baseColors = new[] { new Color(.52f, .63f, .71f, 1), new Color(.24f, .32f, .38f, 1),
                                 new Color(.37f, .43f, .51f, 1), new Color(.68f, .49f, .29f, 1),
                                 new Color(.28f, .67f, .54f, 1) };
        palette = new Material[20];
        for (int role = 0; role < 5; role++)
            for (int variation = 0; variation < 4; variation++)
            {
                var color = baseColors[role] * (.85f + .09f * variation);
                color.a = 1;
                palette[role * 4 + variation] = Material(shader, color);
            }
        var alphaShader = FindAlphaShader();
        transparentPalette = new Material[palette.Length];
        for (int i = 0; i < palette.Length; i++)
            transparentPalette[i] = ReplayFadeMaterial.Create(alphaShader, palette[i].color);
        hiddenMaterial = ReplayFadeMaterial.Create(alphaShader, new Color(0, 0, 0, 0));
        if (!nativeStyle && (shader.name == "Standard" || shader.name.Contains("Diffuse")))
        {
            var keyObject = new GameObject("Replay depth light");
            keyObject.transform.SetParent(root.transform, false);
            keyObject.transform.rotation = Quaternion.Euler(48, -32, 0);
            var key = keyObject.AddComponent<Light>();
            key.type = LightType.Directional;
            key.color = new Color(1, .94f, .85f);
            key.intensity = .95f;
            key.shadows = LightShadows.None;
            key.cullingMask = ~0;
        }
        var markerShader = FindShader(false);
        playerMaterial = Material(markerShader, new Color(.85f, .3f, .8f, 1));
        monsterMaterial = Material(markerShader, new Color(1, .3f, .2f, 1));
        crystalMaterial = Material(markerShader, new Color(.2f, 1, .8f, 1));
        analysis = cameraObject.AddComponent<Replay3DOverlay>();
        analysis.Motion = current.Motion;
        if (current.AudioPath != null)
        {
            try
            {
                // A dedicated listener/source avoids applying camera audio
                // filters a second time to the already recorded mix.
                foreach (var listener in UnityEngine.Object.FindObjectsOfType<AudioListener>(true))
                    if (listener.enabled)
                    {
                        var saved = listener;
                        listener.enabled = false;
                        restore.Add(() =>
                                    {
                                        if (saved != null)
                                            saved.enabled = true;
                                    });
                    }
                var replayListener = root.AddComponent<AudioListener>();
                restore.Add(() =>
                            {
                                if (replayListener != null)
                                {
                                    replayListener.enabled = false;
                                    UnityEngine.Object.Destroy(replayListener);
                                }
                            });
                audio = new ReplayAudioPlayback(root, current.AudioPath, current.Motion.Experience.Audio);
                restore.Add(audio.Dispose);
                AudioListener.volume = 1;
            }
            catch (Exception e)
            {
                current.AudioError = e.Message;
            }
        }
        if (nativeStyle)
            native =
                new ReplayNativeGraphics(root, player, current.Map, current.Motion, restore, originalLights);
        if (native != null)
        {
            native.BindView(view);
            analysis.BeforeRender = native.PrepareRender;
        }
        if (nativeStyle && current.Motion.SceneData.Recorded)
            scenePlayback = new ReplayScenePlayback(current.Motion.SceneData, player, restore,
                                                    current.Motion.Experience.Names);
        var lighting = new SceneBrightness();
        replayBrightness = lighting;
        restore.Add(lighting.Dispose);
        foreach (var renderer in renderers)
            if (renderer != null && renderer.gameObject.scene.handle == scene &&
                ReplaySceneVisibility.Included(sceneMask, renderer.gameObject.layer))
                lighting.Add(renderer);
        foreach (var renderer in root.GetComponentsInChildren<Renderer>(true))
            if (ReplaySceneVisibility.Included(sceneMask, renderer.gameObject.layer))
                lighting.Add(renderer);
        if (!analysis.Ready)
            throw new InvalidOperationException("Replay overlay shader unavailable");
        var sceneColliders = new List<Collider>();
        var playerBody = PlayerContext.Body(player);
        foreach (var collider in UnityEngine.Object.FindObjectsOfType<Collider>(true))
            if (collider != null && collider != player.controller &&
                collider.gameObject.scene.handle == scene &&
                (playerBody == null ||
                 collider.transform != playerBody && !collider.transform.IsChildOf(playerBody)) &&
                !Dynamic(collider.transform) && !Point(collider.transform))
            {
                analysis.AddStatic(collider);
                sceneColliders.Add(collider);
            }
        wallLayer = LayerMask.NameToLayer("Walls");
        var cc = player.controller;
        float playerHeight = Math.Max(.5f, cc.height * Math.Abs(cc.transform.lossyScale.y));
        float floorY = first.Body.y + cc.transform.TransformPoint(cc.center).y - playerHeight * .5f -
                       PlayerContext.Body(player).position.y;
        visibility = new ReplaySceneVisibility(
            sceneColliders.ToArray(), wallLayer,
            LayerMask.GetMask("Props", "Clock", "Writing", "PhysicsProp", "SmallProp"),
            LayerMask.GetMask("Floor"), LayerMask.GetMask("Cieling"), floorY, playerHeight, first.Camera.y);
        visibilityRevision = -1;
        playerMarker = Ball("Player marker", playerMaterial, .3f);
        foreach (var actor in current.Map.Review.Monsters)
            monsters.Add(new Marker { Object = Ball("Recorded monster " + actor.Id, monsterMaterial, .45f),
                                      Track = actor });
        var collected = new Dictionary<int, double>();
        for (int i = 0; i < current.Map.Review.Collected.Count; i++)
            collected[current.Map.Review.Collected[i]] = current.Map.Review.PickupSeconds[i];
        crystalTimes = new double[current.Map.Selected.Length];
        int k = 0;
        foreach (int point in current.Map.Selected)
        {
            var crystal = Ball("Recorded crystal " + point, crystalMaterial, .15f);
            crystal.transform.position = current.Map.Points[point];
            crystals.Add(crystal);
            double at;
            crystalTimes[k++] = collected.TryGetValue(point, out at) ? at : double.PositiveInfinity;
        }
        rendererCursor = geometryCount = batchCount = 0;
        message = "Preparing original scene renderers";
        SetObserver(observer, true);
    }

    private static Shader FindShader(bool lit)
    {
        foreach (string name in lit ? new[] { "Standard", "Legacy Shaders/Diffuse", "Unlit/Color",
                                              "Hidden/Internal-Colored" }
                                    : new[] { "Unlit/Color", "Hidden/Internal-Colored" })
        {
            var shader = Shader.Find(name);
            if (shader != null && shader.isSupported)
                return shader;
        }
        throw new InvalidOperationException("No supported unlit replay shader available");
    }

    private static Shader FindAlphaShader()
    {
        foreach (string name in new[] { "Standard", "Legacy Shaders/Transparent/Diffuse", "Unlit/Transparent",
                                        "Hidden/Internal-Colored", "Sprites/Default", "UI/Default" })
        {
            var shader = Shader.Find(name);
            if (shader != null && shader.isSupported)
                return shader;
        }
        throw new InvalidOperationException("No supported transparent replay shader available");
    }

    private static void Int(Material material, string property, int value)
    {
        if (material.HasProperty(property))
            material.SetInt(property, value);
    }

    private static Material Material(Shader shader, Color color)
    {
        var material = new Material(shader) { color = color, enableInstancing = true };
        if (material.HasProperty("_Glossiness"))
            material.SetFloat("_Glossiness", 0);
        if (material.HasProperty("_Metallic"))
            material.SetFloat("_Metallic", 0);
        Int(material, "_SpecularHighlights", 0);
        Int(material, "_GlossyReflections", 0);
        material.DisableKeyword("_NORMALMAP");
        material.DisableKeyword("_EMISSION");
        Int(material, "_SrcBlend", 1);
        Int(material, "_DstBlend", 0);
        Int(material, "_Cull", 0);
        Int(material, "_ZWrite", 1);
        Int(material, "_ZTest", 4);
        return material;
    }

    private static GameObject Ball(string name, Material material, float radius)
    {
        var g = GameObject.CreatePrimitive(PrimitiveType.Sphere);
        g.name = name;
        var collider = g.GetComponent<Collider>();
        if (collider != null)
            UnityEngine.Object.DestroyImmediate(collider);
        g.layer = layer;
        g.transform.SetParent(root.transform, false);
        g.transform.localScale = Vector3.one * (radius * 2);
        var renderer = g.GetComponent<Renderer>();
        renderer.sharedMaterial = material;
        renderer.shadowCastingMode = UnityEngine.Rendering.ShadowCastingMode.Off;
        renderer.receiveShadows = false;
        return g;
    }

    private static Material[] Materials(Renderer renderer, out int[] indices)
    {
        var originals = renderer.sharedMaterials;
        var bounds = renderer.bounds;
        int protect = visibility.ProtectedRole(renderer),
            named = ReplaySceneVisibility.NamedRole(renderer.name, 3),
            fallback =
                ReplaySceneVisibility.FallbackRole(renderer, wallLayer, current.Motion.Frames[0].Camera.y);
        int variation = unchecked(
            (renderer.name.GetHashCode() ^ (int)(bounds.center.x * 17) ^ (int)(bounds.center.z * 31)) & 3);
        var result = new Material[originals.Length];
        indices = new int[originals.Length];
        for (int i = 0; i < result.Length; i++)
        {
            indices[i] = (protect >= 0 ? protect
                          : named == 2 ? 2
                                       : ReplaySceneVisibility.NamedRole(
                                             originals[i] == null ? "" : originals[i].name, fallback)) *
                             4 +
                         variation;
            result[i] = nativeStyle ? originals[i] : palette[indices[i]];
        }
        return result;
    }

    private static void BuildStage()
    {
        int scene = player.gameObject.scene.handle;
        long start = System.Diagnostics.Stopwatch.GetTimestamp();
        int count = 0;
        while (rendererCursor < renderers.Length && count++ < 64)
        {
            var renderer = renderers[rendererCursor++];
            if (renderer == null || renderer.gameObject.scene.handle != scene)
                continue;
            // Body-camera layers are outside the main camera mask too. Their
            // original models must be hidden when recorded proxies are used.
            if (nativeStyle && Dynamic(renderer.transform))
            {
                ReplayRendererScope.Hide(restore, renderer);
                continue;
            }
            if (!ReplaySceneVisibility.Included(sceneMask, renderer.gameObject.layer))
                continue;
            if (Dynamic(renderer.transform) || Point(renderer.transform) ||
                !(renderer is MeshRenderer) && !(renderer is SkinnedMeshRenderer) &&
                    !(nativeStyle && (renderer is SpriteRenderer ||
                                      scenePlayback != null && renderer is ParticleSystemRenderer)) ||
                !nativeStyle && renderer.GetComponentInParent<VolumetricLight>(true) != null)
            {
                ReplayRendererScope.Hide(restore, renderer);
                continue;
            }
            if (!renderer.gameObject.activeInHierarchy || !renderer.enabled)
                continue;
            int[] indices;
            var materials = Materials(renderer, out indices);
            if (materials.Length == 0)
                continue;
            bool mainWall = ReplaySceneVisibility.IsWallVisual(renderer, wallLayer);
            if (!nativeStyle)
                ReplayRendererScope.Apply(restore, renderer, materials);
            else
            {
                var r = renderer;
                var oldMaterials = r.sharedMaterials;
                restore.Add(() =>
                            {
                                if (r != null)
                                    r.sharedMaterials = oldMaterials;
                            });
            }
            if (replayBrightness != null)
                replayBrightness.AddMaterials(materials);
            visibility.Add(renderer, materials, indices, transparentPalette, hiddenMaterial, mainWall,
                           nativeStyle);
            geometryCount++;
            if (renderer.isPartOfStaticBatch)
                batchCount++;
            if (System.Diagnostics.Stopwatch.GetTimestamp() - start >
                System.Diagnostics.Stopwatch.Frequency / 500)
                break;
        }
        if (rendererCursor >= renderers.Length)
        {
            renderers = null;
            if (geometryCount == 0)
                throw new InvalidOperationException("No level renderers found; original scene restored");
            sceneMessage =
                (nativeStyle ? native.Status + (scenePlayback == null ? " / no recorded game UI or particles"
                                                                      : " / recorded game UI and particles")
                             : "Analysis materials") +
                " / " + geometryCount + " renderers / " + batchCount + " batched";
            message = sceneStatus = sceneMessage;
            MelonLoader.MelonLogger.Msg(
                "Replay3D: " + sceneMessage + " / camera mask " + sceneMask.ToString("X8") + " / alpha " +
                transparentPalette[0].shader.name + " / instancing off / fade candidates " +
                visibility.Candidates + " / revealed " + revealedCount + " / overlay: " +
                analysis.ShaderName + " / collider tracks: " + current.Motion.Colliders.Tracks.Count +
                " / samples: " + current.Motion.Colliders.Count);
            clock.Set(Now, seconds, current.Motion.Seconds, playing, speed);
            clock.Loop((options & Replay3DOptions.Loop) != 0, loopA, loopB);
            clockReady = true;
            state = playing ? 2 : 3;
            view.enabled = true;
        }
    }

    private static bool capturingClip;

    public static ClipCameraPose CaptureClip(long id, double at, uint requested, RenderTexture target,
                                             Texture2D destination, ClipCameraPose? custom = null)
    {
        if (SpeedrunManager.Instance != null && SpeedrunManager.Instance.Running)
            throw new InvalidOperationException("Clip export is unavailable during an active run");
        if (((requested & Replay3DOptions.Simplified) == 0) != nativeStyle)
            throw new InvalidOperationException("Replay graphics profile is still changing");
        if (!clockReady || current == null || view == null || state < 2 || state > 3 ||
            current.Map.Review.Id != id || at < 0 || at > current.Motion.Seconds)
            throw new InvalidOperationException("Open the matching 3D replay before exporting");
        int frame = FrameAt(at);
        if (frame < 0 || at - current.Motion.Frames[frame].Seconds > .25)
            throw new InvalidOperationException("Clip includes a gap in the recorded camera");
        double oldSeconds = seconds;
        uint oldOptions = options;
        bool oldObserver = observer;
        var oldTarget = view.targetTexture;
        float oldAspect = view.aspect;
        var oldRect = view.rect;
        var oldActive = RenderTexture.active;
        var oldPosition = view.transform.position;
        var oldRotation = view.transform.rotation;
        float oldFov = view.fieldOfView;
        var savedVisibility = visibility.Preserve();
        int oldVisibilityRevision = visibilityRevision;
        string oldSceneStatus = sceneStatus, oldMessage = message;
        var savedOrbit = followOrbit.Copy();
        try
        {
            capturingClip = true;
            seconds = at;
            options = requested & (Replay3DOptions.Mask | 8192u);
            observer = (options & Replay3DOptions.Free) != 0;
            followOrbit.Ensure(current.Motion.Frames[0].CameraRotation);
            Apply();
            if ((requested & 8192u) == 0)
                foreach (var marker in monsters)
                    Visible(marker.Object, false);
            if (custom.HasValue)
            {
                custom.Value.Validate();
                view.transform.SetPositionAndRotation(custom.Value.Position, custom.Value.Rotation);
                view.fieldOfView = custom.Value.Fov;
            }
            if (native != null)
                native.Face(view);
            view.targetTexture = target;
            view.aspect = (float)target.width / target.height;
            view.rect = new Rect(0, 0, 1, 1);
            if (scenePlayback != null)
                scenePlayback.Render(view, target, true);
            view.Render();
            if (scenePlayback != null)
                scenePlayback.Render(view, target, false);
            RenderTexture.active = target;
            if (destination != null)
                destination.ReadPixels(new Rect(0, 0, target.width, target.height), 0, 0, false);
            return new ClipCameraPose { Position = view.transform.position,
                                        Rotation = view.transform.rotation, Fov = view.fieldOfView };
        }
        finally
        {
            view.targetTexture = oldTarget;
            view.aspect = oldAspect;
            view.rect = oldRect;
            RenderTexture.active = oldActive;
            seconds = oldSeconds;
            options = oldOptions;
            observer = oldObserver;
            try
            {
                followOrbit.Restore(savedOrbit);
                Apply();
            }
            finally
            {
                try
                {
                    savedVisibility.Dispose();
                }
                finally
                {
                    followOrbit.Restore(savedOrbit);
                    visibilityRevision = oldVisibilityRevision;
                    sceneStatus = oldSceneStatus;
                    message = oldMessage;
                    view.transform.SetPositionAndRotation(oldPosition, oldRotation);
                    view.fieldOfView = oldFov;
                    capturingClip = false;
                }
            }
        }
    }

    public static void LateTick()
    {
        if (!Reserved || !clockReady || current == null || view == null)
            return;
        try
        {
            double now = Now;
            seconds = clock.At(now);
            playing = clock.Playing(now);
            state = playing ? 2 : 3;
            Apply();
            if (audio != null)
            {
                if (now < audioPreviewDeadline)
                    audio.Tick(audioPreviewAt + (now - audioPreviewClock) * audioPreviewRate,
                               audioPreviewRate, true, audioPreviewVolume);
                else
                    audio.Tick(seconds, speed, playing && (options & Replay3DOptions.Audio) != 0,
                               ((options >> 24) & 127) / 100f);
            }
        }
        catch (Exception e)
        {
            Fail("3D replay: " + e.Message);
        }
    }

    private static int FrameAt(double at)
    {
        var f = current.Motion.Frames;
        int lo = 0, hi = f.Count;
        while (lo < hi)
        {
            int m = lo + (hi - lo) / 2;
            if (f[m].Seconds <= at)
                lo = m + 1;
            else
                hi = m;
        }
        return lo - 1;
    }

    private static void SetObserver(bool value, bool force = false)
    {
        if (observer == value && !force)
            return;
        observer = value;
        if (view != null && current != null && observer)
        {
            int index = Math.Max(0, FrameAt(seconds));
            var f = current.Motion.Frames[index];
            view.transform.position = f.Camera - f.CameraRotation * Vector3.forward * 5 + Vector3.up * 3;
            view.transform.rotation = f.CameraRotation;
        }
    }

    private static void Apply()
    {
        int i = FrameAt(seconds);
        var frames = current.Motion.Frames;
        if (i < 0 || seconds - frames[i].Seconds > .25)
        {
            message = "Camera gap / seek to a recorded frame";
            Visible(playerMarker, false);
            analysis.Options = 0;
            return;
        }
        message = sceneStatus;
        var f = frames[i];
        var position = f.Camera;
        var rotation = f.CameraRotation;
        var body = f.Body;
        float fov = f.Fov;
        if (i + 1 < frames.Count)
        {
            var next = frames[i + 1];
            double dt = next.Seconds - f.Seconds;
            if (dt > 0 && dt <= .25 && (next.Body - f.Body).magnitude <= Math.Max(4, dt * 40))
            {
                float u = Mathf.Clamp01((float)((seconds - f.Seconds) / dt));
                position = Vector3.Lerp(position, next.Camera, u);
                rotation = Quaternion.Slerp(rotation, next.CameraRotation, u);
                body = Vector3.Lerp(body, next.Body, u);
                f.BodyRotation = Quaternion.Slerp(f.BodyRotation, next.BodyRotation, u);
                f.LampPosition = Vector3.Lerp(f.LampPosition, next.LampPosition, u);
                f.LampRotation = Quaternion.Slerp(f.LampRotation, next.LampRotation, u);
                fov = Mathf.Lerp(fov, next.Fov, u);
            }
        }
        if (!observer && (options & Replay3DOptions.Follow) == 0)
        {
            view.transform.SetPositionAndRotation(position, rotation);
            view.fieldOfView = fov;
        }
        else if (!observer)
        {
            Follow(position, rotation);
            view.fieldOfView = 65;
        }
        else if (!capturingClip && MenuCursor.GameHotkeysAllowed)
        {
            var t = view.transform;
            float travel = 10 * Time.unscaledDeltaTime * (Input.GetKey(KeyCode.LeftShift) ? 3 : 1);
            Vector3 direction = Vector3.zero;
            if (Input.GetKey(KeyCode.W))
                direction += t.forward;
            if (Input.GetKey(KeyCode.S))
                direction -= t.forward;
            if (Input.GetKey(KeyCode.A))
                direction -= t.right;
            if (Input.GetKey(KeyCode.D))
                direction += t.right;
            if (Input.GetKey(KeyCode.Q))
                direction -= t.up;
            if (Input.GetKey(KeyCode.E))
                direction += t.up;
            t.position += Vector3.ClampMagnitude(direction, 1) * travel;
            if (Input.GetMouseButton(1))
            {
                t.Rotate(Vector3.up, Input.GetAxis("Mouse X") * 3, Space.World);
                t.Rotate(Vector3.right, -Input.GetAxis("Mouse Y") * 3, Space.Self);
            }
        }
        visibility.Update(Now, Time.unscaledDeltaTime, view.transform.position, position, options,
                          capturingClip);
        if (visibilityRevision != visibility.Revision)
        {
            visibilityRevision = visibility.Revision;
            sceneStatus = sceneMessage + " / " + visibility.Status;
        }
        message = sceneStatus;
        analysis.Seconds = seconds;
        analysis.Options = options;
        analysis.TrailSeconds = trailSeconds;
        Visible(playerMarker, observer || (options & Replay3DOptions.Follow) != 0);
        playerMarker.transform.position = body;
        foreach (var marker in monsters)
        {
            var trace = marker.Track.Trace;
            int lo = 0, hi = trace.Count;
            while (lo < hi)
            {
                int m = lo + (hi - lo) / 2;
                if (trace[m].Seconds <= seconds)
                    lo = m + 1;
                else
                    hi = m;
            }
            int p = lo - 1;
            bool connected = p >= 0 && p + 1 < trace.Count && trace[p + 1].Continuous;
            bool visible = p >= 0 && (connected || seconds - trace[p].Seconds <= .5);
            Visible(marker.Object, visible);
            if (!visible)
                continue;
            var point = trace[p].Position;
            if (connected && trace[p + 1].Seconds > trace[p].Seconds)
                point = Vector3.Lerp(
                    point, trace[p + 1].Position,
                    (float)((seconds - trace[p].Seconds) / (trace[p + 1].Seconds - trace[p].Seconds)));
            marker.Object.transform.position = point;
        }
        for (int k = 0; k < crystals.Count; k++)
            Visible(crystals[k], !nativeStyle && crystalTimes[k] > seconds);
        if (native != null)
        {
            var visual = f;
            visual.Body = body;
            visual.Camera = position;
            visual.CameraRotation = rotation;
            native.Apply(seconds, visual, view, !capturingClip || (options & 8192u) != 0);
            foreach (var marker in monsters)
                Visible(marker.Object, false);
            Visible(playerMarker, false);
        }
        if (scenePlayback != null)
        {
            scenePlayback.Apply(seconds, !observer && (options & Replay3DOptions.Follow) == 0);
            if (scenePlayback.Missing > 0)
                message += " / missing scene assets or bindings: " + scenePlayback.Missing;
        }
        if (replayBrightness != null)
            replayBrightness.SetActive((options & Replay3DOptions.Fullbright) != 0);
    }

    private static void Follow(Vector3 focus, Quaternion rotation)
    {
        followOrbit.Ensure(rotation);
        if (!capturingClip && MenuCursor.GameHotkeysAllowed)
        {
            if (Input.GetKeyDown(KeyCode.Backspace))
                followOrbit.Align(rotation);
            if (Input.GetMouseButton(1))
                followOrbit.Rotate(Input.GetAxis("Mouse X"), Input.GetAxis("Mouse Y"));
            followOrbit.Zoom(Input.mouseScrollDelta.y);
        }
        Vector3 target;
        Quaternion look;
        followOrbit.PoseFocus(focus, rotation, out target, out look);
        view.transform.SetPositionAndRotation(target, look);
    }

    private static void Visible(GameObject obj, bool visible)
    {
        if (obj.activeSelf != visible)
            obj.SetActive(visible);
    }

    private static void Fail(string error)
    {
        Stop();
        message = error;
        state = 4;
    }

    public static void Stop()
    {
        bool wasReserved = Reserved;
        lock (loadGate)
        {
            Interlocked.Increment(ref generation);
            Interlocked.Exchange(ref pending, null);
        }
        state = 0;
        changingScene = false;
        current = null;
        renderers = null;
        clockReady = false;
        followOrbit.Reset();
        try
        {
            if (view != null)
                view.enabled = false;
            if (root != null)
                root.SetActive(false);
        }
        finally
        {
            if (analysis != null)
            {
                analysis.enabled = false;
                UnityEngine.Object.Destroy(analysis);
            }
            if (visibility != null)
            {
                try
                {
                    visibility.Dispose();
                }
                catch (Exception e)
                {
                    MelonLoader.MelonLogger.Warning("Replay visibility restore: " + e.Message);
                }
                finally
                {
                    visibility = null;
                }
            }
            try
            {
                if (restore != null)
                {
                    restore.Dispose();
                    if (restore.Error != null)
                        MelonLoader.MelonLogger.Warning("Replay restore: " + restore.Error);
                    restore = null;
                }
            }
            finally
            {
                ReplayGamePause.Frozen = false;
                if (root != null)
                    UnityEngine.Object.Destroy(root);
                if (palette != null)
                    foreach (var material in palette)
                        if (material != null)
                            UnityEngine.Object.Destroy(material);
                if (transparentPalette != null)
                    foreach (var material in transparentPalette)
                        if (material != null)
                            UnityEngine.Object.Destroy(material);
                if (hiddenMaterial != null)
                    UnityEngine.Object.Destroy(hiddenMaterial);
                transparentPalette = null;
                hiddenMaterial = null;
                if (playerMaterial != null)
                    UnityEngine.Object.Destroy(playerMaterial);
                if (monsterMaterial != null)
                    UnityEngine.Object.Destroy(monsterMaterial);
                if (crystalMaterial != null)
                    UnityEngine.Object.Destroy(crystalMaterial);
                root = null;
                view = null;
                analysis = null;
                native = null;
                scenePlayback = null;
                audio = null;
                audioPreviewDeadline = 0;
                replayBrightness = null;
                originalLights.Clear();
                playerMarker = null;
                monsters.Clear();
                crystals.Clear();
                crystalTimes = null;
                palette = null;
                playerMaterial = monsterMaterial = crystalMaterial = null;
                player = null;
                seconds = 0;
                message = sceneMessage = sceneStatus = "";
                if (wasReserved)
                    swallowFrame = Time.frameCount;
            }
        }
    }

    public static void WriteSnapshot(SnapshotBuffer data)
    {
        data.Write(SharedLayout.Replay3dState, state);
        data.Write(SharedLayout.Replay3dRunId, current == null ? 0 : current.Map.Review.Id);
        data.Write(SharedLayout.Replay3dSeconds, seconds);
        data.WriteString(SharedLayout.Replay3dMessage,
                         message + (current != null && current.AudioError != null
                                        ? " / Audio: " + current.AudioError
                                    : audio != null && audio.HasProblem ? " / " + audio.Status
                                                                        : ""),
                         160);
        data.Write(SharedLayout.Replay3dOptions, (int)options);
        data.Write(SharedLayout.Replay3dSpeed, speed);
        data.Write(SharedLayout.Replay3dLoopA, loopA);
        data.Write(SharedLayout.Replay3dLoopB, loopB);
        data.Write(SharedLayout.Replay3dFollowDistance, followOrbit.Radius);
        data.Write(SharedLayout.Replay3dFollowHeight, 0f);
        data.Write(SharedLayout.Replay3dTrailSeconds, trailSeconds);
        data.Write(SharedLayout.Replay3dColliderCount, analysis == null ? 0 : analysis.VisibleColliders);
        data.Write(SharedLayout.Replay3dColliderFlags, current == null || !current.Motion.Colliders.Recorded
                                                           ? 8
                                                           : (int)current.Motion.Colliders.Flags);
    }
}
