using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

public struct SeedSearchLayout
{
    public int Seed;
    public SpawnSelection Selection;
}
// The actual seed accompanies every layout; packet offsets only track progress.
public sealed class SeedSearchBatch
{
    public const int Maximum=2048;
    public int Pid,Scene,Epoch,Seed,Recipe,Status;
    public uint RequestSerial;
    public ulong Total,Offset,SearchKey,GenerationMicros,GenerationFrames;
    public string Hash="",Message="";
    public readonly List<SeedSearchLayout> Layouts=new List<SeedSearchLayout>(Maximum);
    public byte[] Encode()
    {
        if(Total<1||Total>4294967296UL||Offset>Total||Layouts.Count>Maximum||
           (ulong)Layouts.Count>Total-Offset||Status<0||Status>3||
           (Status==0&&Layouts.Count==0)||(Status==1&&Offset+(ulong)Layouts.Count!=Total))
            throw new InvalidDataException("Invalid seed batch");
        var order=new RandomSeedOrder(SearchKey);
        if(Seed!=order.SeedAt(0))throw new InvalidDataException("Invalid search identity");
        using(var stream=new MemoryStream())using(var writer=new BinaryWriter(stream,Encoding.UTF8,true))
        {
            writer.Write(0x42535349u);writer.Write(3);
            writer.Write(Pid);writer.Write(Scene);writer.Write(Epoch);writer.Write(RequestSerial);
            writer.Write(Seed);writer.Write(Recipe);writer.Write(Total);writer.Write(Offset);writer.Write(Status);writer.Write(SearchKey);
            writer.Write(GenerationMicros);writer.Write(GenerationFrames);
            Text(writer,Hash);Text(writer,Message);writer.Write(Layouts.Count);
            for(int i=0;i<Layouts.Count;i++)
            {
                var layout=Layouts[i];
                if(layout.Selection==null||layout.Seed!=order.SeedAt(unchecked((uint)(Offset+(ulong)i))))
                    throw new InvalidDataException("Layout seed does not match random search order");
                writer.Write(layout.Seed);writer.Write(layout.Selection.Spawn);writer.Write(layout.Selection.Crystals.Length);
                foreach(int id in layout.Selection.Crystals)writer.Write(id);
            }
            writer.Flush();return stream.ToArray();
        }
    }
    private static void Text(BinaryWriter writer,string text)
    {byte[] bytes=Encoding.UTF8.GetBytes(text??"");writer.Write(bytes.Length);writer.Write(bytes);}
}

// One reusable buffer per search. No layout objects, arrays, MemoryStreams or
// BinaryWriters are created in the per-seed generation loop. The channel copies
// the finished view synchronously; only then may the next batch reuse it.
public sealed class SeedSearchBatchWriter
{
    private readonly byte[] buffer;
    private readonly int crystalCount,dataStart,countPosition;
    private readonly ulong total;
    private int position,count;
    private ulong offset;
    public SeedSearchBatchWriter(SeedSearchBatch identity,int crystals)
    {
        if(identity==null||crystals<1||crystals>64||identity.Total<1||identity.Total>4294967296UL)
            throw new ArgumentException("Invalid search buffer identity");
        byte[] hash=Encoding.UTF8.GetBytes(identity.Hash??"");
        if(hash.Length>65)throw new ArgumentException("Invalid catalog hash");
        crystalCount=crystals;total=identity.Total;
        buffer=new byte[76+4+hash.Length+4+4+SeedSearchBatch.Maximum*(12+4*crystals)];
        Write32(0x42535349u);Write32(3);Write32((uint)identity.Pid);Write32((uint)identity.Scene);
        Write32((uint)identity.Epoch);Write32(identity.RequestSerial);Write32((uint)identity.Seed);Write32((uint)identity.Recipe);
        Write64(total);Write64(0);Write32(0);Write64(identity.SearchKey);Write64(0);Write64(0);
        Write32((uint)hash.Length);Buffer.BlockCopy(hash,0,buffer,position,hash.Length);position+=hash.Length;
        Write32(0);countPosition=position;Write32(0);dataStart=position;
    }
    private void At32(int at,uint value)
    {buffer[at]=(byte)value;buffer[at+1]=(byte)(value>>8);buffer[at+2]=(byte)(value>>16);buffer[at+3]=(byte)(value>>24);}
    private void At64(int at,ulong value)
    {At32(at,unchecked((uint)value));At32(at+4,(uint)(value>>32));}
    private void Write32(uint value){At32(position,value);position+=4;}
    private void Write64(ulong value){At64(position,value);position+=8;}
    public void Begin(ulong start)
    {if(start>=total)throw new ArgumentOutOfRangeException("start");offset=start;position=dataStart;count=0;At64(40,start);}
    public void Add(int seed,int spawn,int[] crystals)
    {
        if(count>=SeedSearchBatch.Maximum||(ulong)count>=total-offset||crystals==null||crystals.Length!=crystalCount)
            throw new InvalidDataException("Search buffer overflow");
        Write32(unchecked((uint)seed));Write32(unchecked((uint)spawn));Write32((uint)crystalCount);
        if(BitConverter.IsLittleEndian){int length=crystalCount*4;Buffer.BlockCopy(crystals,0,buffer,position,length);position+=length;}
        else for(int i=0;i<crystalCount;i++)Write32(unchecked((uint)crystals[i]));
        count++;
    }
    public byte[] Finish(ulong micros,ulong frames)
    {
        int length;byte[] source=FinishView(micros,frames,out length);
        var bytes=new byte[length];Buffer.BlockCopy(source,0,bytes,0,length);return bytes;
    }
    // Production publishes this view synchronously before Begin/Add reuse it.
    public byte[] FinishView(ulong micros,ulong frames,out int length)
    {
        if(count==0)throw new InvalidDataException("Empty search buffer");
        At32(48,offset+(ulong)count==total?1u:0u);At64(60,micros);At64(68,frames);At32(countPosition,(uint)count);
        length=position;return buffer;
    }
}
