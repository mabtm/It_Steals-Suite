using System;

// Shared by the real GPU adapter and host tests. The wire is always top-down
// BGRA. Infer the async row order against Unity ReadPixels once per device.
public static class ClipPixelLayout {
    public static bool InferBottomUp(byte[] asyncBytes,byte[] readPixels,out bool bottomUp){
        bottomUp=false;if(asyncBytes==null||readPixels==null||asyncBytes.Length!=16||readPixels.Length!=16)return false;
        bool same=true,flipped=true;for(int i=0;i<16;i++){same&=asyncBytes[i]==readPixels[i];flipped&=asyncBytes[i]==readPixels[(1-i/8)*8+i%8];}
        if(same==flipped)return false;bottomUp=same;return true;
    }
    public static void TopDownBgra(byte[] source,byte[] destination,int width,int height,bool bottomUp,bool bgra){
        int stride=checked(width*4),size=checked(stride*height);
        if(width<1||height<1||source==null||destination==null||source.Length!=size||destination.Length!=size||ReferenceEquals(source,destination))throw new ArgumentException("Invalid clip pixel buffer");
        for(int y=0;y<height;y++)Buffer.BlockCopy(source,(bottomUp?height-1-y:y)*stride,destination,y*stride,stride);
        if(!bgra)for(int i=0;i<size;i+=4){byte r=destination[i];destination[i]=destination[i+2];destination[i+2]=r;}
    }
}
