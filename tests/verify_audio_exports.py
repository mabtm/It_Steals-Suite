from pathlib import Path
import subprocess,shlex,json,hashlib,struct
import argparse
parser=argparse.ArgumentParser(description="Validate new recorded-audio exports with a matching FFmpeg build and independent ffprobe/ffmpeg.")
parser.add_argument("fixtures",type=Path,help="fresh output folder from replay_experience_tests.cpp")
parser.add_argument("ffmpeg",type=Path,help="encoder built from Encoder sources")
args=parser.parse_args();ff=args.ffmpeg.resolve();out=args.fixtures.resolve();audit=[]
raw=bytes(((x//4)*7+(x%4)*47)&255 for x in range(320*180*4));raw=raw*30
for fmt,ext in [(2,'mkv'),(4,'avi'),(5,'mov'),(6,'mp4')]:
 v=out/('video.'+ext);final=out/('mixed.'+ext)
 args=shlex.split((out/f'format-{fmt}.args').read_text());subprocess.run([str(ff),*args,str(v)],input=raw,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
 codec=['-c:a','aac','-b:a','128k'] if fmt==6 else ['-c:a','pcm_f32le']
 subprocess.run([str(ff),'-hide_banner','-nostdin','-loglevel','error','-y','-xerror','-i',str(v),'-i',str(out/'audio-1x.wav'),'-map','0:v:0','-map','1:a:0','-c:v','copy',*codec,str(final)],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
 probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-count_frames','-show_streams','-show_format','-of','json',str(final)]));vs=next(s for s in probe['streams'] if s['codec_type']=='video');a=next(s for s in probe['streams'] if s['codec_type']=='audio');assert vs['avg_frame_rate']=='60/1' and int(vs['nb_read_frames'])==27,(fmt,vs)
 assert int(a['sample_rate'])==48000 and int(a['channels'])==2
 decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(final),'-map','0:a:0','-f','f32le','-acodec','pcm_f32le','pipe:1']);expected=(out/'audio-1x.wav').read_bytes()[44:]
 if fmt!=6:assert decoded==expected,(fmt,len(decoded),len(expected))
 subprocess.run(['ffmpeg','-v','error','-xerror','-i',str(final),'-f','null','-'],check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
 audit.append({'format':fmt,'videoFrames':vs['nb_read_frames'],'fps':vs['avg_frame_rate'],'audio':a['codec_name'],'audioRate':a['sample_rate'],'pcmExact':fmt!=6,'bytes':final.stat().st_size,'duration':probe['format']['duration']})
# Repeat the production two-pass job with the audio bit budget included.
cap=256000;seconds=.45;bitrate=int((cap-65536)*8/seconds-128000);target=out/'size-limit-audio.mp4'
for pass_id in (1,2):
 args=[str(ff),'-hide_banner','-nostdin','-loglevel','error','-y','-xerror','-i',str(out/'video.avi')]
 if pass_id==2:args+=['-i',str(out/'audio-1x.wav'),'-map','0:v:0','-map','1:a:0','-c:a','aac','-b:a','128k']
 else:args+=['-map','0:v:0','-an']
 args+=['-c:v','libx264','-preset','medium','-b:v',str(bitrate),'-pass',str(pass_id),'-passlogfile',str(out/'size-pass'),'-frames:v','27','-r','60','-enc_time_base','1:60','-pix_fmt','yuv420p','-threads','4']
 args+=['-f','null','/dev/null'] if pass_id==1 else ['-t','0.450000','-movflags','+faststart','-f','mp4',str(target)]
 subprocess.run(args,check=True,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE)
probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-count_frames','-show_streams','-show_format','-of','json',str(target)]));assert len(probe['streams'])==2 and target.stat().st_size<=cap;assert probe['streams'][0]['nb_read_frames']=='27';assert abs(float(probe['format']['duration'])-.45)<.001
audit.append({'preset':'size-limit','bytes':target.stat().st_size,'cap':cap,'audio':probe['streams'][1]['codec_name'],'frames':probe['streams'][0]['nb_read_frames'],'duration':probe['format']['duration']})
(out/'verified-audio-exports.json').write_text(json.dumps(audit,indent=2));print(json.dumps(audit))
