#pragma once
#include <algorithm>
#include <cmath>
#include <ostream>
#include <string>
struct ExperiencePreferences {
    bool sounds=true,motion=true,launchGraphic=true;
    float volume=.16f;
    void Validate(){volume=std::isfinite(volume)?std::clamp(volume,0.f,1.f):.16f;}
    void Encode(std::ostream& out)const{out<<"experience.sounds="<<sounds<<"\nexperience.motion="<<motion<<"\nexperience.launchGraphic="<<launchGraphic<<"\nexperience.volume="<<volume<<'\n';}
    void Decode(const std::string& key,double value){if(key=="volume")volume=float(std::clamp(value,0.,1.));else if(value==0||value==1){if(key=="sounds")sounds=value==1;else if(key=="motion")motion=value==1;else if(key=="launchGraphic")launchGraphic=value==1;}}
};
