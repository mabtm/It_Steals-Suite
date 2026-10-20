using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;
using HarmonyLib;
using UnityEngine;

// TimeScale zero does not stop Update or yield-null game coroutines. Preserve
// iterator state while the analysis view is frozen; never stop/restart them.
public static class ReplayGamePause
{
    public static bool Frozen;

    public static bool AllowUpdate()
    {
        return !Frozen;
    }

    public static bool AllowCoroutine(ref bool __result)
    {
        if (!Frozen)
            return true;
        __result = true;
        return false;
    }

    private static readonly BindingFlags declared =
        BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.DeclaredOnly;

    public static bool IsUpdate(MethodInfo method)
    {
        return method != null && !method.IsAbstract && method.ReturnType == typeof(void) &&
               method.GetParameters().Length == 0 &&
               (method.Name == "Update" || method.Name == "LateUpdate" || method.Name == "FixedUpdate");
    }

    public static IEnumerable<MethodInfo> Updates()
    {
        // These two Update methods only clear render command buffers and resize
        // camera render targets. Pausing them breaks original light shafts.
        foreach (var type in typeof(MouseLook).Assembly.GetTypes())
            if (typeof(MonoBehaviour).IsAssignableFrom(type) && type != typeof(SteamManager) &&
                type != typeof(VolumetricLight) && type != typeof(VolumetricLightRenderer))
                foreach (var method in type.GetMethods(declared))
                    if (IsUpdate(method))
                        yield return method;
    }

    public static IEnumerable<MethodInfo> Coroutines()
    {
        foreach (var type in typeof(MouseLook).Assembly.GetTypes())
            if (typeof(IEnumerator).IsAssignableFrom(type) && type.DeclaringType != null &&
                typeof(MonoBehaviour).IsAssignableFrom(type.DeclaringType) &&
                type.DeclaringType != typeof(SteamManager))
            {
                var method = type.GetMethod("MoveNext", declared);
                if (method != null && method.ReturnType == typeof(bool) && method.GetParameters().Length == 0)
                    yield return method;
            }
    }

    public static void Install(HarmonyLib.Harmony harmony)
    {
        foreach (var method in Updates())
            harmony.Patch(method, prefix: new HarmonyMethod(typeof(ReplayGamePause), "AllowUpdate"));
        foreach (var method in Coroutines())
            harmony.Patch(method, prefix: new HarmonyMethod(typeof(ReplayGamePause), "AllowCoroutine"));
    }

    public static IEnumerable<GameObject> CulledGeometry(HideAndSeekIt actor)
    {
        // These are the exact catalogs used by OccludeWalls/OccludeObjects.
        // Never activate every inactive object: that would enable unused level
        // variants, collected objectives and gameplay actors.
        foreach (string name in new[] { "AllWalls", "AllProps", "CielingLights" })
        {
            var field = typeof(HideAndSeekIt).GetField(name, declared);
            if (field == null || field.FieldType != typeof(GameObject[]))
                throw new InvalidOperationException("Replay culling catalog unavailable: " + name);
            var objects = (GameObject[])field.GetValue(actor);
            if (objects == null)
                continue;
            foreach (var obj in objects)
                if (obj != null)
                    yield return obj;
        }
    }
}
