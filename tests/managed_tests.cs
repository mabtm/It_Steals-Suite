using System;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Reflection.Emit;
using System.Runtime.InteropServices;
using System.Diagnostics;
using System.Collections.Generic;
using HarmonyLib;

public static class ManagedTests
{
    private static void Check(bool condition, string name)
    {
        if (!condition)
            throw new Exception(name);
    }

    public static int Main(string[] args)
    {
        string directory = Path.Combine(args[0], "run-store-test");
        Directory.CreateDirectory(directory);
        // Start this repeatable test with a fresh database, leaving actual user data alone.
        foreach (string file in Directory.GetFiles(directory))
            File.Delete(file);
        var store = new RunStore(directory);
        Check(store.RecordBest(3, 45), "first Classic PB");
        Check(store.RecordBest(8, 90), "independent Ghost PB");
        Check(!store.RecordBest(3, 46), "slower Classic run rejected");
        Check(!store.RecordBest(8, double.NaN), "NaN rejected");
        Check(!store.RecordBest(8, -1), "negative time rejected");
        store.Runs = 2;
        store.Finished = 2;
        store.TotalFinishedTime = 135;
        CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("de-DE");
        store.Save();
        CultureInfo.CurrentCulture = CultureInfo.GetCultureInfo("en-US");
        var loaded = new RunStore(directory);
        Check(loaded.Best(3) == 45 && loaded.Best(8) == 90 && loaded.TotalFinishedTime == 135,
              "locale-independent reload");
        loaded.RecordBest(3, 12.345678);
        loaded.Save();
        Check(new RunStore(directory).Best(3) == 12.345678, "roundtrip precision and atomic replace");
        string legacy = Path.Combine(args[0], "legacy-stats-test");
        Directory.CreateDirectory(legacy);
        File.WriteAllLines(Path.Combine(legacy, "stats.txt"), new[] { "10", "6", "4", "123,456" });
        string migrated = Path.Combine(legacy, "new-v3");
        Directory.CreateDirectory(migrated);
        foreach (string file in Directory.GetFiles(migrated))
            File.Delete(file);
        var imported = new RunStore(migrated, legacy);
        Check(imported.Runs == 10 && imported.Finished == 6 && imported.Deaths == 4 &&
                  imported.TotalFinishedTime == 123.456,
              "legacy stats migration");
        Check(SpeedrunManager.LevelIndices.Length == 8 && SpeedrunManager.LevelIndices[6] == 12 &&
                  SpeedrunManager.LevelIndices[7] == 13,
              "Outside is appended without changing existing level indices");
        var autoLoad = new SearchLoadController(true, 8, 16.667, 0);
        double start = autoLoad.Milliseconds;
        for (int i = 1; i <= 120; i++)
            autoLoad.Observe(16.667, start, true, i / 60.0);
        Check(autoLoad.Milliseconds > start && autoLoad.Milliseconds <= 3.334,
              "stable frames increase cooperative load within ceiling");
        double raised = autoLoad.Milliseconds;
        for (int i = 121; i <= 180; i++)
            autoLoad.Observe(30, raised, true, i / 60.0);
        Check(autoLoad.Milliseconds < raised, "slow frames reduce load");
        autoLoad.Observe(150, 1, true, 4);
        Check(autoLoad.Milliseconds == .25, "loading stall reduces work");
        var manualLoad = new SearchLoadController(false, 3.5, 16, 0);
        manualLoad.Observe(200, 9, false, 3);
        Check(manualLoad.Milliseconds == 3.5, "manual limit remains stable");
        Check(new SearchLoadController(false, double.NaN, 16, 0).Milliseconds == 2,
              "invalid manual limit safe default");
        var catalogType = typeof(SeedMapPlanner).GetNestedType("Catalog", BindingFlags.NonPublic);
        object waitingCatalog = Activator.CreateInstance(catalogType, true);
        var waitingData = new PracticeMapData { Flags = 1 | 128, NavigationStatus = 1, NavigationModel = 1 };
        catalogType.GetField("Data").SetValue(waitingCatalog, waitingData);
        typeof(SeedMapPlanner)
            .GetMethod("CancelNavigation", BindingFlags.Static | BindingFlags.NonPublic)
            .Invoke(null, new[] { waitingCatalog });
        Check(
            waitingData.NavigationStatus == 0 && waitingData.NavigationModel == 0 &&
                (waitingData.Flags & 128) == 0,
            "scene change/failure cancels deferred navigation without a stuck pending flag or player provenance");
        var ackLimited = new SearchLoadController(true, 2, 16.667, 0);
        for (int i = 1; i <= 300; i++)
            ackLimited.Observe(16.667, .25, i % 3 == 0, i / 60.0);
        Check(ackLimited.Milliseconds > 1, "normal ACK pacing cannot push automatic load to floor");
        var paced = new SearchLoadController(true, 2, 14, 0);
        for (int i = 1; i <= 1200; i++)
            paced.Observe(i < 120 ? 14 : 16.667, .25, true, i / 60.0);
        Check(paced.Milliseconds > .5, "external pacing change below thirty percent recovers from floor");
        var corridors = new NavigationGraphBuilder();
        int c0 = corridors.AddNode(new UnityEngine.Vector3(0, 0, 0)),
            c1 = corridors.AddNode(new UnityEngine.Vector3(1, 0, 0));
        Check(corridors.AddNode(new UnityEngine.Vector3(0, 0, 0)) == c0,
              "Unity corridor nodes deduplicate equal three-dimensional coordinates");
        int queries = 0;
        Func<UnityEngine.Vector3, UnityEngine.Vector3, bool> edge = (a, b) =>
        {
            queries++;
            return true;
        };
        corridors.AddEdge(c0, c1, edge);
        corridors.AddEdge(c1, c0, edge);
        Check(queries == 1 && corridors.Edges().Length == 2,
              "corridor edges validated once and deduplicated");
        int upper = corridors.AddNode(new UnityEngine.Vector3(0, 10, 0));
        Check(upper != c0, "different floors stay separate in native corridor capture");
        int rejected = 0;
        Func<UnityEngine.Vector3, UnityEngine.Vector3, bool> blocked = (a, b) =>
        {
            rejected++;
            return false;
        };
        corridors.AddEdge(c0, upper, blocked);
        corridors.AddEdge(upper, c0, blocked);
        Check(rejected == 1, "rejected segment cache is bounded and avoids repeated native probes");
        Check(corridors.Edges().Length == 2, "blocked navigation segment is never invented");
        var completeCorridor = new NavigationGraphBuilder();
        int startNode = completeCorridor.AddNode(new UnityEngine.Vector3(0, 0, 0)),
            endNode = completeCorridor.AddNode(new UnityEngine.Vector3(10, 0, 0));
        var returnedCorners = new[] { new UnityEngine.Vector3(.005f, 0, 0), new UnityEngine.Vector3(5, 0, 2),
                                      new UnityEngine.Vector3(9.995f, 0, 0) };
        completeCorridor.AddCompletePath(startNode, endNode, returnedCorners, 3);
        completeCorridor.AddCompletePath(startNode, endNode, returnedCorners, 3);
        Check(
            completeCorridor.Nodes().Length == 3 && completeCorridor.Edges().Length == 4,
            "complete native path binds slightly displaced endpoints without creating isolated anchors or duplicate edges");
        bool wrongEndpoints = false;
        try
        {
            completeCorridor.AddCompletePath(
                startNode, endNode,
                new[] { new UnityEngine.Vector3(1, 0, 0), new UnityEngine.Vector3(10, 0, 0) }, 2);
        }
        catch (InvalidOperationException)
        {
            wrongEndpoints = true;
        }
        Check(wrongEndpoints, "complete-path insertion rejects unrelated endpoints");
        CheckPlayerWallRouting(args[0]);
        Check(Marshal.SizeOf(typeof(TrainerCommands)) == 192, "command size");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "ReplayMinSeconds").ToInt32() == 128 &&
                  Marshal.OffsetOf(typeof(TrainerCommands), "ReplayMinCrystals").ToInt32() == 132,
              "v16 retention command offsets");
        var rule = new ReplayRetention(10, 0);
        Check(!rule.Accept(10, 99) && rule.Accept(10.001, 0) && !rule.Accept(double.NaN, 9),
              "time policy uses strictly longer and rejects nonfinite duration");
        var crystalRule = new ReplayRetention(10, 3);
        Check(!crystalRule.Accept(100, 2) && crystalRule.Accept(1, 3),
              "crystal policy uses actual pickup count");
        Check(!new ReplayRetention(double.NaN, 0).Accept(10, 0) && new ReplayRetention(0, 0).Accept(.01, 0),
              "invalid policy fallback and zero threshold");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "PracticeSeed").ToInt32() == 20, "seed offset");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "TeleportX").ToInt32() == 28, "teleport offset");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "SeededPractice").ToInt32() == 40,
              "seed mode offset");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "TelemetryOptions").ToInt32() == 44,
              "optional telemetry offset");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "WindowModeCmd").ToInt32() == 48,
              "window mode command offset");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "InputCapture").ToInt32() == 52,
              "input ownership offset");
        Check(typeof(Monster3).GetField("WallMonsters") != null &&
                  typeof(Monster3).GetField("cieling") != null && typeof(WallAI).GetField("angry") != null &&
                  typeof(WallAI).GetField("isSpotted") != null,
              "Timer actor and ceiling fields exist in actual Game DLL");
        var geometryOnly = new TrainerCommands { TelemetryOptions = 256 };
        Check(geometryOnly.RequestsPlannerGeometry && !geometryOnly.RequestsPlanner,
              "minimap geometry is distinct from seed research");
        geometryOnly.SpeedrunModeActive = 1;
        Check(!geometryOnly.RequestsPlannerGeometry, "Clean mode blocks geometry export");
        var commands = new TrainerCommands();
        Check(commands.RequestsMap && commands.RequestsPath && commands.RequestsDetails,
              "v3 compatibility requests full export");
        commands.TelemetryOptions = 0x80000000u;
        Check(!commands.RequestsMap && !commands.RequestsPath && !commands.RequestsDetails,
              "hidden trainer skips expensive export");
        commands.TelemetryOptions |= 5u;
        Check(commands.RequestsMap && commands.RequestsDetails && !commands.RequestsPath,
              "map does not implicitly request navigation paths");
        var buffer = new SnapshotBuffer();
        buffer.Write(0, 1.25f);
        buffer.Write(5472, 123.456789);
        buffer.WriteString(4500, new string('x', 100), 64);
        Check(BitConverter.ToSingle(buffer.Bytes, 0) == 1.25f &&
                  BitConverter.ToDouble(buffer.Bytes, 5472) == 123.456789,
              "float/double packing");
        Check(buffer.Bytes[4563] == 0, "string termination");
        buffer.WriteString(4500, "ä漢🙂", 6);
        Check(new System.Text.UTF8Encoding(false, true).GetString(buffer.Bytes, 4500, 5) == "ä漢" &&
                  buffer.Bytes[4505] == 0,
              "UTF-8 truncation keeps complete multibyte characters");
        buffer.Clear();
        Check(BitConverter.ToDouble(buffer.Bytes, 5472) == 0, "clear stale fields");
        Check(PracticeRandom.DeriveSeed(123, 3, 17, 0) == PracticeRandom.DeriveSeed(123, 3, 17, 0),
              "same seed sequence");
        Check(PracticeRandom.DeriveSeed(123, 3, 17, 0) != PracticeRandom.DeriveSeed(123, 3, 17, 1),
              "per-site reseed sequence");
        Check(PracticeRandom.DeriveSeed(123, 3, 17, 0) != PracticeRandom.DeriveSeed(123, 8, 17, 0),
              "scene-specific sequence");
        // Inspect actual managed methods, including coroutine state machines.
        int reseeds = 0, methods = 0, timeSources = 0, cursorWrites = 0;
        MethodInfo counter =
            typeof(HarmonyPatches).GetMethod("CountReseeds", BindingFlags.Static | BindingFlags.NonPublic);
        MethodInfo sources =
            typeof(HarmonyPatches)
                .GetMethod("CountGameSeedSources", BindingFlags.Static | BindingFlags.NonPublic);
        MethodInfo cursors =
            typeof(HarmonyPatches)
                .GetMethod("CountCursorWrites", BindingFlags.Static | BindingFlags.NonPublic);
        foreach (Type type in typeof(MouseLook).Assembly.GetTypes())
            foreach (MethodInfo method in type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic |
                                                          BindingFlags.Instance | BindingFlags.Static |
                                                          BindingFlags.DeclaredOnly))
            {
                int count = (int)counter.Invoke(null, new object[] { method });
                cursorWrites += (int)cursors.Invoke(null, new object[] { method });
                if (count > 0)
                {
                    reseeds += count;
                    methods++;
                    timeSources += (int)sources.Invoke(null, new object[] { method });
                }
            }
        Check(reseeds == 22, "all actual game reseeds included");
        Check(timeSources == 22,
              "all 22 stock seed sources are verified (TickCount, milliseconds, position)");
        Check(cursorWrites >= 8, "managed cursor writes are discoverable in the supplied game");
        CheckGameControls();
        var culling =
            Array.Find(typeof(HideAndSeekIt).GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic),
                       t => t.Name.StartsWith("<OccludeWalls>"));
        Check(culling != null, "actual Hide & Seek has the wall-culling coroutine");
        var cullInstructions = MethodInstructions(culling.GetMethod(
            "MoveNext", BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public));
        var cullPatch = typeof(HarmonyPatches)
                            .GetMethod("FreecamWallVisibility", BindingFlags.NonPublic | BindingFlags.Static);
        int adjusted = 0, oldCalls = 0, newCalls = 0;
        foreach (var instruction in cullInstructions)
            if (IsCall(instruction, typeof(UnityEngine.Transform), "get_position"))
                oldCalls++;
        foreach (var instruction in (IEnumerable<CodeInstruction>)cullPatch.Invoke(
                     null, new object[] { cullInstructions }))
        {
            if (IsCall(instruction, typeof(FreeCam), "VisibilityOrigin"))
                adjusted++;
            if (IsCall(instruction, typeof(UnityEngine.Transform), "get_position"))
                newCalls++;
        }
        Check(
            adjusted == 1 && oldCalls == newCalls,
            "freecam culling wraps exactly the player origin; wall/enemy coordinates and native position calls remain intact");
        Console.WriteLine(
            "Actual Hide & Seek wall-culling coroutine: one camera-origin wrapper, unchanged wall/enemy calls: PASS");
        var origin = new UnityEngine.Vector3(2, 3, 4);
        Check(FreeCam.VisibilityOrigin(origin).x == 2 && FreeCam.VisibilityOrigin(origin).z == 4,
              "inactive freecam keeps the game's original culling origin");
        // Inspect the compiled SDK-facing camera setup: the spectator may
        // contain camera rendering only, never clone actor collision/scripts.
        var setup = MethodInstructions(
            typeof(FreeCam).GetMethod("Enable", BindingFlags.Instance | BindingFlags.NonPublic));
        bool cameraCopy = false;
        int components = 0;
        foreach (var instruction in setup)
        {
            Check(!IsCall(instruction, typeof(UnityEngine.Transform), "SetParent") &&
                      !IsCall(instruction, typeof(UnityEngine.Object), "Instantiate"),
                  "freecam cannot detach or clone a player-bearing camera object");
            if (IsCall(instruction, typeof(UnityEngine.Camera), "CopyFrom"))
                cameraCopy = true;
            var call = instruction.operand as MethodInfo;
            if (call != null && call.Name == "AddComponent" && call.IsGenericMethod)
            {
                var component = call.GetGenericArguments()[0];
                Check(component == typeof(UnityEngine.Camera) || component == typeof(HitboxDrawer),
                      "spectator has no colliders or gameplay behaviours");
                components++;
            }
        }
        Check(cameraCopy && components == 2,
              "spectator copies view settings and has exactly Camera and hitbox rendering");
        Console.WriteLine(
            "SDK-facing spectator camera isolation (no actor clone/detach/collider components): PASS");
        CheckMenuAbort(args[0]);
        CheckSeedMaps(args[0]);
        CheckPortable(args[0]);
        CheckTracking(args[0]);
        CheckStatisticsReset(args[0]);
        CheckRunReviewsAndValidity(args[0]);
        Console.WriteLine(
            "Managed persistence, run transitions, seed evidence, crash recovery and DLL scan: PASS (" +
            reseeds + " verified reseeds in " + methods + " methods)");
        return 0;
    }

    private static void CheckRunReviewsAndValidity(string root)
    {
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "FrameLimitMode").ToInt32() == 100 &&
                  Marshal.OffsetOf(typeof(TrainerCommands), "GameFrameLimit").ToInt32() == 108 &&
                  Marshal.OffsetOf(typeof(TrainerCommands), "RunReviewId").ToInt32() == 116 &&
                  Marshal.OffsetOf(typeof(TrainerCommands), "RunHudVisible").ToInt32() == 124,
              "v15 commands align across C# and C++");
        Check(typeof(MouseLook).GetField("mainCamera").FieldType == typeof(UnityEngine.Camera) &&
                  typeof(MouseLook).GetField("UICamera").FieldType == typeof(UnityEngine.Camera) &&
                  typeof(MouseLook).GetField("controller").FieldType ==
                      typeof(UnityEngine.CharacterController),
              "actual game exposes a player camera separately from UI and controller references");
        var matrix = UnityEngine.Matrix4x4.identity;
        matrix.m00 = matrix.m22 = 0;
        matrix.m02 = -1;
        matrix.m20 = 1;
        matrix.m11 = 2;
        matrix.m03 = 10;
        matrix.m23 = 20;
        var outline = SceneGeometry.ProjectBox(matrix, new UnityEngine.Vector3(1, 1, 3),
                                               new UnityEngine.Vector3(4, 2, 1));
        Check(
            !outline.Approximate && outline.Corners[0].x == 7.5f && outline.Corners[0].z == 19 &&
                outline.Corners[1].z == 23 && outline.Corners[2].x == 6.5f,
            "rotated/scaled wall retains its contour instead of being replaced by a larger axis-aligned box");
        var bounds =
            SceneGeometry.WorldBounds(matrix, new UnityEngine.Bounds(new UnityEngine.Vector3(1, 1, 3),
                                                                     new UnityEngine.Vector3(4, 2, 1)));
        Check(bounds.min.x == 6.5f && bounds.max.x == 7.5f && bounds.min.z == 19 && bounds.max.z == 23 &&
                  bounds.min.y == 0 && bounds.max.y == 4,
              "all transformed collider corners contribute to bounds");
        Check(!SceneGeometry.IntersectsPlayerHeight(
                  new UnityEngine.Bounds(new UnityEngine.Vector3(0, -.1f, 0),
                                         new UnityEngine.Vector3(100, .2f, 100)),
                  0, 1.8f) &&
                  !SceneGeometry.IntersectsPlayerHeight(
                      new UnityEngine.Bounds(new UnityEngine.Vector3(0, 3, 0),
                                             new UnityEngine.Vector3(100, 1, 100)),
                      0, 1.8f) &&
                  SceneGeometry.IntersectsPlayerHeight(bounds, 0, 1.8f),
              "floor and ceiling slabs do not cover the whole 2D map as walls");
        matrix.m10 = .2f;
        Check(SceneGeometry.ProjectBox(matrix, UnityEngine.Vector3.zero, new UnityEngine.Vector3(4, 2, 1))
                  .Approximate,
              "tilted boxes are explicitly marked conservative");
        foreach (string name in new[] { "cheatengine-x86_64", "Cheat Engine", "artmoney", "WeMod", "x64dbg",
                                        "reclass.net", "squalr" })
            Check(ExternalToolMonitor.SuspiciousName(name), "known external editor name: " + name);
        foreach (string name in new[] { "steam", "It Steals", "explorer", "obs64", "notepad", "" })
            Check(!ExternalToolMonitor.SuspiciousName(name),
                  "ordinary game/recording process stays clear: " + name);
        var valid = new RunRecord { ObservedFromStart = true, SeedKnown = true, SeedType = SeedKind.Random,
                                    ValidationChecked = true };
        Check(RunValidity.Eligible(valid) && VerificationReport.Integrity(valid) == "VALID (LOCAL)",
              "completed clean local audit can be valid");
        foreach (RunAids aid in new[] { RunAids.Practice, RunAids.FixedSeed, RunAids.Hitboxes,
                                        RunAids.Fullbright, RunAids.FreeCamera, RunAids.Teleport,
                                        RunAids.Minimap, RunAids.Path, RunAids.SpawnPlanner,
                                        RunAids.LiveInspector })
        {
            var r = valid.Copy();
            r.Aids = aid;
            Check(!RunValidity.Eligible(r), "each trainer aid invalidates the record: " + aid);
        }
        foreach (RunWarnings flag in Enum.GetValues(typeof(RunWarnings)))
        {
            if (flag == RunWarnings.None || flag == RunWarnings.EventLogTruncated)
                continue;
            var r = valid.Copy();
            r.Warnings = flag;
            Check(!RunValidity.Eligible(r), "each integrity failure stays invalid: " + flag);
        }
        var old = valid.Copy();
        old.ValidationChecked = false;
        Check(!RunValidity.Eligible(old), "older un-audited records cannot silently become VALID");
        string folder = Path.Combine(root, "new-validation-test");
        Directory.CreateDirectory(folder);
        foreach (string f in Directory.GetFiles(folder))
            File.Delete(f);
        var manager = new SpeedrunManager(folder);
        var tracker = (RunTracker) typeof(SpeedrunManager)
                          .GetField("tracker", BindingFlags.Instance | BindingFlags.NonPublic)
                          .GetValue(manager);
        var seed = new SeedEvidence();
        seed.Reset(true);
        seed.Observe(42, 42, false, 0);
        long now = DateTime.UtcNow.Ticks;
        tracker.Begin(3, now, RunAids.None, seed);
        tracker.Active.ValidationChecked = true;
        HitboxDrawer.Enabled = true;
        HitboxDrawer.Enabled = false;
        Check(tracker.Active.Aids == RunAids.Hitboxes,
              "feature activation is sticky even if it is disabled before the next telemetry sample");
        tracker.End(RunOutcome.Completed, now + 100000000, 10, 1);
        Check(!tracker.Last.PersonalBest, "briefly enabled aids cannot earn a clean PB");
        tracker.Begin(3, now + 1, RunAids.None, seed);
        tracker.Active.ValidationChecked = true;
        SetManagerField(manager, "active", true);
        SetManagerField(manager, "practice", false);
        SetManagerField(manager, "timerState", (byte)1);
        manager.AuditBeforeSuppression(new TrainerCommands { SpeedrunModeActive = 1, PlannerRequest = 4 });
        Check(tracker.Active.Warnings == RunWarnings.None,
              "stopping background search is allowed in clean mode");
        manager.AuditBeforeSuppression(new TrainerCommands { SpeedrunModeActive = 1, RunReviewRequest = 1 });
        Check(tracker.Active.Warnings == RunWarnings.None,
              "a denied late replay hotkey cannot invalidate a new run");
        manager.AuditBeforeSuppression(new TrainerCommands { SpeedrunModeActive = 1, HitboxEnabled = 1 });
        Check((tracker.Active.Warnings & RunWarnings.ForbiddenCommand) != 0,
              "forbidden command is recorded before suppression");
        tracker.End(RunOutcome.Completed, now + 200000000, 9, 1);
        SetManagerField(manager, "timerState", (byte)0);
        manager.Dispose();
        var reload = new RunStore(folder);
        Check(reload.History[0].ValidationChecked && !RunValidity.Eligible(reload.History[0]) &&
                  (reload.History[1].Warnings & RunWarnings.ForbiddenCommand) != 0,
              "validation and reasons survive persistence");
        reload.Dispose();
        var journal = File.ReadAllText(Path.Combine(folder, "speedruns-v3.txt"));
        Check(journal.StartsWith("version=7\n"), "new audit journal has a protected format version");
        // Exercise the old 25-field record importer directly; its audit flag
        // is absent and therefore false, even when every legacy field is clean.
        var pack = typeof(RunStore).GetMethod("Pack", BindingFlags.Static | BindingFlags.NonPublic);
        var unpack = typeof(RunStore).GetMethod("Unpack", BindingFlags.Static | BindingFlags.NonPublic);
        var legacyFields = ((string)pack.Invoke(null, new object[] { valid, false })).Split('|');
        string legacy = string.Join("|", legacyFields, 0, 25);
        // A valid identity is required for any recovered record.
        valid.Id = valid.StartedUtc = now;
        valid.Scene = 3;
        legacyFields = ((string)pack.Invoke(null, new object[] { valid, false })).Split('|');
        legacy = string.Join("|", legacyFields, 0, 25);
        var legacyRecord = (RunRecord)unpack.Invoke(null, new object[] { legacy });
        Check(legacyRecord != null && !legacyRecord.ValidationChecked && !RunValidity.Eligible(legacyRecord),
              "legacy journal migration preserves conservative validity");
        var sample = new RunReviewData { Id = now, Outcome = 3, Seconds = .5 };
        var positions = new[] { new UnityEngine.Vector3(0, 1, 0), new UnityEngine.Vector3(4, 1, 0),
                                new UnityEngine.Vector3(4, 1, 4), new UnityEngine.Vector3(0, 1, 4),
                                new UnityEngine.Vector3(2, 1, 4), new UnityEngine.Vector3(6, 1, 4) };
        for (int i = 0; i < positions.Length; i++)
            Check(sample.Sample(i * .1, positions[i]), "position sample accepted in time order");
        sample.Collected.AddRange(new[] { 1, 0 });
        sample.PickupSeconds.AddRange(new[] { .3, .4 });
        var packet =
            new PracticeMapData { Pid = 6789,         Scene = 5,          Epoch = 1,       Seed = -674714359,
                                  Recipe = 1,         SourceKind = 1,     Spawn = 0,       LiveSpawn = 0,
                                  Keep = 3,           Flags = 1,          Stage = 1,       Review = sample,
                                  AgentRadius = .35f, AgentHeight = 1.8f, AgentStep = .3f, AgentSlope = 45 };
        packet.Spawns = new[] { positions[0] };
        packet.Points = new[] { positions[4], positions[3], new UnityEngine.Vector3(12, 1, 4) };
        packet.Selected = packet.LiveSelected = new[] { 0, 1, 2 };
        packet.Vertices = new[] { new UnityEngine.Vector3(0, 0, 0), new UnityEngine.Vector3(20, 0, 0),
                                  new UnityEngine.Vector3(20, 0, 20), new UnityEngine.Vector3(0, 0, 20) };
        packet.Indices = new[] { 0, 1, 2, 0, 2, 3 };
        packet.WallOutlines = packet.Obstacles = new[] { outline };
        packet.Walls = new[] { UnityEngine.Rect.MinMaxRect(6.5f, 19, 7.5f, 23) };
        packet.MovementTick = .02f;
        packet.MovementSprintStep = .14f;
        sample.LightSources = 255;
        sample.LightRejected = 2;
        sample.ValidationChecked = sample.Valid = sample.ObservedFromStart = sample.SeedKnown = true;
        sample.SeedKind = 1;
        sample.ValidityReason = "Local checks passed";
        var monster = new ReplayMonster { Id = 1, Kind = 2 };
        monster.Trace.Add(new RunPose { Seconds = 0, Position = positions[0] });
        monster.Trace.Add(new RunPose { Seconds = .5, Position = positions[1] });
        monster.Paths.Add(new ReplayPath { Seconds = .1, Corners = new[] { positions[0], positions[2] } });
        sample.Monsters.Add(monster);
        sample.Flashes.Add(new ReplayFlash { Seconds = .2, Player = positions[4], Kind = 1, Actor = 1 });
        sample.Flashes.Add(new ReplayFlash { Seconds = .4, Player = positions[5], Kind = 2, Actor = 1 });
        byte[] encoded = packet.Encode();
        var decoded = PracticeMapArchive.Decode(encoded);
        Check(decoded.Seed == packet.Seed && decoded.Review.Id == sample.Id &&
                  decoded.Review.Trace.Count == 6 && decoded.Review.Collected[0] == 1 &&
                  decoded.Review.PickupSeconds[1] == .4 && decoded.WallOutlines[0].Corners[1].z == 23,
              "archive preserves the exact layout, trace, wall contour and timed pickup order");
        Check(
            decoded.Review.LightSources == 255 && decoded.Review.LightRejected == 2 &&
                decoded.MovementTick == .02f && decoded.Review.Valid &&
                decoded.Review.Monsters[0].Trace.Count == 2 &&
                decoded.Review.Monsters[0].Paths[0].Corners.Length == 2 &&
                decoded.Review.Flashes[1].Kind == 2,
            "validation, monster positions, planned paths and flashlight/camera events survive v8 archive recovery");
        File.WriteAllBytes(Path.Combine(root, "review-wire.bin"), encoded);
        var outside = packet.Copy();
        outside.Scene = 13;
        var outsideBytes = outside.Encode();
        Check(PracticeMapArchive.Decode(outsideBytes).Scene == 13,
              "Outside archives are accepted and retain their own scene identity");
        File.WriteAllBytes(Path.Combine(root, "outside-review-wire.bin"), outsideBytes);
        var stunType =
            Array.Find(typeof(MonsterController).GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic),
                       t => t.Name.StartsWith("<RayTestStun>"));
        var original = MethodInstructions(
            stunType.GetMethod("MoveNext", BindingFlags.Instance | BindingFlags.NonPublic));
        int setters = 0;
        foreach (var instruction in HarmonyPatches.CaptureCameraStun(original))
            if (IsCall(instruction, typeof(ReplayCapture), "CameraStunned"))
                setters++;
        Check(setters == 2,
              "actual game camera stun and recovery writes are observed without replacing ray tests");
        var timerType =
            Array.Find(typeof(Monster3).GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic),
                       t => t.Name.StartsWith("<RayTest>"));
        int timerWrites = 0;
        foreach (var instruction in HarmonyPatches.CaptureTimerSight(MethodInstructions(
                     timerType.GetMethod("MoveNext", BindingFlags.Instance | BindingFlags.NonPublic))))
            if (IsCall(instruction, typeof(ReplayCapture), "TimerSpotted"))
                timerWrites++;
        Check(
            timerWrites == 4,
            "all four actual Timer wall sight-state writes are observed, including actors seen before periodic discovery");
        var cameraFactory =
            typeof(MouseLook).GetMethod("CameraFlash", BindingFlags.Instance | BindingFlags.NonPublic);
        var hideRay =
            typeof(HideAndSeekIt).GetMethod("RayTest", BindingFlags.Instance | BindingFlags.NonPublic);
        bool hitState = false;
        foreach (var instruction in MethodInstructions(hideRay))
        {
            var field = instruction.operand as FieldInfo;
            if (instruction.opcode == OpCodes.Stfld && field != null && field.Name == "SeeingIt")
                hitState = true;
        }
        Check(
            hitState && cameraFactory != null,
            "Hide & Seek light observations bind to the actual SeeingIt hit writer and camera events to the typed player factory");
        int sprintSteps = 0, walkSteps = 0;
        foreach (var instruction in MethodInstructions(typeof(MouseLook).GetMethod(
                     "FixedUpdate", BindingFlags.Instance | BindingFlags.NonPublic)))
        {
            if (instruction.opcode == OpCodes.Ldc_R4)
            {
                float value = (float)instruction.operand;
                if (Math.Abs(value - .14f) < 1e-6)
                    sprintSteps++;
                if (Math.Abs(value - .05f) < 1e-6)
                    walkSteps++;
            }
        }
        Check(
            sprintSteps == 1 && walkSteps == 1,
            "verified movement uses 0.14 sprint and 0.05 walk units per physics tick, without a movement deltaTime multiplier");
        var signal = new ReplaySignal();
        int rises = 0;
        foreach (bool state in new[] { false, true, true, true, false, true })
            if (signal.Rise(state))
                rises++;
        Check(
            rises == 2,
            "nine ray tests or repeated observations of one held state must not emit duplicate contact episodes");
        var pulses = new RunReviewData();
        Check(
            pulses.LightEvent(.1, positions[0], 1) && pulses.ConfirmLight(0, 3) &&
                pulses.Flashes.Count == 1 && pulses.Flashes[0].Actor == 3 &&
                pulses.Flashes[0].Seconds == .1 && pulses.Flashes[0].Player.x == 0,
            "explicitly associated contact enriches the original pulse without changing its time or position");
        Check(pulses.ConfirmLight(0, 3) && pulses.Flashes.Count == 1,
              "camera sight and stun confirmations of the same shot must be idempotent");
        Check(pulses.LightEvent(.3, positions[1], 1) && pulses.Flashes.Count == 2,
              "separate quick flashlight activations are not lost to contact debounce");
        Check(!pulses.LightEvent(double.NaN, positions[0], 1) && !pulses.LightEvent(.2, positions[0], 1) &&
                  !pulses.LightEvent(.4, positions[0], 3) && !pulses.LightEvent(.4, positions[0], 2, -1),
              "invalid flash events cannot poison a recording");
        var misses = new RunReviewData { Id = now + 2,
                                         Outcome = 2,
                                         Seconds = .5,
                                         ValidationChecked = true,
                                         Valid = true,
                                         ObservedFromStart = true,
                                         SeedKnown = true,
                                         SeedKind = 1,
                                         ValidityReason = "Local checks passed" };
        misses.LightEvent(.2, positions[4], 1);
        misses.LightEvent(.4, positions[5], 2);
        var missPacket = packet.Copy();
        missPacket.Review = misses;
        var missBytes = missPacket.Encode();
        var missDecoded = PracticeMapArchive.Decode(missBytes);
        Check(missDecoded.Review.Flashes.Count == 2 && missDecoded.Review.Flashes[0].Actor == 0 &&
                  missDecoded.Review.Monsters.Count == 0,
              "missed flashlight/camera events survive v9 recovery with no monster actor");
        File.WriteAllBytes(Path.Combine(root, "missed-flash-wire.bin"), missBytes);
        var legacy8 = new byte[encoded.Length - 31];
        Array.Copy(encoded, legacy8, legacy8.Length - 8);
        Array.Copy(BitConverter.GetBytes(8), 0, legacy8, 4, 4);
        Array.Copy(BitConverter.GetBytes(PracticeMapData.Checksum(legacy8, legacy8.Length - 8)), 0, legacy8,
                   legacy8.Length - 8, 8);
        Check(PracticeMapArchive.Decode(legacy8).Review.Flashes.Count == 2,
              "existing v8 flash recordings still recover");
        for (int length = 0; length < encoded.Length; length++)
        {
            var truncated = new byte[length];
            Array.Copy(encoded, truncated, length);
            bool rejected = false;
            try
            {
                PracticeMapArchive.Decode(truncated);
            }
            catch (IOException)
            {
                rejected = true;
            }
            Check(rejected, "truncated recorded run is rejected");
        }
        var oversized = (byte[])encoded.Clone();
        Array.Copy(BitConverter.GetBytes(int.MaxValue), 0, oversized, oversized.Length - 16, 4);
        bool countRejected = false;
        try
        {
            PracticeMapArchive.Decode(oversized);
        }
        catch (InvalidDataException)
        {
            countRejected = true;
        }
        Check(countRejected, "oversized pickup array is rejected before allocation");
        var reduced = new RunReviewData();
        for (int i = 0; i < 65550; i++)
            reduced.Sample(i * .021, new UnityEngine.Vector3(i * .01f, 0, 0));
        Check(reduced.Trace.Count <= 32768 && reduced.Reduced && reduced.Trace[0].Seconds == 0 &&
                  Math.Abs(reduced.Trace[reduced.Trace.Count - 1].Position.x - 655.49f) < .001f,
              "long-run samples stay bounded while preserving the beginning and latest position");
        int before = reduced.Trace.Count;
        Check(!reduced.Sample(double.NaN, UnityEngine.Vector3.zero) &&
                  !reduced.Sample(1, UnityEngine.Vector3.zero) && reduced.Trace.Count == before,
              "invalid and backward-time samples cannot poison a saved trace");
        var rate = new RunReviewData();
        for (int i = 0; i < 1000; i++)
            rate.Sample(i * .001, UnityEngine.Vector3.zero);
        Check(rate.Trace.Count == 50, "normal movement recording is capped at 50 Hz");
        string recovery = Path.Combine(root, "review-recovery-test");
        Directory.CreateDirectory(Path.Combine(recovery, "RunReviews"));
        File.WriteAllBytes(Path.Combine(recovery, "RunReviews", "run-" + sample.Id + ".isrun"), encoded);
        File.WriteAllBytes(Path.Combine(recovery, "RunReviews", "run-" + (sample.Id + 1) + ".isrun"),
                           new byte[] { 1, 2, 3 });
        RunReviewRecorder.Restore(recovery);
        var deadline = Stopwatch.StartNew();
        while (deadline.ElapsedMilliseconds < 5000 &&
               (RunReviewRecorder.Last == null || RunReviewRecorder.Last.Review.Id != sample.Id))
            System.Threading.Thread.Sleep(1);
        Check(RunReviewRecorder.Last != null && RunReviewRecorder.Last.Review.Id == sample.Id &&
                  RunReviewRecorder.RecoveryWarning != null,
              "background recovery finds the previous intact run and reports a damaged latest file");
        foreach (bool curve in new[] { false, true })
        {
            var motion = new ReplayMonster { Id = 1, Kind = 2 };
            for (int i = 0; i <= 10000; i++)
            {
                double at = i * .1;
                motion.Sample(
                    at, new UnityEngine.Vector3((float)(at * 8), 0, curve ? (float)(2 * Math.Sin(at)) : 0),
                    i > 0);
            }
            Check(motion.Trace.Count < (curve ? 6000 : 600) && motion.Trace[0].Seconds == 0 &&
                      motion.Trace[motion.Trace.Count - 1].Seconds == 1000,
                  "linear keyframes retain the whole monster timeline with bounded samples");
            int next = 1;
            for (int i = 1; i <= 10000; i++)
            {
                double at = i * .1;
                while (next + 1 < motion.Trace.Count && motion.Trace[next].Seconds < at)
                    next++;
                var a = motion.Trace[next - 1];
                var b = motion.Trace[next];
                double factor = (at - a.Seconds) / (b.Seconds - a.Seconds);
                var predicted = a.Position + (b.Position - a.Position) * (float)factor;
                var actual =
                    new UnityEngine.Vector3((float)(at * 8), 0, curve ? (float)(2 * Math.Sin(at)) : 0);
                Check(
                    (predicted - actual).magnitude <= .0201f,
                    "monster keyframes reproduce every observed straight/curved sample within two centimeters");
            }
        }
        Check(!RunValidity.Eligible(new RunRecord { ObservedFromStart = true, SeedKnown = true,
                                                    SeedType = SeedKind.Random, ValidationChecked = true,
                                                    Warnings = (RunWarnings)(1u << 25) }),
              "unknown integrity warning bits fail closed");
        CheckReplayRetention(root, packet);
        Console.WriteLine(
            "v16 commands, rotated wall/floor geometry, sticky validation, forbidden-command guard, legacy migration, 50 Hz/bounded trace and persisted review recovery: PASS");
    }

    private static void CheckReplayRetention(string root, PracticeMapData fixture)
    {
        string directory = Path.Combine(root, "retention-test");
        if (Directory.Exists(directory))
            Directory.Delete(directory, true);
        Directory.CreateDirectory(directory);
        var store = new RunStore(directory);
        long id = DateTime.UtcNow.Ticks;
        var active =
            typeof(RunReviewRecorder).GetField("active", BindingFlags.Static | BindingFlags.NonPublic);
        var instance =
            typeof(SeedMapPlanner).GetProperty("Instance", BindingFlags.Static | BindingFlags.Public);
        Check(instance.GetValue(null, null) == null, "retention fixture has no live Unity planner");
        for (int attempt = 0; attempt < 4; attempt++)
        {
            var record = new RunRecord { Id = id + attempt, Scene = fixture.Scene,
                                         Seconds = attempt == 0   ? 10
                                                   : attempt == 1 ? 10.001
                                                                  : 1,
                                         Outcome = RunOutcome.Reset };
            RunReviewRecorder.ConfigureRetention(10, attempt >= 2 ? 3u : 0u);
            RunReviewRecorder.Begin(record, null);
            // Changing preferences mid-attempt must affect only the next run.
            RunReviewRecorder.ConfigureRetention(600, 1000);
            var packet = fixture.Copy();
            packet.Review = new RunReviewData { Id = record.Id };
            packet.Review.Sample(0, UnityEngine.Vector3.zero);
            if (attempt >= 2)
                for (int i = 0; i < (attempt == 2 ? 2 : 3); i++)
                {
                    packet.Review.Collected.Add(i);
                    packet.Review.PickupSeconds.Add(.1 * i);
                }
            active.SetValue(null, packet);
            RunReviewRecorder.Finish(record, store);
            bool retained = attempt == 1 || attempt == 3;
            Check(RunReviewRecorder.Last.Review.Id == record.Id && RunReviewRecorder.LastRetained == retained,
                  "recording uses its frozen time / crystal rule");
            Check(RunReviewRecorder.Find(record.Id) != null,
                  "finished attempts can be reviewed before restart");
            var status = new SnapshotBuffer();
            RunReviewRecorder.WriteStatus(status, record.Id, record.Scene);
            Check(BitConverter.ToInt64(status.Bytes, SharedLayout.ReviewRunId) == record.Id &&
                      BitConverter.ToInt32(status.Bytes, SharedLayout.ReviewScene) == record.Scene &&
                      status.Bytes[SharedLayout.ReviewReady] == 1,
                  "status binds to the exact finished recording");
            status.Clear();
            RunReviewRecorder.WriteStatus(status, id + 10000, 13);
            Check(BitConverter.ToInt64(status.Bytes, SharedLayout.ReviewRunId) == id + 10000 &&
                      BitConverter.ToInt32(status.Bytes, SharedLayout.ReviewScene) == 13 &&
                      status.Bytes[SharedLayout.ReviewReady] == 0 &&
                      BitConverter.ToInt32(status.Bytes, SharedLayout.ReviewSamples) == 0,
                  "missing capture preserves current identity and never attaches the previous replay");
            status.Clear();
            RunReviewRecorder.WriteStatus(status, record.Id, record.Scene == 13 ? 3 : 13);
            Check(status.Bytes[SharedLayout.ReviewReady] == 0,
                  "a recording from another scene cannot satisfy the requested run");
            RunReviewRecorder.DiscardTransient();
            Check((RunReviewRecorder.Find(record.Id) != null) == retained,
                  "restart discards only temporary attempts");
        }
        store.Dispose();
        var files = Directory.GetFiles(Path.Combine(directory, "RunReviews"), "run-*.isrun");
        Check(files.Length == 2, "only qualifying attempts reach the durable replay archive");
        foreach (var file in files)
        {
            var saved = PracticeMapArchive.Decode(File.ReadAllBytes(file));
            Check(saved.Review.Id == id + 1 || saved.Review.Id == id + 3,
                  "retained replay keeps exact attempt identity");
        }
        RunReviewRecorder.ConfigureRetention(10, 0);
        var older = new SnapshotBuffer();
        RunReviewRecorder.WriteStatus(older, id + 1, fixture.Scene);
        Check(BitConverter.ToInt64(older.Bytes, SharedLayout.ReviewRunId) == id + 1 &&
                  older.Bytes[SharedLayout.ReviewReady] == 1,
              "an older retained recording is requested by identity instead of substituting the latest");
        Console.WriteLine(
            "Replay retention: frozen rule, strict 10-second boundary, actual crystal count, transient restart discard, durable archive identity and exact current/older/missing/scene-bound replay metadata: PASS");
    }

    private sealed class ReferenceRng
    {
        public uint[] words = new uint[4];
        public int init, range, one;

        public void Initialize(int seed)
        {
            words[0] = unchecked((uint)seed);
            for (int i = 1; i < 4; i++)
            {
                uint value = words[i - 1];
                if (init == 1 || init == 3)
                    value ^= value >> 30;
                words[i] = unchecked(1812433253u * value + (uint)((init == 1 || init == 2) ? i : 1));
            }
        }

        public int Draw(int min, int max)
        {
            uint width = (uint)(max - min);
            if (width == 1 && one == 0)
                return min;
            uint old = words[0], last = words[3];
            old ^= old << 11;
            uint next = last ^ (last >> 19) ^ old ^ (old >> 8);
            Array.Copy(words, 1, words, 0, 3);
            words[3] = next;
            ulong value = next;
            if (range == 1)
                value &= 0x7fffffff;
            if (range == 4 && value > int.MaxValue)
                value = 4294967296UL - value;
            uint result = range == 2   ? (uint)(value * width / 4294967296UL)
                          : range == 3 ? (uint)((value / 2) * width / 2147483648UL)
                                       : (uint)(value % width);
            return min + (int)result;
        }

        public PortableRandom State()
        {
            return new PortableRandom { X = words[0], Y = words[1], Z = words[2], W = words[3] };
        }

        public void Set(PortableRandom state)
        {
            words[0] = state.X;
            words[1] = state.Y;
            words[2] = state.Z;
            words[3] = state.W;
        }
    }

    private static void CheckPortable(string directory)
    {
        for (int init = 0; init < 4; init++)
            for (int range = 0; range < 5; range++)
                for (int one = 0; one < 2; one++)
                {
                    var oracle = new ReferenceRng { init = init, range = range, one = one };
                    var verifier =
                        new PortableRngVerifier(9, 34, 18, 12, SeedMapPlanner.SpawnSite, oracle.Initialize,
                                                oracle.Draw, oracle.State, oracle.Set);
                    while (verifier.Status == 1)
                        verifier.Probe();
                    Check(verifier.Status == 2 && verifier.Completed == 4160 &&
                              verifier.Descriptor.InitMode == init &&
                              verifier.Descriptor.RangeMode == range && verifier.Descriptor.AdvanceOne == one,
                          "candidate discovery " + init + "/" + range + "/" + one + " at " +
                              verifier.Completed + ": " + verifier.Note);
                }
        var bad = new ReferenceRng();
        var reject = new PortableRngVerifier(9, 34, 18, 12, SeedMapPlanner.SpawnSite, bad.Initialize,
                                             (low, high) =>
                                             {
                                                 bad.Draw(low, high);
                                                 return low;
                                             },
                                             bad.State);
        while (reject.Status == 1)
            reject.Probe();
        Check(reject.Status == 3,
              "unmatched runtime must fall back instead of exporting plausible but wrong seeds");
        var a = new UnityEngine.Vector3(1, 2, 3);
        var b = new UnityEngine.Vector3(4, 5, 6);
        var c = new UnityEngine.Vector3(7, 8, 9);
        var aligned = CatalogOrder.Match(new[] { a, b, c }, new[] { c, a, b });
        Check(aligned != null && aligned[0] == 1 && aligned[1] == 2 && aligned[2] == 0,
              "Practice reload restores captured catalog order after shuffled tag lookup");
        Check(CatalogOrder.Match(new[] { a, b, c }, new[] { a, a, c }) == null,
              "changed catalog cannot silently claim the selected seed layout");
        using (var output = new BinaryWriter(File.Create(Path.Combine(directory, "portable-rng-wire.bin"))))
        {
            output.Write(0x52474e50);
            output.Write(40 * 8);
            for (int init = 0; init < 4; init++)
                for (int range = 0; range < 5; range++)
                    for (int one = 0; one < 2; one++)
                        foreach (int seed in new[] { 0, 1, -1, int.MinValue, int.MaxValue, 123, -7123,
                                                     47839471 })
                        {
                            output.Write(init);
                            output.Write(range);
                            output.Write(one);
                            output.Write(seed);
                            output.Write(SeedMapPlanner.SpawnSite);
                            var oracle = new ReferenceRng { init = init, range = range, one = one };
                            int applied = PracticeRandom.DeriveSeed(seed, 12, SeedMapPlanner.SpawnSite, 0);
                            output.Write(applied);
                            oracle.Initialize(applied);
                            var selection = SpawnSelection.Select(9, 34, 18, -1, oracle.Draw);
                            output.Write(selection.Spawn);
                            foreach (int id in selection.Crystals)
                                output.Write(id);
                        }
        }
        var cpuMap = new PracticeMapData { Pid = 6789,
                                           Scene = 12,
                                           Epoch = 1,
                                           Seed = 0,
                                           LiveSeed = 0,
                                           LiveRecipe = 2,
                                           Recipe = 2,
                                           SourceKind = 2,
                                           Spawn = 0,
                                           LiveSpawn = 0,
                                           Keep = 2,
                                           Flags = 1,
                                           Stage = 2,
                                           Spawns = new[] { a, b },
                                           Points = new[] { b, c },
                                           Selected = new[] { 0, 1 },
                                           LiveSelected = new[] { 0, 1 },
                                           RngStatus = 2,
                                           RngProbes = 4160,
                                           RngInit = 0,
                                           RngRange = 0,
                                           RngAdvance = 1,
                                           RngSite = SeedMapPlanner.SpawnSite,
                                           SearchBackend = 1,
                                           RngNote = "Synthetic verified CPU encoder fixture." };
        File.WriteAllBytes(Path.Combine(directory, "portable-map-wire.bin"), cpuMap.Encode());
    }

    private static void CheckPlayerWallRouting(string directory)
    {
        Check(PlayerWallLeg.Blocks(-2, 0, 2, 0, .5, .5, .35), "player leg across wall is rejected");
        Check(!PlayerWallLeg.Blocks(-2, .9, 2, .9, .5, .5, .35), "player-width clearance is retained");
        Check(!PlayerWallLeg.Blocks(.75, .75, .75, .75, .5, .5, .35),
              "rounded corner gap is not incorrectly closed by rectangular inflation");
        Check(PlayerWallLeg.Blocks(.7, .7, .7, .7, .5, .5, .35), "insufficient corner clearance is rejected");
        var random = new Random(1417);
        for (int trial = 0; trial < 1000; trial++)
        {
            double ax = random.NextDouble() * 12 - 6, az = random.NextDouble() * 12 - 6,
                   bx = random.NextDouble() * 12 - 6, bz = random.NextDouble() * 12 - 6;
            double hx = .2 + random.NextDouble() * 2, hz = .2 + random.NextDouble() * 2,
                   radius = .1 + random.NextDouble() * .5, best = double.PositiveInfinity;
            for (int step = 0; step <= 2048; step++)
            {
                double t = step / 2048.0, x = ax + (bx - ax) * t, z = az + (bz - az) * t,
                       dx = Math.Max(0, Math.Abs(x) - hx), dz = Math.Max(0, Math.Abs(z) - hz);
                best = Math.Min(best, Math.Sqrt(dx * dx + dz * dz));
            }
            bool result = PlayerWallLeg.Blocks(ax, az, bx, bz, hx, hz, radius);
            // Distance to a closed set is 1-Lipschitz. This independent dense
            // reference bounds the unsampled minimum by half a step length.
            double gap = Math.Sqrt((bx - ax) * (bx - ax) + (bz - az) * (bz - az)) / 4096.0;
            if (best < radius - 1e-9)
                Check(result, "wall validator cannot miss a sampled collision");
            if (best - gap > radius + 1e-9)
                Check(!result, "wall validator cannot reject independently proven clearance");
        }
        Func<UnityEngine.Vector3, UnityEngine.Vector3, bool> clear = (a, b) =>
            !PlayerWallLeg.Blocks(a.x - 10, a.z - 6, b.x - 10, b.z - 6, 2, 6, .35);
        var graph = new NavigationGraphBuilder();
        int start = graph.AddNode(new UnityEngine.Vector3(2, 0, 5)),
            target = graph.AddNode(new UnityEngine.Vector3(18, 0, 5)),
            other = graph.AddNode(new UnityEngine.Vector3(2, 0, 15));
        Check(!graph.AddCompletePath(
                  start, target,
                  new[] { new UnityEngine.Vector3(2, 0, 5), new UnityEngine.Vector3(18, 0, 5) }, 2, clear) &&
                  graph.Edges().Length == 0,
              "complete enemy path through wall commits no graph edges");
        Check(!graph.AddCompletePath(start, target,
                                     new[] { new UnityEngine.Vector3(2, 0, 5),
                                             new UnityEngine.Vector3(7.5f, 0, 13),
                                             new UnityEngine.Vector3(18, 0, 5) },
                                     3, clear) &&
                  graph.Edges().Length == 0 && graph.Nodes().Length == 3,
              "later blocked leg cannot leak a partial corridor or shortcut");
        var corners = new[] { new UnityEngine.Vector3(2, 0, 5), new UnityEngine.Vector3(7.5f, 0, 13),
                              new UnityEngine.Vector3(12.5f, 0, 13), new UnityEngine.Vector3(18, 0, 5) };
        Check(graph.AddCompletePath(start, target, corners, 4, clear),
              "complete wall-clear detour remains available");
        Check(graph.AddCompletePath(
                  start, other, new[] { new UnityEngine.Vector3(2, 0, 5), new UnityEngine.Vector3(2, 0, 15) },
                  2, clear),
              "clear corridor remains available");
        Check(graph.AddCompletePath(target, other,
                                    new[] { new UnityEngine.Vector3(18, 0, 5),
                                            new UnityEngine.Vector3(12.5f, 0, 13),
                                            new UnityEngine.Vector3(2, 0, 15) },
                                    3, clear),
              "reverse-side corridor remains available");
        var packet = new PracticeMapData { Pid = 6789,
                                           Scene = 5,
                                           Epoch = 1,
                                           Flags = 1,
                                           Stage = 1,
                                           Spawn = 0,
                                           LiveSpawn = 0,
                                           Keep = 2,
                                           NavigationStatus = 2,
                                           NavigationModel = 1,
                                           NavigationQueries = 3,
                                           NavigationTotal = 3 };
        packet.Spawns = new[] { new UnityEngine.Vector3(2, 1, 5) };
        packet.Points = new[] { new UnityEngine.Vector3(18, 1, 5), new UnityEngine.Vector3(2, 1, 15) };
        packet.Selected = packet.LiveSelected = new[] { 0, 1 };
        packet.Vertices = new[] { new UnityEngine.Vector3(0, 0, 0), new UnityEngine.Vector3(20, 0, 0),
                                  new UnityEngine.Vector3(20, 0, 20), new UnityEngine.Vector3(0, 0, 20) };
        packet.Indices = new[] { 0, 1, 2, 0, 2, 3 };
        packet.AgentRadius = .35f;
        packet.AgentHeight = 1.8f;
        packet.AgentStep = .3f;
        packet.AgentFloor = 0;
        packet.AgentSlope = 45;
        packet.Obstacles = packet.WallOutlines = new[] { new WallOutline {
            Corners = new[] { new UnityEngine.Vector3(8, 0, 0), new UnityEngine.Vector3(12, 0, 0),
                              new UnityEngine.Vector3(12, 0, 12), new UnityEngine.Vector3(8, 0, 12) }
        } };
        packet.Walls = new[] { new UnityEngine.Rect(8, 0, 4, 12) };
        packet.NavigationNodes = graph.Nodes();
        packet.NavigationEdges = graph.Edges();
        packet.NavigationAnchors = new[] { start, target, other };
        Directory.CreateDirectory(directory);
        File.WriteAllBytes(Path.Combine(directory, "checked-player-navigation-wire.bin"), packet.Encode());
        Console.WriteLine(
            "Player wall clearance, rounded corners, 1000 independent dense references, atomic complete paths and managed player-corridor wire: PASS");
    }

    private static void CheckSeedMaps(string directory)
    {
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "PlannerRequest").ToInt32() == 64,
              "planner request offset");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "PlannerSeed").ToInt32() == 68,
              "planner seed offset");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "PlannerLevel").ToInt32() == 72,
              "planner map offset");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "PlannerOptions").ToInt32() == 76,
              "planner options offset");
        var command = new TrainerCommands { TelemetryOptions = 0x80000080u };
        Check(command.RequestsPlanner, "explicit planner request");
        command.SpeedrunModeActive = 1;
        Check(!command.RequestsPlanner, "clean mode blocks planner");
        command.PracticeModeActive = 1;
        Check(command.RequestsPlanner, "practice enables planner");
        int calls = 0;
        int[] draws = { 7, 2, 0, 2, 0 };
        Func<int, int, int> range = (min, max) =>
        {
            int i = calls++;
            Check(i == 0 ? min == 1 && max == 30 : min == 0 && max == (i == 1 ? 7 : 8 - i),
                  "stock random draw order");
            return draws[i];
        };
        var selection = SpawnSelection.Select(7, 6, 3, -1, range);
        Check(selection.Spawn == 2 && selection.Crystals.Length == 3 && selection.Crystals[0] == 3 &&
                  selection.Crystals[1] == 1 && selection.Crystals[2] == 0 && calls == 5,
              "reverse crystal array and random removals");
        calls = 0;
        var firstTimer = SpawnSelection.Select(7, 6, 3, 4, range);
        Check(firstTimer.Spawn == 4 && calls == 5, "first Timer override still consumes spawn draw");
        Check(SpawnSelection.Matches(selection, 2, new[] { 0, 1, 3 }) &&
                  !SpawnSelection.Matches(selection, 2, new[] { 0, 0, 3 }),
              "exact selection set comparison");
        var simulate =
            typeof(SeedMapPlanner).GetMethod("Simulate", BindingFlags.Instance | BindingFlags.NonPublic);
        bool restoreFinally = false;
        foreach (ExceptionHandlingClause clause in simulate.GetMethodBody().ExceptionHandlingClauses)
            if (clause.Flags == ExceptionHandlingClauseOptions.Finally)
                restoreFinally = true;
        Check(restoreFinally, "simulation RNG state restore uses finally");
        Check(typeof(SeedMapPlanner).GetMethod("BeforeStart").GetParameters()[0].Name == "__instance",
              "Harmony Start prefix injects the actual player instance");
        var p1 = new UnityEngine.Vector3(1, 2, 3);
        var p2 = new UnityEngine.Vector3(4, 5, 6);
        Check(SeedMapPlanner.SamePositions(new[] { p1, p2 }, new[] { p2, p1 }) &&
                  !SeedMapPlanner.SamePositions(new[] { p1, p2 }, new[] { p1, p1 }),
              "preview replay compares actual coordinates, not unstable array IDs");
        var original = GameInstructions("Start");
        int tagCalls = 0;
        foreach (var instruction in original)
        {
            var method = instruction.operand as MethodInfo;
            if (method != null && method.DeclaringType == typeof(UnityEngine.GameObject) &&
                method.Name == "FindGameObjectsWithTag")
                tagCalls++;
        }
        Check(tagCalls >= 2, "actual player Start contains spawn and point arrays");
        var transpiler = typeof(HarmonyPatches)
                             .GetMethod("CaptureSpawnArrays", BindingFlags.Static | BindingFlags.NonPublic);
        var patched = (IEnumerable<CodeInstruction>)transpiler.Invoke(null, new object[] { original });
        int capturedCalls = 0;
        foreach (var instruction in patched)
        {
            var method = instruction.operand as MethodInfo;
            if (method != null && method.DeclaringType == typeof(SeedMapPlanner) &&
                method.Name == "FindTagged")
                capturedCalls++;
        }
        Check(capturedCalls == tagCalls,
              "capture replaces every actual Start tag lookup without changing selection order");
        var packet = new PracticeMapData { Pid = 6789,
                                           Scene = 3,
                                           Epoch = 1,
                                           Seed = -123,
                                           LiveSeed = -123,
                                           Recipe = 1,
                                           LiveRecipe = 1,
                                           SourceKind = 1,
                                           Spawn = 0,
                                           LiveSpawn = 0,
                                           Keep = 1,
                                           Flags = 1 | 8,
                                           Stage = 1,
                                           Requested = 1,
                                           Completed = 1,
                                           CapturedUtc = 123456,
                                           Hash = new string('a', 64),
                                           Message = "managed wire fixture" };
        packet.Spawns = new[] { new UnityEngine.Vector3(0, 1, 0) };
        packet.Points = new[] { new UnityEngine.Vector3(1.25f, 2.5f, 3.75f) };
        packet.Selected = packet.LiveSelected = new[] { 0 };
        packet.Samples.Add(new SpawnSelection { Spawn = 0, Crystals = new[] { 0 } });
        packet.RequestSerial = 99;
        var nativePacket = new PracticeMapData { Pid = 6789,
                                                 Scene = 7,
                                                 Epoch = 1,
                                                 Spawn = 0,
                                                 LiveSpawn = 0,
                                                 Keep = 2,
                                                 Stage = 1,
                                                 Flags = 1,
                                                 NavigationStatus = 2,
                                                 NavigationQueries = 3,
                                                 NavigationTotal = 3 };
        nativePacket.Spawns = new[] { new UnityEngine.Vector3(2, 1, 2) };
        nativePacket.Points = new[] { new UnityEngine.Vector3(8, 1, 2), new UnityEngine.Vector3(18, 1, 2) };
        nativePacket.Selected = nativePacket.LiveSelected = new[] { 0, 1 };
        nativePacket.NavigationNodes =
            new[] { nativePacket.Spawns[0], nativePacket.Points[0], nativePacket.Points[1] };
        nativePacket.NavigationEdges = new[] { 0, 1, 1, 2 };
        nativePacket.NavigationAnchors = new[] { 0, 1, 2 };
        File.WriteAllBytes(Path.Combine(directory, "native-navigation-wire.bin"), nativePacket.Encode());
        byte[] bytes = packet.Encode();
        Check(BitConverter.ToUInt32(bytes, 0) == 0x504D5349u && BitConverter.ToInt32(bytes, 4) == 13 &&
                  BitConverter.ToInt32(bytes, 8) == 6789 && BitConverter.ToUInt32(bytes, 76) == 99,
              "map v13 wire header and request ticket");
        Directory.CreateDirectory(directory);
        File.WriteAllBytes(Path.Combine(directory, "map-wire.bin"), bytes);
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "PlannerSearchCount").ToInt32() == 80 &&
                  Marshal.OffsetOf(typeof(TrainerCommands), "PlannerBudgetMicros").ToInt32() == 88,
              "64-bit search count and budget offsets");
        IntPtr packed = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(TrainerCommands)));
        try
        {
            var hugeCommand =
                new TrainerCommands { PlannerSearchCount = 4294967296UL, PlannerBudgetMicros = 8000,
                                      PlannerSearchKey = 0x0123456789abcdefUL };
            Marshal.StructureToPtr(hugeCommand, packed, false);
            byte[] commandBytes = new byte[192];
            Marshal.Copy(packed, commandBytes, 0, commandBytes.Length);
            Check(BitConverter.ToUInt64(commandBytes, 80) == 4294967296UL &&
                      BitConverter.ToUInt32(commandBytes, 88) == 8000,
                  "large search command really marshals without truncation");
            var restoredCommand = (TrainerCommands)Marshal.PtrToStructure(packed, typeof(TrainerCommands));
            Check(restoredCommand.PlannerSearchCount == 4294967296UL &&
                      restoredCommand.PlannerSearchKey == 0x0123456789abcdefUL &&
                      BitConverter.ToUInt64(commandBytes, 92) == 0x0123456789abcdefUL,
                  "large command count and random key roundtrip");
        }
        finally
        {
            Marshal.FreeHGlobal(packed);
        }
        for (int size = 1; size <= 1024; size = size < 16 ? size + 1 : size * 2)
        {
            int keep = Math.Min(14, size);
            var generator = new SpawnSelectionGenerator(7, size, keep);
            for (int seed = 0; seed < 100; seed++)
            {
                var reference = new System.Random(seed);
                var optimized = new System.Random(seed);
                Func<int, int, int> originalRange = (min, max) => reference.Next(min, max);
                Func<int, int, int> scratchRange = (min, max) => optimized.Next(min, max);
                var originalSelection =
                    SpawnSelection.Select(7, size, keep, seed % 3 == 0 ? 4 : -1, originalRange);
                var optimizedSelection = generator.Next(seed % 3 == 0 ? 4 : -1, scratchRange);
                Check(originalSelection.Spawn == optimizedSelection.Spawn &&
                          originalSelection.Crystals.Length == optimizedSelection.Crystals.Length,
                      "scratch selection sizes");
                for (int i = 0; i < keep; i++)
                    Check(originalSelection.Crystals[i] == optimizedSelection.Crystals[i],
                          "scratch selection preserves exact stock order");
                Check(reference.Next() == optimized.Next(),
                      "scratch selection consumes exactly the same RNG draws");
            }
        }
        // Reused selection output has the exact stock draws/order as well.
        var output = new int[14];
        var bufferGenerator = new SpawnSelectionGenerator(7, 64, 14);
        for (int seed = 0; seed < 200; seed++)
        {
            var referenceBufferRng = new Random(seed);
            var optimized = new Random(seed);
            var selected =
                SpawnSelection.Select(7, 64, 14, -1, (min, max) => referenceBufferRng.Next(min, max));
            int spawn = bufferGenerator.NextInto(-1, (min, max) => optimized.Next(min, max), output);
            Check(spawn == selected.Spawn, "buffer generator spawn");
            for (int i = 0; i < output.Length; i++)
                Check(output[i] == selected.Crystals[i], "buffer generator exact order");
            Check(referenceBufferRng.Next() == optimized.Next(),
                  "buffer generator keeps subsequent RNG draw");
        }
        uint[] sampleSlots = { 0u, 1u, 2u, 65535u, 65536u, 0x7fffffffu, 0x80000000u, 0xffffffffu };
        int[] expectedRandomSeeds = { -1979949308, -2081764548, -2127917975, 1038828463,
                                      -1174749596, 904229284,   -947813596,  2114310595 };
        var sampleOrder = new RandomSeedOrder(0x0123456789abcdefUL);
        for (int i = 0; i < sampleSlots.Length; i++)
            Check(sampleOrder.SeedAt(sampleSlots[i]) == expectedRandomSeeds[i],
                  "independent random-order vectors");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "PlannerSearchKey").ToInt32() == 92,
              "random search key offset");
        using (var seedFile = new BinaryWriter(File.Create(Path.Combine(directory, "seed-order-wire.bin"))))
        {
            for (uint i = 0; i < 1024; i++)
            {
                ulong key = unchecked(0x0123456789abcdefUL + 0x9e3779b97f4a7c15UL * i);
                uint slot = unchecked(i * 2654435761u);
                seedFile.Write(key);
                seedFile.Write(slot);
                seedFile.Write(new RandomSeedOrder(key).SeedAt(slot));
            }
        }
        var batch = new SeedSearchBatch { Pid = 6789,
                                          Scene = 3,
                                          Epoch = 1,
                                          RequestSerial = 99,
                                          Seed = new RandomSeedOrder(0x0123456789abcdefUL).SeedAt(0),
                                          SearchKey = 0x0123456789abcdefUL,
                                          Recipe = 1,
                                          Total = 4294967296UL,
                                          Offset = 0,
                                          Hash = new string('a', 64) };
        batch.Layouts.Add(
            new SeedSearchLayout { Seed = batch.Seed,
                                   Selection = new SpawnSelection { Spawn = 0, Crystals = new[] { 0 } } });
        byte[] batchBytes = batch.Encode();
        File.WriteAllBytes(Path.Combine(directory, "seed-batch-wire.bin"), batchBytes);
        Check(BitConverter.ToUInt64(batchBytes, 32) == 4294967296UL &&
                  BitConverter.ToUInt64(batchBytes, 40) == 0,
              "batch 64-bit total and offset");
        // Compare the allocation-free writer against the independent existing
        // BinaryWriter encoder, including the last seeds of the full domain.
        var wire = new SeedSearchBatch { Pid = 6789,
                                         Scene = 3,
                                         Epoch = 1,
                                         RequestSerial = 99,
                                         Seed = batch.Seed,
                                         SearchKey = batch.SearchKey,
                                         Recipe = 1,
                                         Total = 4294967296UL,
                                         Offset = 4294967296UL - 2048,
                                         Hash = new string('a', 64),
                                         Status = 1,
                                         GenerationMicros = 1234567,
                                         GenerationFrames = 99 };
        var reusable = new SeedSearchBatchWriter(wire, 64);
        reusable.Begin(wire.Offset);
        var crystals = new int[64];
        for (int i = 0; i < 64; i++)
            crystals[i] = i;
        for (int i = 0; i < 2048; i++)
        {
            int next = sampleOrder.SeedAt(unchecked((uint)(wire.Offset + (ulong)i)));
            reusable.Add(next, i % 7, crystals);
            wire.Layouts.Add(new SeedSearchLayout { Seed = next, Selection = new SpawnSelection {
                                                       Spawn = i % 7, Crystals = (int[])crystals.Clone()
                                                   } });
        }
        byte[] direct = reusable.Finish(wire.GenerationMicros, wire.GenerationFrames),
               referenceBytes = wire.Encode();
        Check(direct.Length == referenceBytes.Length && direct.Length < 600000,
              "large packet remains bounded");
        for (int i = 0; i < direct.Length; i++)
            Check(direct[i] == referenceBytes[i], "direct writer matches BinaryWriter wire");
        File.WriteAllBytes(Path.Combine(directory, "seed-large-wire.bin"), direct);
        bool overflow = false;
        try
        {
            reusable.Add(1, 0, crystals);
        }
        catch (InvalidDataException)
        {
            overflow = true;
        }
        Check(overflow, "full search buffer cannot overflow");
        reusable.Begin(0);
        reusable.Add(batch.Seed, 0, crystals);
        byte[] firstBytes = reusable.Finish(1, 1);
        Check(BitConverter.ToUInt64(firstBytes, 40) == 0 && BitConverter.ToInt32(firstBytes, 48) == 0 &&
                  firstBytes.Length < direct.Length,
              "writer reset reuses buffer without stale rows");
        var generate =
            typeof(SeedMapPlanner).GetMethod("GenerateBatch", BindingFlags.Instance | BindingFlags.NonPublic);
        bool batchFinally = false;
        foreach (ExceptionHandlingClause clause in generate.GetMethodBody().ExceptionHandlingClauses)
            if (clause.Flags == ExceptionHandlingClauseOptions.Finally)
                batchFinally = true;
        Check(batchFinally, "batched generator restores real Unity RNG state in finally");
        using (var mapChannel = new PracticeMapChannel(987654, 55))
        {
            mapChannel.Queue(packet);
            mapChannel.Flush();
            Check(!mapChannel.ReadyForBatch(), "catalog must be consumed before batches");
            var channelView =
                (System.IO.MemoryMappedFiles.MemoryMappedViewAccessor) typeof(PracticeMapChannel)
                    .GetField("view", BindingFlags.Instance | BindingFlags.NonPublic)
                    .GetValue(mapChannel);
            uint first = channelView.ReadUInt32(4);
            Check(!mapChannel.TryPublishBatch(batchBytes), "unread catalog cannot be overwritten by a batch");
            channelView.Write(12, first);
            Check(mapChannel.ReadyForBatch() && mapChannel.TryPublishBatch(batchBytes),
                  "acknowledged catalog accepts first batch");
            uint second = channelView.ReadUInt32(4);
            Check(second != first && !mapChannel.ReadyForBatch() && !mapChannel.TryPublishBatch(batchBytes),
                  "unread batch backpressure and revision");
            Check(channelView.ReadUInt32(64) == 0x42535349u, "batch really written to optional map channel");
            channelView.Write(12, second);
            Check(mapChannel.TryPublishBatch(batchBytes), "consuming batch releases backpressure");
            channelView.Write(12, channelView.ReadUInt32(4));
            int viewLength;
            byte[] viewBytes = reusable.FinishView(2, 2, out viewLength);
            Check(mapChannel.TryPublishBatch(viewBytes, viewLength) &&
                      channelView.ReadInt32(0) == viewLength && viewLength < viewBytes.Length,
                  "publish only the used reusable packet bytes");
            uint stableRevision = channelView.ReadUInt32(4);
            byte stableFirst = channelView.ReadByte(64);
            // The synchronous channel owns a copy, not an alias of scratch.
            viewBytes[0] ^= 1;
            Check(channelView.ReadByte(64) == stableFirst &&
                      !mapChannel.TryPublishBatch(viewBytes, viewLength) &&
                      channelView.ReadUInt32(4) == stableRevision,
                  "packet copy stays immutable until acknowledged");
            viewBytes[0] ^= 1;
            Check(!mapChannel.TryPublishBatch(viewBytes, viewBytes.Length + 1) &&
                      !mapChannel.TryPublishBatch(viewBytes, 0),
                  "logical packet length guard");
        }
        Check(SharedLayout.Size == 16952 && SharedLayout.HitboxDetails == 7146,
              "extended telemetry retains the existing hitbox offsets");
        Console.WriteLine(
            "Spawn selection, first Timer override, Clean-mode gating, actual Start transpiler and RNG finally: PASS");
    }

    private static List<CodeInstruction> GameInstructions(string name)
    {
        MethodInfo method = typeof(MouseLook).GetMethod(name, BindingFlags.Instance | BindingFlags.Public |
                                                                  BindingFlags.NonPublic);
        return MethodInstructions(method);
    }

    private static List<CodeInstruction> MethodInstructions(MethodInfo method)
    {
        byte[] il = method.GetMethodBody().GetILAsByteArray();
        var one = (OpCode[]) typeof(HarmonyPatches)
                      .GetField("oneByte", BindingFlags.Static | BindingFlags.NonPublic)
                      .GetValue(null);
        var two = (OpCode[]) typeof(HarmonyPatches)
                      .GetField("twoByte", BindingFlags.Static | BindingFlags.NonPublic)
                      .GetValue(null);
        var result = new List<CodeInstruction>();
        for (int i = 0; i < il.Length;)
        {
            byte b = il[i++];
            OpCode op = b == 0xfe ? two[il[i++]] : one[b];
            object operand = null;
            if (op.OperandType == OperandType.InlineMethod)
                operand = method.Module.ResolveMethod(BitConverter.ToInt32(il, i));
            if (op.OperandType == OperandType.InlineField)
                operand = method.Module.ResolveField(BitConverter.ToInt32(il, i));
            if (op.OperandType == OperandType.InlineString)
                operand = method.Module.ResolveString(BitConverter.ToInt32(il, i));
            if (op.OperandType == OperandType.ShortInlineR)
                operand = BitConverter.ToSingle(il, i);
            if (op.OperandType == OperandType.InlineR)
                operand = BitConverter.ToDouble(il, i);
            result.Add(new CodeInstruction(op, operand));
            switch (op.OperandType)
            {
            case OperandType.InlineNone:
                break;
            case OperandType.ShortInlineBrTarget:
            case OperandType.ShortInlineI:
            case OperandType.ShortInlineVar:
                i++;
                break;
            case OperandType.InlineVar:
                i += 2;
                break;
            case OperandType.InlineI8:
            case OperandType.InlineR:
                i += 8;
                break;
            case OperandType.InlineSwitch:
                i += 4 + 4 * BitConverter.ToInt32(il, i);
                break;
            default:
                i += 4;
                break;
            }
        }
        return result;
    }

    private static bool IsCall(CodeInstruction instruction, Type type, string name)
    {
        var method = instruction.operand as MethodBase;
        return method != null && method.DeclaringType == type && method.Name == name;
    }

    private static void CheckGameControls()
    {
        var movement = GameInstructions("FixedUpdate");
        int vertical = movement.FindIndex(i => Equals(i.operand, "Vertical"));
        int horizontal = movement.FindIndex(i => Equals(i.operand, "Horizontal"));
        Check(vertical >= 0 && movement[vertical + 2].opcode == OpCodes.Stloc_1 && horizontal >= 0 &&
                  movement[horizontal + 2].opcode == OpCodes.Stloc_0,
              "actual game input axes assigned to expected locals");
        int right = movement.FindIndex(i => IsCall(i, typeof(UnityEngine.Transform), "get_right"));
        int forward = movement.FindIndex(i => IsCall(i, typeof(UnityEngine.Transform), "get_forward"));
        Check(right >= 0 && Equals(movement[right - 1].operand, typeof(MouseLook).GetField("playerBody")) &&
                  movement[right + 1].opcode == OpCodes.Ldloc_1 &&
                  IsCall(movement[right + 2], typeof(UnityEngine.Vector3), "op_Multiply"),
              "supplied game really moves forward along playerBody.right (+90 degree yaw)");
        Check(forward >= 0 && movement[forward + 1].opcode == OpCodes.Ldloc_0 &&
                  movement[forward + 2].opcode == OpCodes.Neg &&
                  IsCall(movement[forward + 3], typeof(UnityEngine.Vector3), "op_Multiply"),
              "supplied game's right strafe is negative playerBody.forward");
        var update = GameInstructions("Update");
        int title = update.FindIndex(i => Equals(i.operand, "TitleScreen"));
        Check(update.Exists(i => Equals(i.operand, "escape")) && title >= 0 &&
                  IsCall(update[title + 1], typeof(UnityEngine.SceneManagement.SceneManager), "LoadScene"),
              "supplied game Escape menu transition loads TitleScreen");
        Console.WriteLine("Actual game DLL movement axes / Escape menu transition: PASS");
    }

    private static void SetManagerField(SpeedrunManager manager, string name, object value)
    {
        typeof(SpeedrunManager)
            .GetField(name, BindingFlags.Instance | BindingFlags.NonPublic)
            .SetValue(manager, value);
    }

    private static double ManagerElapsed(SpeedrunManager manager)
    {
        return (double)typeof(SpeedrunManager)
            .GetProperty("Elapsed", BindingFlags.Instance | BindingFlags.NonPublic)
            .GetValue(manager, null);
    }

    private static void CheckMenuAbort(string root)
    {
        string directory = Path.Combine(root, "menu-abort-test");
        Directory.CreateDirectory(directory);
        foreach (string file in Directory.GetFiles(directory))
            File.Delete(file);
        string reports = Path.Combine(directory, "Reports");
        if (Directory.Exists(reports))
            foreach (string file in Directory.GetFiles(reports))
                File.Delete(file);
        Check(!Directory.Exists(reports) || Directory.GetFiles(reports, "*.json").Length == 0,
              "test report cleanup");
        var manager = new SpeedrunManager(directory);
        var tracker = (RunTracker) typeof(SpeedrunManager)
                          .GetField("tracker", BindingFlags.Instance | BindingFlags.NonPublic)
                          .GetValue(manager);
        var store = (RunStore) typeof(SpeedrunManager)
                        .GetField("store", BindingFlags.Instance | BindingFlags.NonPublic)
                        .GetValue(manager);
        var seed = new SeedEvidence();
        seed.Reset(true);
        seed.Observe(42, 42, false, 0);
        long now = DateTime.UtcNow.Ticks;
        // Attach a real active record without invoking Unity's native scene or
        // clock APIs. SceneLoaded, End, snapshots and persistence are production code.
        tracker.Begin(3, now, RunAids.None, seed, new RunEnvironment(), true);
        SetManagerField(manager, "active", true);
        SetManagerField(manager, "runScene", 3);
        SetManagerField(manager, "started", Stopwatch.GetTimestamp() - 7 * Stopwatch.Frequency);
        SetManagerField(manager, "timerState", (byte)1);
        manager.SceneLoaded(2); // Actual Escape destination: TitleScreen.
        double stopped = ManagerElapsed(manager);
        Check(stopped >= 7 && tracker.Active == null && tracker.Last.Outcome == RunOutcome.Interrupted &&
                  tracker.Last.Seconds == stopped && !tracker.Last.PersonalBest,
              "game menu aborts once and latches the recorded final time");
        SetManagerField(manager, "started", Stopwatch.GetTimestamp() - 300 * Stopwatch.Frequency);
        Check(ManagerElapsed(manager) == stopped,
              "aborted timer cannot resume or advance after leaving the menu");
        var buffer = new SnapshotBuffer();
        manager.WriteSnapshot(buffer, new TrainerCommands());
        Check(buffer.Bytes[SharedLayout.TimerState] == 2 &&
                  buffer.Bytes[SharedLayout.RunOutcome] == (byte)RunOutcome.Interrupted &&
                  BitConverter.ToDouble(buffer.Bytes, SharedLayout.CurrentRunTimeSeconds) == stopped,
              "native overlay receives stopped timer and interrupted outcome");
        manager.SceneLoaded(2);
        Check(store.Runs == 1 && store.Resets == 0 && store.Finished == 0 && store.History.Count == 1 &&
                  ManagerElapsed(manager) == stopped,
              "duplicate menu callbacks do not add attempts, resets or completions");
        manager.SceneLoaded(3);
        Check(ManagerElapsed(manager) == 0 && tracker.Active == null,
              "new playable scene is ready for a new attempt without resuming old run");
        tracker.Begin(3, now + 1, RunAids.None, seed);
        SetManagerField(manager, "started", Stopwatch.GetTimestamp() - 9 * Stopwatch.Frequency);
        SetManagerField(manager, "timerState", (byte)1);
        typeof(SpeedrunManager)
            .GetMethod("End", BindingFlags.Instance | BindingFlags.NonPublic)
            .Invoke(manager, new object[] { RunOutcome.Completed });
        double completed = ManagerElapsed(manager);
        manager.SceneLoaded(2);
        Check(tracker.Last.Outcome == RunOutcome.Completed && ManagerElapsed(manager) == completed &&
                  store.Finished == 1 && store.History.Count == 2,
              "menu after completion preserves result and exact completed time");
        manager.Dispose();
        var loaded = new RunStore(directory);
        Check(loaded.History.Count == 2 && loaded.History[0].Outcome == RunOutcome.Interrupted &&
                  loaded.History[0].Seconds == stopped && loaded.Runs == 2 && loaded.Resets == 0,
              "menu abort and new run persist without extra reset");
        loaded.Dispose();
        string[] reportFiles = Directory.GetFiles(reports, "*.json");
        Check(reportFiles.Length == 1 && File.ReadAllText(reportFiles[0]).Contains("Interrupted"),
              "verification report preserves menu interruption");
        Console.WriteLine(
            "Real speedrun manager menu abort / stopped snapshot / duplicate load / next run / persistence: PASS");
    }

    private static void CheckStatisticsReset(string root)
    {
        string folder = Path.Combine(root, "statistics-reset-test");
        if (Directory.Exists(folder))
            Directory.Delete(folder, true);
        var seed = new SeedEvidence();
        seed.Reset(true);
        seed.Observe(1, 1, false, 0);
        long now = DateTime.UtcNow.Ticks;
        using (var store = new RunStore(folder))
        {
            var tracker = new RunTracker(store);
            tracker.Begin(3, now, RunAids.None, seed);
            Check(!tracker.ResetStatistics(), "statistics cannot reset during an active run");
            tracker.End(RunOutcome.Reset, now + 39990000, 3.999, 0);
            Check(store.Runs == 0 && store.Resets == 0 && tracker.SessionAttempts == 0 && store.Today(0) == 0,
                  "quick empty resets stay out of lifetime, daily and session counters");
            store.Save();
        }
        using (var store = new RunStore(folder))
        {
            Check(store.Runs == 0 && store.Resets == 0,
                  "restart does not restore ignored attempts from history");
            var tracker = new RunTracker(store);
            tracker.Begin(3, now, RunAids.None, seed);
            tracker.End(RunOutcome.Reset, now + 40000000, 4, 0);
            Check(store.Runs == 1 && store.Resets == 1 && tracker.SessionResets == 1,
                  "regular reset still counts");
            tracker.Begin(3, now + 30000000, RunAids.None, seed);
            tracker.End(RunOutcome.Reset, now + 35000000, .5, .1f);
            Check(store.Runs == 2 && store.Resets == 2, "short attempts with progress still count");
            store.RecordBest(3, 45.123);
            File.WriteAllText(Path.Combine(folder, "retained-replay"), "keep");
            Check(tracker.ResetStatistics(), "reset stored statistics");
            Check(store.Runs == 0 && store.Finished == 0 && store.Resets == 0 && store.TimedResets == 0 &&
                      store.Today(0) == 0 && tracker.SessionAttempts == 0,
                  "counter views reset together");
            Check(store.Best(3) == 45.123 && File.Exists(Path.Combine(folder, "retained-replay")),
                  "statistics reset preserves personal bests and replay files");
            store.Save();
        }
        using (var store = new RunStore(folder))
            Check(store.Runs == 0 && store.Best(3) == 45.123, "statistics reset survives restart");
    }

    private static void CheckTracking(string root)
    {
        string directory = Path.Combine(root, "run-tracking-test");
        Directory.CreateDirectory(directory);
        foreach (string file in Directory.GetFiles(directory))
            File.Delete(file);
        var seed = new SeedEvidence();
        seed.Reset(false);
        seed.Observe(7, 7, false, 0);
        Check(seed.Known && seed.Seed == 7 && seed.Kind == SeedKind.Unknown,
              "observed numeric seed retained while incomplete coverage stays unknown");
        seed.Reset(true);
        seed.Observe(-123, -123, false, 0);
        seed.Observe(456, 456, false, 0);
        Check(seed.Known && seed.Kind == SeedKind.Random && seed.Seed == -123 && seed.Reseeds == 2,
              "first random seed latched across later reseeds");
        long now = DateTime.UtcNow.Ticks;
        var store = new RunStore(directory);
        var tracker = new RunTracker(store);
        Check(tracker.Begin(3, now, RunAids.None, seed), "first attempt starts");
        tracker.Active.ValidationChecked =
            true; // The model receives a completed local audit from the runtime.
        Check(!tracker.Begin(3, now, RunAids.None, seed) && store.Runs == 1, "duplicate Start hook ignored");
        Check(tracker.End(RunOutcome.Completed, now + 100000000, 10, 1), "completion detected");
        Check(!tracker.End(RunOutcome.Completed, now, 10, 1) && store.Finished == 1 && store.Resets == 0,
              "duplicate completion ignored");
        Check(store.Best(3) == 10 && store.History[0].PersonalBest, "observed unassisted random local PB");
        tracker.Begin(3, now + 1, RunAids.None, seed);
        tracker.Mark(RunAids.Fullbright);
        tracker.End(RunOutcome.Completed, now + 200000000, 5, 1);
        Check(store.Best(3) == 10 && !store.History[1].PersonalBest,
              "latched assistance excludes faster local PB");
        tracker.Begin(3, now + 2, RunAids.None, seed);
        seed.Observe(11, 22, true, 0);
        tracker.ObserveSeed(seed); // Zero is a valid set seed.
        Check(tracker.Active.SeedType == SeedKind.Set && tracker.Active.Seed == 0,
              "actual forced reseed marks set seed");
        tracker.End(RunOutcome.Reset, now + 300000000, 3, .4f);
        Check(!tracker.End(RunOutcome.Death, now, 3, .4f) && store.Resets == 1 && store.Deaths == 0,
              "reset/death callbacks cannot double count");
        tracker.Begin(3, now + 3, RunAids.None, seed);
        tracker.End(RunOutcome.Death, now + 400000000, 7, .2f);
        Check(store.Resets == 2 && store.Deaths == 1 && store.TotalResetTime == 10 && store.TimedResets == 2,
              "death is one reset with one elapsed sample");
        Check(tracker.SessionAttempts == 4 && tracker.SessionCompleted == 2 && tracker.SessionResets == 2,
              "session accounting");
        var uncertain = new SeedEvidence();
        uncertain.Reset(true);
        uncertain.Observe(1, 1, false, 0);
        uncertain.RevokeOriginalPath();
        Check(uncertain.Kind == SeedKind.Unknown && uncertain.Known && !uncertain.Coverage,
              "foreign RNG patch revokes random classification without losing observed value");
        Check(store.Today(0) == 4 && store.Today(1) == 2 && store.Today(2) == 2, "daily accounting");
        store.Dispose();
        Check(store.LastError == null, "background writer flushed");
        var reloaded = new RunStore(directory);
        Check(reloaded.Runs == 4 && reloaded.Resets == 2 && reloaded.History.Count == 4 &&
                  reloaded.History[2].SeedKnown && reloaded.History[2].SeedType == SeedKind.Set &&
                  reloaded.History[2].Seed == 0,
              "history/seed/counters survive restart");
        var pending = new RunTracker(reloaded);
        pending.Begin(8, now + 4, RunAids.None, seed);
        pending.Update(12, .5f);
        reloaded.Dispose();
        var recovered = new RunStore(directory);
        Check(recovered.Active == null && recovered.Runs == 5 && recovered.Resets == 2 &&
                  recovered.History[4].Outcome == RunOutcome.Abandoned,
              "interrupted process recovery without invented resets");
        Check(new RunStore(directory).History.Count == 5, "recovery is idempotent");
        for (int i = 0; i < 70; i++)
            recovered.AddHistory(new RunRecord { Id = now + i, Number = i, Scene = 3, StartedUtc = now,
                                                 EndedUtc = now + 1, Outcome = RunOutcome.Reset });
        recovered.Save();
        Check(new RunStore(directory).History.Count == 64, "bounded run history");
        string migration = Path.Combine(root, "v4-stats-migration");
        Directory.CreateDirectory(migration);
        File.WriteAllText(Path.Combine(migration, "speedruns-v3.txt"),
                          "version=3\nruns=20\nfinished=5\ndeaths=15\ntotalSeconds=200\npb.3=30\n");
        var previous = new RunStore(migration);
        Check(previous.Runs == 20 && previous.Resets == 15 && previous.TimedResets == 0 &&
                  previous.Best(3) == 30,
              "v4 totals and per-level PB migration");
        previous.Dispose();
        recovered.Dispose();
        CheckRecoveryAndReports(root);
    }

    private static void CheckRecoveryAndReports(string root)
    {
        string directory = Path.Combine(root, "v6-recovery-test");
        Directory.CreateDirectory(directory);
        foreach (string f in Directory.GetFiles(directory))
            File.Delete(f);
        var original = new RunStore(directory);
        original.Runs = 8;
        original.Finished = 2;
        original.Resets = 6;
        original.Save();
        original.Runs = 9;
        original.Save();
        original.Dispose();
        string path = Path.Combine(directory, "speedruns-v3.txt"), good = File.ReadAllText(path);
        File.WriteAllText(path, good.Replace("runs=9", "runs=broken"));
        var backup = new RunStore(directory);
        Check(backup.LoadWarning != null && backup.Runs == 9,
              "checksum failure recovers previous good backup");
        backup.Save();
        backup.Dispose();
        Check(Directory.GetFiles(directory, "*.corrupt-*").Length > 0 && new RunStore(directory).Runs == 9,
              "damaged primary preserved and recovered data committed");
        File.WriteAllText(path, "version=999\nruns=123\n");
        string future = File.ReadAllText(path);
        var newer = new RunStore(directory);
        newer.Runs = 99;
        newer.QueueSave();
        newer.Dispose();
        Check(newer.LastError != null && File.ReadAllText(path) == future,
              "future version cannot be overwritten");
        string reports = Path.Combine(root, "v6-report-test");
        Directory.CreateDirectory(reports);
        foreach (string f in Directory.GetFiles(reports))
            File.Delete(f);
        string reportDir = Path.Combine(reports, "Reports");
        if (Directory.Exists(reportDir))
            foreach (string f in Directory.GetFiles(reportDir))
                File.Delete(f);
        var store = new RunStore(reports);
        var tracker = new RunTracker(store);
        var seed = new SeedEvidence();
        seed.Reset(true);
        seed.Observe(42, 42, false, 0);
        long now = DateTime.UtcNow.Ticks;
        var environment = new RunEnvironment { GameVersion = "test \"version\"\\line\n",
                                               UnityVersion = "test", GameHash = "UNKNOWN" };
        tracker.Begin(3, now, RunAids.None, seed, environment, true);
        tracker.Update(1, .1f);
        tracker.ObserveFeatures(RunAids.Fullbright);
        tracker.ObserveFeatures(RunAids.None);
        tracker.Warn(RunWarnings.ObservationGap);
        tracker.Warn(RunWarnings.ObservationGap);
        tracker.End(RunOutcome.Completed, now + 20000000, 2, 1);
        store.Dispose();
        Check(store.LastReportError == null && !store.ReportsPending, "report worker flush");
        string[] files = Directory.GetFiles(reportDir, "*.json");
        Check(files.Length == 1, "one report per completed run");
        string report = File.ReadAllText(files[0]);
        Check(report.Contains("\"integrity\": \"UNVALID\"") && report.Contains("FeaturesChanged") &&
                  report.Contains("ObservationGap") &&
                  report.Contains("test \\\"version\\\"\\\\line\\u000a") && !store.History[0].PersonalBest,
              "report JSON escaping, latched aids and warning PB exclusion");
        var loaded = new RunStore(reports);
        Check(loaded.History[0].Warnings == RunWarnings.ObservationGap &&
                  loaded.History[0].Environment.GameVersion == environment.GameVersion,
              "verification metadata migration/roundtrip");
        var running = new RunTracker(loaded);
        running.Begin(3, now + 1, RunAids.None, seed, environment, true);
        running.Update(4, .2f);
        Check(running.Active.Events.Count <= 128, "event count bounded initially");
        for (int i = 0; i < 1000; i++)
            running.Event("Test", i.ToString());
        Check(running.Active.Events.Count == 128 &&
                  (running.Active.Warnings & RunWarnings.EventLogTruncated) != 0,
              "event log has fixed limit and explicit truncation warning");
        loaded.Dispose();
        var recovered = new RunStore(reports);
        recovered.Dispose();
        Check(recovered.History[1].Outcome == RunOutcome.Abandoned && recovered.History[1].EndedUtc == 0 &&
                  (recovered.History[1].Warnings & RunWarnings.ProcessStopped) != 0,
              "crash report keeps unknown stop time and warning");
        string invalidDirectory = Path.Combine(root, "invalid-timing-test");
        Directory.CreateDirectory(invalidDirectory);
        foreach (string f in Directory.GetFiles(invalidDirectory))
            File.Delete(f);
        var invalidStore = new RunStore(invalidDirectory);
        var invalid = new RunTracker(invalidStore);
        invalid.Begin(3, now + 2, RunAids.None, seed);
        invalid.Update(5, .3f);
        invalid.Update(double.NaN, .5f);
        Check(invalid.Active.Seconds == 5 && (invalid.Active.Warnings & RunWarnings.InvalidTiming) != 0,
              "invalid time cannot shorten a run");
        invalid.End(RunOutcome.Completed, now + 30000000, 6, 1);
        Check(!invalid.Last.PersonalBest, "invalid timing cannot earn PB");
        invalidStore.Dispose();
        var preserved = new RunStore(reports);
        preserved.QueueReport(preserved.History[0]);
        preserved.Dispose();
        Check(preserved.LastReportError != null && File.ReadAllText(files[0]) == report,
              "recovered journal cannot overwrite an existing report");
        string duplicates = Path.Combine(root, "duplicate-utc-test");
        Directory.CreateDirectory(duplicates);
        foreach (string f in Directory.GetFiles(duplicates))
            File.Delete(f);
        string duplicateReports = Path.Combine(duplicates, "Reports");
        if (Directory.Exists(duplicateReports))
            foreach (string f in Directory.GetFiles(duplicateReports))
                File.Delete(f);
        var duplicateStore = new RunStore(duplicates);
        var duplicateTracker = new RunTracker(duplicateStore);
        duplicateTracker.Begin(3, now, RunAids.None, seed, environment, true);
        duplicateTracker.End(RunOutcome.Reset, now + 1, 1, .1f);
        duplicateTracker.Begin(3, now, RunAids.None, seed, environment, true);
        duplicateTracker.End(RunOutcome.Reset, now + 1, 1, .1f);
        duplicateStore.Dispose();
        Check(duplicateStore.History.Count == 2 &&
                  duplicateStore.History[0].Id != duplicateStore.History[1].Id &&
                  duplicateStore.History[0].StartedUtc == duplicateStore.History[1].StartedUtc &&
                  Directory.GetFiles(duplicateReports, "*.json").Length == 2,
              "same UTC start timestamps preserve both attempts and unique reports");
        var partialStore = new RunStore(duplicates);
        var partial = new RunTracker(partialStore);
        partial.Begin(3, now + 2, RunAids.None, seed, environment, false, false);
        partial.End(RunOutcome.Completed, now + 3, 1, 1);
        partialStore.Dispose();
        Check(!partial.Last.ObservedFromStart && !partial.Last.PersonalBest &&
                  (partial.Last.Warnings & RunWarnings.StateUnavailable) != 0,
              "fallback attachment cannot claim a fully observed start or local PB");
        var flags = new TrainerCommands { TelemetryOptions = 0x80000000u | 4u };
        Check(!flags.RequestsWalls && !flags.RequestsCrystals && !flags.RequestsMonster,
              "unrequested map categories skip export");
        flags.TelemetryOptions |= 16u | 32u | 64u;
        Check(flags.RequestsWalls && flags.RequestsCrystals && flags.RequestsMonster,
              "category requests explicit");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "ReportOptions").ToInt32() == 56 &&
                  Marshal.OffsetOf(typeof(TrainerCommands), "TeleportScene").ToInt32() == 60,
              "protocol v5 reserved command extensions");
        Check(Marshal.OffsetOf(typeof(TrainerCommands), "TeleportExitFreecam").ToInt32() == 41,
              "teleport freecam exit flag packing");
        MethodInfo merge =
            typeof(DataExporter).GetMethod("Merge", BindingFlags.Static | BindingFlags.NonPublic);
        var input =
            new System.Collections.Generic.List<UnityEngine.Rect> { UnityEngine.Rect.MinMaxRect(0, 0, 1, 2),
                                                                    UnityEngine.Rect.MinMaxRect(1, 0, 3, 2) };
        var output = new System.Collections.Generic.List<UnityEngine.Rect>();
        merge.Invoke(null, new object[] { input, true, output });
        Check(output.Count == 1 && output[0].xMax == 3 && output[0].yMax == 2,
              "reused wall merge preserves geometry");
    }
}
