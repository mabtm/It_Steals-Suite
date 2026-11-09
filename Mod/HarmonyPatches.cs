using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using System.Reflection.Emit;
using HarmonyLib;
using MelonLoader;

public static class PracticeRandom
{
    public static bool Enabled { get; private set; }
    public static int BaseSeed { get; private set; }
    public static bool RawSpawnSeed { get; private set; }
    public static int PatchedCallSites { get; internal set; }
    public static bool SourcesVerified { get; private set; }
    public static readonly SeedEvidence Evidence = new SeedEvidence();

    public static void VerifySources(bool verified)
    {
        SourcesVerified = verified;
        if (!verified)
        {
            Evidence.RevokeOriginalPath();
            Disable();
        }
    }

    public static void SceneLoaded(int index)
    {
        scene = index;
        calls.Clear();
        Evidence.Reset(SourcesVerified && PatchedCallSites == 22);
    }

    private static int scene;
    private static readonly Dictionary<int, uint> calls = new Dictionary<int, uint>();

    public static void Begin(int seed, int sceneIndex, bool enabled, bool rawSpawnSeed = false)
    {
        BaseSeed = seed;
        scene = sceneIndex;
        RawSpawnSeed = rawSpawnSeed;
        calls.Clear();
        Enabled = enabled && SourcesVerified && PatchedCallSites == 22;
        if (enabled && !Enabled)
            MelonLogger.Error(
                "Seed mode unavailable: supported game build and complete original RNG hooks are required.");
        if (Enabled)
        {
            if (SpeedrunManager.Instance != null)
                SpeedrunManager.Instance.MarkAssisted(RunAids.FixedSeed);
            UnityEngine.Random.InitState(DeriveSeed(seed, sceneIndex, 0, 0));
        }
    }

    public static void Disable()
    {
        Enabled = false;
        calls.Clear();
    }

    public static int DeriveSeed(int seed, int sceneIndex, int site, uint occurrence)
    {
        unchecked
        {
            uint x = (uint)seed ^ ((uint)sceneIndex * 0x9E3779B9u) ^ ((uint)site * 0x85EBCA6Bu) ^
                     (occurrence * 0xC2B2AE35u);
            x ^= x >> 16;
            x *= 0x7FEB352Du;
            x ^= x >> 15;
            x *= 0x846CA68Bu;
            x ^= x >> 16;
            return (int)x;
        }
    }

    public static void InitState(int originalSeed, int site)
    {
        if (!Enabled)
        {
            UnityEngine.Random.InitState(originalSeed);
            SeedMapPlanner.ObserveRng(site, originalSeed, 0, false, false);
            Evidence.Observe(originalSeed, originalSeed, false, 0);
            return;
        }
        uint occurrence;
        calls.TryGetValue(site, out occurrence);
        calls[site] = unchecked(occurrence + 1);
        int applied = RawSpawnSeed && site == SeedMapPlanner.SpawnSite && occurrence == 0
                          ? BaseSeed
                          : DeriveSeed(BaseSeed, scene, site, occurrence);
        UnityEngine.Random.InitState(applied);
        SeedMapPlanner.ObserveRng(site, applied, BaseSeed, true, RawSpawnSeed);
        Evidence.Observe(originalSeed, applied, true, BaseSeed, RawSpawnSeed);
    }
}

// Patch managed call sites, including compiler-generated coroutine MoveNext.
// Random.InitState itself is a Unity internal call and is deliberately left intact.
public static class HarmonyPatches
{
    private static readonly MethodInfo initState =
        typeof(UnityEngine.Random).GetMethod("InitState", new[] { typeof(int) });
    private static readonly MethodInfo replacement = typeof(PracticeRandom).GetMethod("InitState");
    private static readonly MethodInfo cursorVisible =
        typeof(UnityEngine.Cursor).GetProperty("visible").GetSetMethod();
    private static readonly MethodInfo cursorLock =
        typeof(UnityEngine.Cursor).GetProperty("lockState").GetSetMethod();
    private static readonly FieldInfo objectives = typeof(MouseLook).GetField("Objectives");
    private static readonly OpCode[] oneByte = new OpCode[256], twoByte = new OpCode[256];
    private static readonly List<MethodInfo> rngMethods = new List<MethodInfo>();
    private static bool originalSources;
    private static string owner;
    public static bool SupportedBuild;

