from pathlib import Path
import json, subprocess, shlex, hashlib, time
import numpy as np
import argparse, os
parser=argparse.ArgumentParser(description="Independently verify production export arguments and complete size-limited clips")
parser.add_argument('cases',type=Path)
parser.add_argument('--encoder',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
base=args.cases.resolve();encoder=args.encoder.resolve();out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
cases=json.loads((base/'codec-cases.json').read_text())
results=[]
def raw_frames(count):
    y,x=np.indices((180,320)); frames=[]
    for n in range(count):
        p=np.empty((180,320,4),dtype=np.uint8)
        p[:,:,0]=(x*3+y+n*5)%256;p[:,:,1]=(x+y*2+n*7)%256;p[:,:,2]=(x*2+y*3+n*11)%256;p[:,:,3]=(x+y+n)%256
        for bit in range(8):p[:20,bit*40:(bit+1)*40,:3]=255 if (n>>bit)&1 else 0
        frames.append(p.tobytes())
    return b''.join(frames)
def run(args,raw=None):
    result=subprocess.run([str(encoder),*args],input=raw,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    if result.returncode:raise RuntimeError(result.stderr.decode(errors='replace')[-5000:])
    return result

def inspect(file,count,fps,raw=None,lossless=False,color=False):
    probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_streams','-show_format','-show_frames','-of','json',str(file)]))
    stream=probe['streams'][0]; frames=probe['frames']
    assert len(frames)==count,(file,len(frames),count)
    pts=[float(f['best_effort_timestamp_time']) for f in frames]
    assert abs(pts[0])<.0011
    assert all(b>a for a,b in zip(pts,pts[1:])),(file,pts)
    assert max(abs(t-n/fps) for n,t in enumerate(pts))<.0011,(file,pts[-1])
    duration=float(probe['format']['duration']);assert abs(duration-count/fps)<=max(.002,1/fps+.001),(file,duration,count/fps)
    num,den=map(int,stream['avg_frame_rate'].split('/'));assert abs(num/den-fps)<.001,(file,stream['avg_frame_rate'])
    if color:assert stream['color_space']==stream['color_transfer']==stream['color_primaries']=='bt709',stream
    decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(file),'-map','0:v:0','-vsync','0','-f','rawvideo','-pix_fmt','bgra','pipe:1'])
    size=320*180*4;assert len(decoded)==count*size
    hashes=[hashlib.sha256(decoded[i*size:(i+1)*size]).hexdigest() for i in range(count)]
    assert len(set(hashes))==count,(file,'duplicate frames')
    pixels=np.frombuffer(decoded,np.uint8).reshape(count,180,320,4)
    ids=[]
    for n in range(count):
        got=sum((1<<bit) for bit in range(8) if pixels[n,5:15,bit*40+12:bit*40+28,:3].mean()>127)
        ids.append(got)
    assert ids==list(range(count)),(file,'frame order',ids)
    if lossless:assert decoded==raw,(file,'lossless pixel mismatch')
    return {'frames':count,'fps':fps,'duration':duration,'expected':count/fps,'bytes':file.stat().st_size,'losslessExact':lossless,'orderedUniqueFrames':True,'pix_fmt':stream['pix_fmt']}
for case in cases:
    suffix={2:'.mkv',4:'.avi',5:'.mov',6:'.mp4'}[case['format']]
    file=out/(str(case['format'])+'-'+str(case['fps'])+suffix);raw=raw_frames(case['frames'])
    start=time.monotonic();run(shlex.split(case['arguments'])+[str(file)],raw);encoding=time.monotonic()-start
    result=inspect(file,case['frames'],case['fps'],raw,case['format'] in [2,4],case['format'] in [5,6]);result['name']=file.name;result['encodeSeconds']=encoding
    results.append(result);print(json.dumps(result),flush=True)
# Render once to an exact source; both rate-control passes read the same frames.
source=out/'budget-source.avi';raw=raw_frames(240)
source_case=next(c for c in cases if c['format']==4 and c['fps']==60)
args=source_case['arguments'].replace('-frames:v '+str(source_case['frames']),'-frames:v 240')
run(shlex.split(args)+[str(source)],raw)
for case in json.loads((base/'budget-cases.json').read_text()):
    target=out/('budget-'+str(case['cap'])+'.mp4');stats=out/('pass-'+str(case['cap']))
    common=['-hide_banner','-nostdin','-loglevel','error','-y','-filter_threads','1','-xerror','-i',str(source),'-map','0:v:0','-an','-passlogfile',str(stats)]
    start=time.monotonic()
    run(common+shlex.split(case['first'])+['NUL' if os.name=='nt' else '/dev/null']);run(common+shlex.split(case['second'])+[str(target)])
    assert target.stat().st_size<=case['cap'],(target,target.stat().st_size,case['cap'])
    result=inspect(target,240,60,color=True);result.update(name=target.name,cap=case['cap'],testSeconds=time.monotonic()-start)
    results.append(result);print(json.dumps(result),flush=True)
(out/'codec-verification.json').write_text(json.dumps(results,indent=2))
print('PASS: independent ffprobe/FFmpeg decoder; all frame IDs, PTS, durations, 30/60/120fps, exact lossless RGBA, BT709 metadata, size-limited complete clips.')
