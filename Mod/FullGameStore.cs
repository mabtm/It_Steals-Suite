using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using System.Threading;

// A sleeping, bounded IO worker owns full-game files and publishes an immutable
// PB index. Unity never waits for file reads, replacement or metadata scanning.
public sealed class FullGameStore : IDisposable
{
    private readonly string directory;
    private readonly object gate = new object();
    private readonly AutoResetEvent wake = new AutoResetEvent(false);
    private readonly Dictionary<long, FullGameRecord> pending = new Dictionary<long, FullGameRecord>();
    private readonly Dictionary<string, FullGameRecord> best = new Dictionary<string, FullGameRecord>(StringComparer.Ordinal);
    private Dictionary<string, FullGameRecord> published = new Dictionary<string, FullGameRecord>(StringComparer.Ordinal);
    private readonly Thread worker;
    private bool stopping, writing;
    private long lastId;
    public volatile bool Ready;
    public volatile string Error, Warning;

    public FullGameStore(string dataDirectory)
    {
        directory = Path.Combine(dataDirectory, "FullGame");
        worker = new Thread(Work) { IsBackground = true, Name = "Elite.FullGame", Priority = ThreadPriority.BelowNormal };
        worker.Start();
    }
    public long NextId(long utc)
    {
        lock (gate)
        {
            long next = Math.Max(utc, lastId + 1);
            if (next <= 0 || next > DateTime.MaxValue.Ticks) throw new InvalidOperationException("Full-game identity limit reached");
            return lastId = next;
        }
    }
    public FullGameRecord Best(FullGamePlan plan)
    {
        FullGameRecord value;
        var view = Volatile.Read(ref published);
        return plan != null && view.TryGetValue(plan.Key, out value) ? value.Copy() : null;
    }
    public void Queue(FullGameRecord record)
    {
        if (record == null) return;
        FullGameFormat.Validate(record);
        var copy = record.Copy();
        lock (gate)
        {
            if (stopping) { Error = "Full-game result could not be queued during shutdown"; return; }
            if (!pending.ContainsKey(copy.Id) && pending.Count >= 128)
            { Error = "Full-game storage queue full; a result could not be saved"; return; }
            pending[copy.Id] = copy; wake.Set();
        }
    }
    private void Rank(FullGameRecord r)
    {
        if (!r.Ranked) return;
        FullGameRecord previous;
        if (!best.TryGetValue(r.Plan.Key, out previous) || r.Seconds < previous.Seconds ||
            r.Seconds == previous.Seconds && r.Id < previous.Id)
        {
            best[r.Plan.Key] = r.Copy();
            Volatile.Write(ref published, new Dictionary<string, FullGameRecord>(best, StringComparer.Ordinal));
        }
    }
    private static string Read(string file)
    {
        if (new FileInfo(file).Length > FullGameFormat.MaxBytes) throw new InvalidDataException("Full-game file exceeds limit");
        return File.ReadAllText(file, new UTF8Encoding(false, true));
    }
    private void Load()
    {
        Directory.CreateDirectory(directory);
        int count = 0;
        foreach (string path in Directory.EnumerateFiles(directory, "full-*.isfg"))
        {
            if (++count > 100000) { Warning = "Full-game archive scan limit reached; existing files are preserved"; break; }
            try
            {
                FullGameRecord record;
                try { record = FullGameFormat.Decode(Read(path)); }
                catch (NotSupportedException) { throw; }
                catch (Exception)
                { record = FullGameFormat.Decode(Read(path + ".bak")); Warning = "Recovered full-game metadata from a last-good backup"; }
                if (Path.GetFileName(path) != "full-" + record.Id.ToString(CultureInfo.InvariantCulture) + ".isfg")
                    throw new InvalidDataException("Full-game filename and identity differ");
                lock (gate) lastId = Math.Max(lastId, record.Id);
                Rank(record);
            }
            catch (Exception e) { Warning = "Some full-game metadata could not be loaded: " + e.Message; }
        }
    }
    private void Write(FullGameRecord record)
    {
        string target = Path.Combine(directory, "full-" + record.Id.ToString(CultureInfo.InvariantCulture) + ".isfg");
        string text = FullGameFormat.Encode(record);
        bool backup = true;
        if (File.Exists(target))
        {
            try
            {
                var previous = FullGameFormat.Decode(Read(target));
                if (previous.Id != record.Id || previous.Plan.Key != record.Plan.Key)
                    throw new NotSupportedException("Existing full-game identity preserved");
                if (previous.State == FullGameState.Completed && FullGameFormat.Encode(previous) != text)
                    throw new NotSupportedException("Completed full-game result is immutable");
            }
            catch (NotSupportedException) { throw; }
            catch (Exception)
            {
                backup = false;
                File.Copy(target, target + ".corrupt-" + DateTime.UtcNow.Ticks.ToString(CultureInfo.InvariantCulture), false);
            }
        }
        File.WriteAllText(target + ".tmp", text, new UTF8Encoding(false));
        if (File.Exists(target)) File.Replace(target + ".tmp", target, backup ? target + ".bak" : null);
        else File.Move(target + ".tmp", target);
        Rank(record);
    }
    private void Work()
    {
        try { Load(); }
        catch (Exception e) { Error = "Full-game history unavailable: " + e.Message; }
        finally { Ready = true; }
        for (;;)
        {
            FullGameRecord next = null;
            lock (gate)
            {
                foreach (var item in pending) { next = item.Value; break; }
                if (next != null) { pending.Remove(next.Id); writing = true; }
                else if (stopping) return;
            }
            if (next == null) { wake.WaitOne(); continue; }
            try
            {
                Directory.CreateDirectory(directory);
                for (int attempt = 0; ; attempt++)
                    try { Write(next); break; }
                    catch (IOException) { if (attempt >= 2) throw; Thread.Sleep(25); }
                Error = null;
            }
            catch (Exception e) { Error = "Full-game result not saved: " + e.Message; }
            finally { lock (gate) { writing = false; Monitor.PulseAll(gate); } }
        }
    }
    public bool Flush(int milliseconds = 5000)
    {
        var until = DateTime.UtcNow.AddMilliseconds(milliseconds);
        lock (gate)
        {
            while (pending.Count != 0 || writing)
            {
                var left = until - DateTime.UtcNow;
                if (left.TotalMilliseconds <= 0) return false;
                Monitor.Wait(gate, left);
            }
            return Error == null;
        }
    }
    public void Dispose()
    {
        lock (gate) { stopping = true; wake.Set(); }
        if (worker.Join(5000)) wake.Dispose();
    }
}
