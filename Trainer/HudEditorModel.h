#pragma once
#include <algorithm>
#include <cmath>
namespace suite_hud {
struct Rect {float x=0,y=0,w=0,h=0;bool Contains(float px,float py)const{return px>=x&&py>=y&&px<=x+w&&py<=y+h;}};
struct Canvas {bool active=false;float x=0,y=0,w=0,h=0,scale=1;};
inline Canvas canvas;
inline bool active=false;
inline Rect bounds[3];
inline float Fraction(float at,float extent,float panel,float margin){return std::clamp((at-margin)/std::max(1.f,extent-panel-2*margin),0.f,1.f);}
inline float Snap(float at,float size,float extent,float margin,float threshold){for(float target:{margin,(extent-size)*.5f,extent-size-margin})if(std::abs(at-target)<=threshold)return target;return at;}
inline Rect Move(Rect r,float dx,float dy,float width,float height,float margin,bool snap,float distance){r.x+=dx;r.y+=dy;if(snap){r.x=Snap(r.x,r.w,width,margin,distance);r.y=Snap(r.y,r.h,height,margin,distance);}r.x=std::clamp(r.x,margin,std::max(margin,width-margin-r.w));r.y=std::clamp(r.y,margin,std::max(margin,height-margin-r.h));return r;}
}
