using System;
using System.Diagnostics;
using System.IO.MemoryMappedFiles;
using System.Reflection;
public static class GenerationBufferBenchmark
{
    public static void Main()
    {
        foreach(int points in new[]{32,64,128,256,512,1024})
        {
            const int count=50000;const ulong key=0x0123456789abcdefUL;
            var rng=new Random(177);Func<int,int,int> range=(min,max)=>rng.Next(min,max);
            var order=new RandomSeedOrder(key);var generator=new SpawnSelectionGenerator(8,points,14);
            var identity=new SeedSearchBatch {Pid=998877,Scene=3,Epoch=1,RequestSerial=100,Seed=order.SeedAt(0),Recipe=1,Total=count,SearchKey=key,Hash=new string('a',64)};
#if SEARCHLAB_BUFFER
            var writer=new SeedSearchBatchWriter(identity,14);var selected=new int[14];
#endif
            using(var channel=new PracticeMapChannel(998877,55)) {
                var view=(MemoryMappedViewAccessor)typeof(PracticeMapChannel).GetField("view",BindingFlags.Instance|BindingFlags.NonPublic).GetValue(channel);
                GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();int collections=GC.CollectionCount(0);ulong done=0,checksum=0,frames=0;
                var clock=Stopwatch.StartNew();
                while(done<count) {
#if SEARCHLAB_BUFFER
                    writer.Begin(done);int rows=0;
                    while(done<count&&rows<SeedSearchBatch.Maximum) {
                        int seed=order.SeedAt((uint)done);int spawn=generator.NextInto(-1,range,selected);writer.Add(seed,spawn,selected);
                        checksum=unchecked(checksum*31+(uint)spawn);foreach(int id in selected)checksum=unchecked(checksum*31+(uint)id);
                        done++;rows++;
                    }
                    int length;byte[] bytes=writer.FinishView(0,++frames,out length);if(!channel.TryPublishBatch(bytes,length))throw new Exception("Unexpected backpressure");
#else
                    var batch=new SeedSearchBatch {Pid=identity.Pid,Scene=identity.Scene,Epoch=identity.Epoch,RequestSerial=identity.RequestSerial,Seed=identity.Seed,Recipe=1,Total=count,Offset=done,SearchKey=key,Hash=identity.Hash};
                    while(done<count&&batch.Layouts.Count<SeedSearchBatch.Maximum) {
                        int seed=order.SeedAt((uint)done);var selection=generator.Next(-1,range);batch.Layouts.Add(new SeedSearchLayout {Seed=seed,Selection=selection});
                        checksum=unchecked(checksum*31+(uint)selection.Spawn);foreach(int id in selection.Crystals)checksum=unchecked(checksum*31+(uint)id);
                        done++;
                    }
                    batch.Status=done==count?1:0;if(!channel.TryPublishBatch(batch.Encode()))throw new Exception("Unexpected backpressure");frames++;
#endif
                    view.Write(12,view.ReadUInt32(4));
                }
                clock.Stop();Console.WriteLine("points={0}, {1:F0} layouts/s, {2:F4} s, Gen0={3}, packets={4}, checksum={5}",points,count/clock.Elapsed.TotalSeconds,clock.Elapsed.TotalSeconds,GC.CollectionCount(0)-collections,frames,checksum);
            }
        }
        Console.WriteLine("System.Random selection + actual Mono shared packet buffer/ACK; no Unity InitState, game frames or native route calculation.");
    }
}
