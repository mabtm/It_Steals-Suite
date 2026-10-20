from pathlib import Path
import struct
import numpy as np
from PIL import Image

def render(path):
    raw=path.read_bytes();offset=0
    def take(fmt):
        nonlocal offset
        values=struct.unpack_from(fmt,raw,offset);offset+=struct.calcsize(fmt)
        return values
    w,h,aw,ah,count=take('<5I')
    atlas=np.frombuffer(raw,np.uint8,count=aw*ah,offset=offset).reshape(ah,aw).astype(np.float32)/255;offset+=aw*ah
    canvas=np.empty((h,w,3),np.float32);canvas[:]=(.035,.045,.06)
    for _ in range(count):
        nv,ni,nc=take('<3I')
        vertices=np.frombuffer(raw,dtype=np.dtype([('pos','<f4',(2,)),('uv','<f4',(2,)),('color','<u4')]),count=nv,offset=offset);offset+=nv*20
        indices=np.frombuffer(raw,dtype='<u4',count=ni,offset=offset);offset+=ni*4
        for _ in range(nc):
            x0,y0,x1,y1,n,start,base=take('<4f3I')
            clip=(max(0,int(x0)),max(0,int(y0)),min(w,int(x1)),min(h,int(y1)))
            for i in range(start,start+n,3):
                vs=vertices[indices[i:i+3]+base];p=vs['pos'].astype(np.float64)
                left=max(clip[0],int(np.floor(p[:,0].min())));right=min(clip[2],int(np.ceil(p[:,0].max())))
                top=max(clip[1],int(np.floor(p[:,1].min())));bottom=min(clip[3],int(np.ceil(p[:,1].max())))
                if right<=left or bottom<=top:continue
                a,b,c=p;den=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
                if abs(den)<1e-9:continue
                xx,yy=np.meshgrid(np.arange(left,right)+.5,np.arange(top,bottom)+.5)
                u=((b[1]-c[1])*(xx-c[0])+(c[0]-b[0])*(yy-c[1]))/den
                v=((c[1]-a[1])*(xx-c[0])+(a[0]-c[0])*(yy-c[1]))/den;t=1-u-v
                # Half-open shared-edge rule prevents blending a diagonal twice.
                mask=(u>=0)&(v>=0)&(t>=0)
                if not mask.any():continue
                weights=np.stack([u[mask],v[mask],t[mask]],axis=1)
                uv=weights@vs['uv'];tx=np.clip(uv[:,0]*aw-.5,0,aw-1);ty=np.clip(uv[:,1]*ah-.5,0,ah-1)
                ix=tx.astype(int);iy=ty.astype(int);fx=tx-ix;fy=ty-iy;jx=np.minimum(ix+1,aw-1);jy=np.minimum(iy+1,ah-1)
                alpha=atlas[iy,ix]*(1-fx)*(1-fy)+atlas[iy,jx]*fx*(1-fy)+atlas[jy,ix]*(1-fx)*fy+atlas[jy,jx]*fx*fy
                colors=np.stack([(vs['color']>>shift)&255 for shift in (0,8,16,24)],axis=1)/255
                color=weights@colors;alpha*=color[:,3]
                region=canvas[top:bottom,left:right];region[mask]=color[:,:3]*alpha[:,None]+region[mask]*(1-alpha[:,None])
    assert offset==len(raw)
    Image.fromarray(np.clip(canvas*255,0,255).astype(np.uint8)).save(path.with_suffix('.png'))

if __name__ == '__main__':
    import argparse
    parser=argparse.ArgumentParser(description='Render the real ImGui test draw output with its captured alpha font atlas.')
    parser.add_argument('draw',nargs='+',type=Path)
    for path in parser.parse_args().draw:
        render(path)
        print(path.with_suffix('.png'))