    public static bool GameplayPatchesTrusted()
    {
        foreach (var name in new[] { "Start", "Update", "FixedUpdate", "OnTriggerEnter", "Death" })
        {
            var info = HarmonyLib.Harmony.GetPatchInfo(AccessTools.Method(typeof(MouseLook), name));
            if (info != null)
                foreach (var id in info.Owners)
                    if (id != owner)
                        return false;
        }
        return true;
    }

    public static void RefreshSeedCoverage()
    {
        bool trusted = SupportedBuild && originalSources && PracticeRandom.PatchedCallSites == 22;
        foreach (MethodInfo method in rngMethods)
        {
            var patches = HarmonyLib.Harmony.GetPatchInfo(method);
            if (patches == null)
            {
                trusted = false;
                break;
            }
            bool owned = false;
            foreach (string id in patches.Owners)
            {
                if (id == owner)
                    owned = true;
                else
                    trusted = false;
            }
            if (!owned)
                trusted = false;
        }
        PracticeRandom.VerifySources(trusted);
    }

    static HarmonyPatches()
    {
        foreach (FieldInfo field in typeof(OpCodes).GetFields(BindingFlags.Public | BindingFlags.Static))
        {
            OpCode op = (OpCode)field.GetValue(null);
            ushort value = unchecked((ushort)op.Value);
            if (value < 256)
                oneByte[value] = op;
            else if ((value & 0xFF00) == 0xFE00)
                twoByte[value & 255] = op;
        }
    }

