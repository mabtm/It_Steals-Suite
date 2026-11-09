#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>

namespace suite_full
{
struct Preferences
{
    int mode=0;
    std::array<int,8> seeds{123,123,123,123,123,123,123,123};
    uint32_t raw=0;
    bool overlay=true;
    float x=0,y=.35f,width=400,scale=1,opacity=.92f;
    void Validate()
    {
        mode=std::clamp(mode,0,2);raw&=255u;
        auto bound=[](float v,float a,float b,float fallback){return std::isfinite(v)?std::clamp(v,a,b):fallback;};
        x=bound(x,0,1,0);y=bound(y,0,1,.35f);width=bound(width,260,600,400);
        scale=bound(scale,.6f,1.8f,1);opacity=bound(opacity,.1f,1,.92f);
    }
    void Encode(std::ostream &out) const
    {
        out<<"fullgame.mode="<<mode<<"\nfullgame.raw="<<raw<<"\nfullgame.overlay="<<int(overlay)
            <<"\nfullgame.x="<<x<<"\nfullgame.y="<<y<<"\nfullgame.width="<<width<<"\nfullgame.scale="<<scale<<"\nfullgame.opacity="<<opacity<<'\n';
        for(int i=0;i<8;i++)out<<"fullgame.seed"<<i<<'='<<seeds[i]<<'\n';
    }
    void Decode(const std::string &key,double value)
    {
        if(!std::isfinite(value))return;
        int number=int(std::clamp(value,-2147483648.,2147483647.));
        if(key=="mode")mode=std::clamp(number,0,2);
        else if(key=="raw")raw=uint32_t(std::clamp(number,0,255));
        else if(key=="overlay")overlay=value!=0;
        else if(key=="x")x=float(value);else if(key=="y")y=float(value);else if(key=="width")width=float(value);
        else if(key=="scale")scale=float(value);else if(key=="opacity")opacity=float(value);
        else if(key.size()==5 && key.starts_with("seed") && key[4]>='0' && key[4]<='7')seeds[size_t(key[4]-'0')]=number;
    }
};
inline Preferences preferences;
}
