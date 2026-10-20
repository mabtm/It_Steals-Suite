using System;

// Exact 2D segment-to-rectangle distance, including rounded player clearance.
// Unlike expanding an AABB, this does not close a valid diagonal corner gap.
public static class PlayerWallLeg
{
    private static bool Clip(double a,double b,double half,ref double lo,ref double hi)
    {
        double delta=b-a;if(Math.Abs(delta)<1e-12)return a>=-half&&a<=half;
        double p=(-half-a)/delta,q=(half-a)/delta;lo=Math.Max(lo,Math.Min(p,q));hi=Math.Min(hi,Math.Max(p,q));return lo<=hi;
    }
    private static double PointSegment(double x,double z,double ax,double az,double bx,double bz)
    {
        double dx=bx-ax,dz=bz-az,length=dx*dx+dz*dz;
        double t=length>0?Math.Max(0,Math.Min(1,((x-ax)*dx+(z-az)*dz)/length)):0;
        dx=x-ax-t*dx;dz=z-az-t*dz;return dx*dx+dz*dz;
    }
    public static bool Blocks(double ax,double az,double bx,double bz,double halfX,double halfZ,double radius)
    {
        double lo=0,hi=1;
        if(Clip(ax,bx,halfX,ref lo,ref hi)&&Clip(az,bz,halfZ,ref lo,ref hi))return true;
        double dx=Math.Max(0,Math.Abs(ax)-halfX),dz=Math.Max(0,Math.Abs(az)-halfZ),best=dx*dx+dz*dz;
        dx=Math.Max(0,Math.Abs(bx)-halfX);dz=Math.Max(0,Math.Abs(bz)-halfZ);best=Math.Min(best,dx*dx+dz*dz);
        for(int x=-1;x<=1;x+=2)for(int z=-1;z<=1;z+=2)best=Math.Min(best,PointSegment(x*halfX,z*halfZ,ax,az,bx,bz));
        return best<radius*radius;
    }
}

