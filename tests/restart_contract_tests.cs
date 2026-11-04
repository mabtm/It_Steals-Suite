using System;
using System.Runtime.InteropServices;

public static class RestartContractTests
{
    private static void Check(bool condition, string name)
    {
        if (!condition)
            throw new Exception(name);
    }

    public static int Main()
    {
        var oldTrainer = new TrainerCommands { SpeedrunModeActive = 1, TelemetryOptions = 0x80000000u };
        Check(oldTrainer.LegacyRestartAllowed(true), "older trainer keeps the game's T fallback");
        var currentTrainer = oldTrainer;
        currentTrainer.TelemetryOptions |= 8192u;
        Check(!currentTrainer.LegacyRestartAllowed(true),
              "current trainer suppresses T fallback so one press cannot reload twice");
        // Ownership is independent of the chosen key and remains set when it is
        // unassigned or input is temporarily blocked by typing or key capture.
        currentTrainer.InputCapture = 1;
        Check(!currentTrainer.LegacyRestartAllowed(true), "text entry does not reactivate T");
        currentTrainer.InputCapture = 0;
        Check(!currentTrainer.LegacyRestartAllowed(true), "rebinding or unassigning does not reactivate T");
        Check(currentTrainer.LegacyRestartAllowed(false), "a lost trainer connection restores legacy control");
        Check(new TrainerCommands().LegacyRestartAllowed(false), "standalone mod retains its restart key");
        Check(Marshal.SizeOf(typeof(TrainerCommands)) == 192 &&
                  Marshal.OffsetOf(typeof(TrainerCommands), "TelemetryOptions").ToInt32() == 44 &&
                  Marshal.OffsetOf(typeof(TrainerCommands), "SpeedrunLoadLevelCmd").ToInt32() == 25,
              "restart ownership reuses an option bit without moving command fields");
        currentTrainer.SpeedrunLoadLevelCmd = 1;
        Check(currentTrainer.Speedrun && currentTrainer.SpeedrunLoadLevelCmd == 1,
              "trainer restart uses the existing speedrun reload command");
        Console.WriteLine("Restart ownership, old-trainer fallback, disconnected fallback and command compatibility: PASS");
        return 0;
    }
}