    public static void Install(HarmonyLib.Harmony harmony)
    {
        PracticeRandom.PatchedCallSites = 0;
        owner = harmony.Id;
        rngMethods.Clear();
        int gameSources = 0;
        foreach (Type type in typeof(MouseLook).Assembly.GetTypes())
        {
            foreach (MethodInfo method in type.GetMethods(BindingFlags.Public | BindingFlags.NonPublic |
                                                          BindingFlags.Instance | BindingFlags.Static |
                                                          BindingFlags.DeclaredOnly))
            {
                int count = CountReseeds(method);
                if (CountCursorWrites(method) > 0)
                    harmony.Patch(
                        method, transpiler: new HarmonyMethod(typeof(HarmonyPatches), "CaptureCursorIntent"));
                if (count == 0)
                    continue;
                rngMethods.Add(method);
                gameSources += CountGameSeedSources(method);
                harmony.Patch(method,
                              transpiler: new HarmonyMethod(typeof(HarmonyPatches), "RewriteReseeds"));
                PracticeRandom.PatchedCallSites += count;
            }
        }
        originalSources = gameSources == 22;
        RefreshSeedCoverage();
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "Start"),
                      prefix: new HarmonyMethod(typeof(SeedMapPlanner), "BeforeStart"),
                      postfix: new HarmonyMethod(typeof(HarmonyPatches), "PlayerReady"),
                      transpiler: new HarmonyMethod(typeof(HarmonyPatches), "CaptureSpawnArrays"));
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "OnTriggerEnter"),
                      prefix: new HarmonyMethod(typeof(FreeCam), "AllowPlayerTrigger"));
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "OnTriggerEnter"),
                      prefix: new HarmonyMethod(typeof(Replay3D), "AllowPlayerTrigger"));
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "Update"),
                      prefix: new HarmonyMethod(typeof(Replay3D), "AllowPlayerUpdate"));
        ReplayGamePause.Install(harmony);
        ReplayExperienceCapture.Install(harmony);
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "OnTriggerEnter"),
                      prefix: new HarmonyMethod(typeof(RunReviewRecorder), "BeforePickup"),
                      postfix: new HarmonyMethod(typeof(RunReviewRecorder), "AfterPickup"),
                      transpiler: new HarmonyMethod(typeof(HarmonyPatches), "CaptureFinish"));
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "Death"),
                      prefix: new HarmonyMethod(typeof(HarmonyPatches), "Death"));
        foreach (var type in typeof(HideAndSeekIt)
                     .GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic))
            if (type.Name.StartsWith("<OccludeWalls>", StringComparison.Ordinal))
                harmony.Patch(AccessTools.Method(type, "MoveNext"),
                              transpiler: new HarmonyMethod(typeof(HarmonyPatches), "FreecamWallVisibility"));
        harmony.Patch(AccessTools.Method(typeof(HideAndSeekIt), "RayTest"),
                      prefix: new HarmonyMethod(typeof(ReplayCapture), "BeforeHideLight"),
                      postfix: new HarmonyMethod(typeof(ReplayCapture), "AfterHideLight"));
        harmony.Patch(AccessTools.Method(typeof(MonsterController), "RayTest"),
                      prefix: new HarmonyMethod(typeof(ReplayCapture), "BeforeClassicLight"),
                      postfix: new HarmonyMethod(typeof(ReplayCapture), "AfterClassicLight"));
        harmony.Patch(AccessTools.Method(typeof(HideAndSeekIt), "FixedUpdate"),
                      prefix: new HarmonyMethod(typeof(ReplayCapture), "BeforeHideTick"),
                      postfix: new HarmonyMethod(typeof(ReplayCapture), "AfterHideTick"));
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "Update"),
                      postfix: new HarmonyMethod(typeof(ReplayCapture), "AfterPlayerUpdate"));
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "Update"),
                      prefix: new HarmonyMethod(typeof(ReplayInputCapture), "BeforeUpdate"),
                      postfix: new HarmonyMethod(typeof(ReplayInputCapture), "AfterUpdate"),
                      transpiler: new HarmonyMethod(typeof(ReplayInputCapture), "ObserveUpdate"),
                      finalizer: new HarmonyMethod(typeof(ReplayInputCapture), "UpdateFailed"),
                      ilmanipulator: null);
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "FixedUpdate"),
                      postfix: new HarmonyMethod(typeof(ReplayInputCapture), "Physics"));
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "FixedUpdate"),
                      prefix: new HarmonyMethod(typeof(MovementCapture), "BeforePhysics"),
                      postfix: new HarmonyMethod(typeof(MovementCapture), "AfterPhysics"),
                      transpiler: new HarmonyMethod(typeof(MovementCapture), "ObserveAxes"),
                      finalizer: new HarmonyMethod(typeof(MovementCapture), "PhysicsFailed"),
                      ilmanipulator: null);
        harmony.Patch(AccessTools.Method(typeof(MouseLook), "CameraFlash"),
                      postfix: new HarmonyMethod(typeof(ReplayCapture), "CameraStarted"));
        foreach (var type in typeof(MonsterController)
                     .GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic))
            if (type.Name.StartsWith("<RayTestStun>", StringComparison.Ordinal))
                harmony.Patch(AccessTools.Method(type, "MoveNext"),
                              transpiler: new HarmonyMethod(typeof(HarmonyPatches), "CaptureCameraStun"));
        foreach (var type in typeof(Monster3).GetNestedTypes(BindingFlags.Public | BindingFlags.NonPublic))
            if (type.Name.StartsWith("<RayTest>", StringComparison.Ordinal))
                harmony.Patch(AccessTools.Method(type, "MoveNext"),
                              transpiler: new HarmonyMethod(typeof(HarmonyPatches), "CaptureTimerSight"));
        MelonLogger.Msg("Practice RNG: " + PracticeRandom.PatchedCallSites + " managed reseeds patched.");
    }

    public static IEnumerable<CodeInstruction> CaptureCameraStun(IEnumerable<CodeInstruction> instructions)
    {
        var field = AccessTools.Field(typeof(MonsterController), "IsStunned");
        var setter = AccessTools.Method(typeof(ReplayCapture), "CameraStunned");
        foreach (var instruction in instructions)
        {
            if (instruction.opcode == OpCodes.Stfld && Equals(instruction.operand, field))
            {
                instruction.opcode = OpCodes.Call;
                instruction.operand = setter;
            }
            yield return instruction;
        }
    }

    public static IEnumerable<CodeInstruction> CaptureTimerSight(IEnumerable<CodeInstruction> instructions)
    {
        var field = AccessTools.Field(typeof(WallAI), "isSpotted");
        var setter = AccessTools.Method(typeof(ReplayCapture), "TimerSpotted");
        foreach (var instruction in instructions)
        {
            if (instruction.opcode == OpCodes.Stfld && Equals(instruction.operand, field))
            {
                instruction.opcode = OpCodes.Call;
                instruction.operand = setter;
            }
            yield return instruction;
        }
    }

    private static int CountReseeds(MethodInfo method)
    {
        MethodBody body = method.GetMethodBody();
        if (body == null)
            return 0;
        byte[] il = body.GetILAsByteArray();
        int count = 0;
        for (int i = 0; i < il.Length;)
        {
            byte b = il[i++];
            OpCode op = b == 0xFE ? twoByte[il[i++]] : oneByte[b];
            if (op.OperandType == OperandType.InlineMethod)
            {
                int token = BitConverter.ToInt32(il, i);
                if (op == OpCodes.Call && method.Module.ResolveMethod(token) == initState)
                    count++;
            }
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
        return count;
    }

    private static int CountGameSeedSources(MethodInfo method)
    {
        MethodBody body = method.GetMethodBody();
        if (body == null)
            return 0;
        byte[] il = body.GetILAsByteArray();
        int count = 0;
        MethodBase previousCall = null, beforePreviousCall = null;
        OpCode previousOp = OpCodes.Nop;
        for (int i = 0; i < il.Length;)
        {
            byte b = il[i++];
            OpCode op = b == 0xFE ? twoByte[il[i++]] : oneByte[b];
            MethodBase call = op == OpCodes.Call && op.OperandType == OperandType.InlineMethod
                                  ? method.Module.ResolveMethod(BitConverter.ToInt32(il, i))
                                  : null;
            if (call == initState)
            {
                bool clock = previousCall != null && (previousCall.DeclaringType == typeof(Environment) &&
                                                          previousCall.Name == "get_TickCount" ||
                                                      previousCall.DeclaringType == typeof(DateTime) &&
                                                          previousCall.Name == "get_Millisecond");
                bool position = previousOp == OpCodes.Conv_I4 && beforePreviousCall != null &&
                                beforePreviousCall.DeclaringType == typeof(UnityEngine.Vector3) &&
                                beforePreviousCall.Name == "Distance" &&
                                method.DeclaringType == typeof(WallAI);
                if (clock || position)
                    count++;
            }
            beforePreviousCall = previousCall;
            previousOp = op;
            previousCall = call;
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
        return count;
    }

    private static int CountCursorWrites(MethodInfo method)
    {
        MethodBody body = method.GetMethodBody();
        if (body == null)
            return 0;
        byte[] il = body.GetILAsByteArray();
        int count = 0;
        for (int i = 0; i < il.Length;)
        {
            byte b = il[i++];
            OpCode op = b == 0xFE ? twoByte[il[i++]] : oneByte[b];
            if (op == OpCodes.Call && op.OperandType == OperandType.InlineMethod)
            {
                MethodBase call = method.Module.ResolveMethod(BitConverter.ToInt32(il, i));
                if (call == cursorVisible || call == cursorLock)
                    count++;
            }
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
        return count;
    }

    private static IEnumerable<CodeInstruction> CaptureCursorIntent(IEnumerable<CodeInstruction> instructions)
    {
        foreach (CodeInstruction instruction in instructions)
        {
            if (instruction.opcode == OpCodes.Call && Equals(instruction.operand, cursorVisible))
                instruction.operand = typeof(MenuCursor).GetMethod("SetGameVisible");
            else if (instruction.opcode == OpCodes.Call && Equals(instruction.operand, cursorLock))
                instruction.operand = typeof(MenuCursor).GetMethod("SetGameLock");
            yield return instruction;
        }
    }

    private static IEnumerable<CodeInstruction> CaptureSpawnArrays(IEnumerable<CodeInstruction> instructions)
    {
        var original =
            typeof(UnityEngine.GameObject).GetMethod("FindGameObjectsWithTag", new[] { typeof(string) });
        var capture = typeof(SeedMapPlanner).GetMethod("FindTagged");
        foreach (var instruction in instructions)
        {
            if (instruction.opcode == OpCodes.Call && Equals(instruction.operand, original))
                instruction.operand = capture;
            yield return instruction;
        }
    }

    private static IEnumerable<CodeInstruction> RewriteReseeds(IEnumerable<CodeInstruction> instructions,
                                                               MethodBase __originalMethod)
    {
        int occurrence = 0;
        foreach (CodeInstruction instruction in instructions)
        {
            if (instruction.opcode == OpCodes.Call && Equals(instruction.operand, initState))
            {
                int site = unchecked(__originalMethod.MetadataToken * 31 + occurrence++);
                // Branches targeting the original call must also push the site ID.
                var push = new CodeInstruction(OpCodes.Ldc_I4, site);
                push.labels.AddRange(instruction.labels);
                instruction.labels.Clear();
                push.blocks.AddRange(instruction.blocks);
                instruction.blocks.Clear();
                yield return push;
                instruction.operand = replacement;
            }
            yield return instruction;
        }
    }

    private static IEnumerable<CodeInstruction> FreecamWallVisibility(
        IEnumerable<CodeInstruction> instructions)
    {
        var list = new List<CodeInstruction>(instructions);
        var body = typeof(HideAndSeekIt).GetField("playerBody");
        var position = typeof(UnityEngine.Transform).GetProperty("position").GetGetMethod();
        for (int i = 0; i < list.Count; i++)
        {
            yield return list[i];
            // Only the player's culling origin changes. Wall positions and
            // enemy proximity remain the game's own values. No object is
            // activated by the helper and normal play keeps the original origin.
            if (i >= 2 && Equals(list[i].operand, position) && Equals(list[i - 2].operand, body))
                yield return new CodeInstruction(OpCodes.Call, typeof(FreeCam).GetMethod("VisibilityOrigin"));
        }
    }

    private static IEnumerable<CodeInstruction> CaptureFinish(IEnumerable<CodeInstruction> instructions)
    {
        var loadScene = typeof(UnityEngine.SceneManagement.SceneManager).GetMethod("LoadScene", new[] { typeof(string) });
        var completionScene = typeof(SpeedrunManager).GetMethod("LoadCompletionScene");
        foreach (CodeInstruction instruction in instructions)
        {
            if (instruction.opcode == OpCodes.Call && Equals(instruction.operand, loadScene))
                instruction.operand = completionScene;
            yield return instruction;
            if (instruction.opcode == OpCodes.Stfld && Equals(instruction.operand, objectives))
            {
                yield return new CodeInstruction(OpCodes.Ldarg_0);
                yield return new CodeInstruction(OpCodes.Call,
                                                 typeof(SpeedrunManager).GetMethod("ObjectivesObserved"));
            }
        }
    }

    private static void PlayerReady(MouseLook __instance)
    {
        PlayerContext.Observed = __instance;
        SeedMapPlanner.AfterStart(__instance);
        if (SpeedrunManager.Instance != null)
            SpeedrunManager.Instance.PlayerReady(__instance);
    }

    private static bool Death(ref IEnumerator __result)
    {
        if (!SpeedrunManager.IsSpeedrunActive || !SpeedrunManager.Instance.CanHandleDeath)
            return true;
        SpeedrunManager.Instance.NotifyDeath();
        __result = Empty();
        return false;
    }

    private static IEnumerator Empty()
    {
        yield break;
    }
}
