#pragma once
#include "imgui.h"
#include <algorithm>
#include <cmath>

inline void DrawPracticeRouteLeg(ImDrawList* draw,ImVec2 a,ImVec2 b,ImU32 color,float width,bool estimated) {
    if(!estimated){draw->AddLine(a,b,color,width);return;}
    float dx=b.x-a.x,dy=b.y-a.y,length=std::hypot(dx,dy);if(length<.001f)return;
    // Bounded draw work even at unusually large map zooms.
    float stride=std::max(10.f,length/256.f);
    for(float at=0;at<length;at+=stride){float end=std::min(length,at+stride*.6f);draw->AddLine(ImVec2(a.x+dx*at/length,a.y+dy*at/length),ImVec2(a.x+dx*end/length,a.y+dy*end/length),color,width);}
}
