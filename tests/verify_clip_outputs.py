"""Independent output checks. Requires Pillow/numpy; optional native FFmpeg encoder."""
from pathlib import Path
import argparse,json,subprocess,shlex
import numpy as np
from PIL import Image
parser=argparse.ArgumentParser()
parser.add_argument('camera_output',type=Path,help='Output directory of clip_camera_tests.cpp')
parser.add_argument('--encoder',type=Path,help='A native FFmpeg 8.1.3 build with the Encoder/build-encoder.sh configuration')
args=parser.parse_args();folder=args.camera_output
expected=np.frombuffer((folder/'expected.bgra').read_bytes(),np.uint8).reshape(180,320,4)[:,:,[2,1,0,3]]
images=sorted((folder/'rgba.frames').glob('*.png'));assert len(images)==3
for file in images:
    with Image.open(file) as check: check.verify()
    with Image.open(file) as check: assert check.mode=='RGBA' and np.array_equal(np.asarray(check),expected)
manifest=json.loads((folder/'rgba.frames/sequence.json').read_text())
assert manifest['fps']==30 and manifest['frames']==3 and manifest['alpha']=='straight'
print('PNG: all RGBA pixels/alpha, frame count and manifest exact: PASS')
if args.encoder:
    for fps in (30,60,120):
        count = fps + 7
        # Each frame includes a unique changing marker and arbitrary RGBA/alpha values.
        pixels=((np.arange(count*320*180*4,dtype=np.uint64)*37+np.arange(count*320*180*4,dtype=np.uint64)//11*13)%256).astype(np.uint8).reshape(count,180,320,4)
        for i in range(count): pixels[i,0,0]=[i,255-i,i^165,128+i]
        assert len({tuple(frame[0,0]) for frame in pixels})==count
        raw=pixels.tobytes()
        destination=folder/f'ffv1-{fps}.mkv'
        codec=shlex.split((folder/f'ffv1-args-{fps}.txt').read_text())
        subprocess.run([str(args.encoder.resolve()),*codec,str(destination)],input=raw,check=True)
        decoded=subprocess.check_output(['ffmpeg','-v','error','-i',str(destination),'-f','rawvideo','-pix_fmt','bgra','-'])
        assert decoded==raw, 'A changed/missing/duplicated frame or RGB/alpha mismatch'
        probe=json.loads(subprocess.check_output(['ffprobe','-v','error','-count_frames','-show_streams','-show_frames','-of','json',str(destination)]))
        stream=probe['streams'][0];frames=probe['frames']
        assert stream['codec_name']=='ffv1' and stream['pix_fmt']=='bgra' and int(stream['nb_read_frames'])==count
        assert stream['r_frame_rate']==f'{fps}/1' and stream['avg_frame_rate']==f'{fps}/1'
        pts=[float(f['best_effort_timestamp_time']) for f in frames]
        assert len(pts)==count and all(a<b for a,b in zip(pts,pts[1:]))
        error=max(abs(t-i/fps) for i,t in enumerate(pts))
        assert error <= .0005001 # Matroska uses millisecond timestamps.
        print(f'FFV1 production arguments / independent decode: {fps} FPS, {count}/{count} unique frames, exact RGB+alpha, PTS error {error*1000:.4f} ms: PASS')
