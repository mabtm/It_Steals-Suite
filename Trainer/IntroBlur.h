#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>
namespace suite_ui {
// Three separable box passes approximate a soft Gaussian. Rolling sums keep
// the cost linear in pixels; all work is confined to one small startup crop.
inline bool BlurIntroPixels(std::vector<uint32_t>& pixels,int w,int h){
    if(w<1||h<1||w>512||h>256||pixels.size()!=size_t(w)*h)return false;
    std::vector<uint32_t> scratch(pixels.size());constexpr int radius=5,count=radius*2+1;
    for(int pass=0;pass<3;pass++)for(bool vertical:{false,true}){
        int lines=vertical?w:h,length=vertical?h:w;
        auto at=[&](int line,int n){return vertical?size_t(n)*w+line:size_t(line)*w+n;};
        for(int line=0;line<lines;line++){
            int b=0,g=0,r=0;auto add=[&](int n,int sign){uint32_t p=pixels[at(line,std::clamp(n,0,length-1))];b+=sign*int(p&255);g+=sign*int((p>>8)&255);r+=sign*int((p>>16)&255);};
            for(int n=-radius;n<=radius;n++)add(n,1);
            for(int n=0;n<length;n++){scratch[at(line,n)]=0xff000000u|uint32_t(r/count)<<16|uint32_t(g/count)<<8|uint32_t(b/count);add(n-radius,-1);add(n+radius+1,1);}
        }pixels.swap(scratch);
    }return true;
}
}
