#pragma once
#include "MinimapGeometry.h"
#include "MapLevels.h"
#include "RouteDrawing.h"
#include "SuiteAppearance.h"

inline void RenderMinimapWindow(float px, float pz, float rotY, float monX, float monZ,
    int crystalCount, int wallCount, int pathCorners, float* pathPts,
    float mapMinX, float mapMaxX, float mapMinZ, float mapMaxZ,
    bool mapBoundsValid, bool monActive, bool showPath) {
    if (!g_pShared || !g_showMinimapWindow || (g_speedrunMode && !g_practiceMode)) return;

    auto geometry=g_mapWorker.Get(MapLevelIndex(g_pShared->currentSceneIndex));
    bool hideSeek=g_pShared->hideAndSeekMode;
    bool fullGeometry=geometry&&geometry->live&&geometry->data&&!geometry->data->review&&
        (geometry->data->physicalCaptured||(geometry->outline&&!geometry->outline->empty()))&&
        geometry->data->pid==g_pShared->pid&&geometry->data->epoch==int(g_pShared->sceneEpoch)&&geometry->session==g_session;
    if(fullGeometry){const auto& d=*geometry->data;
        if(!mapBoundsValid){mapMinX=d.minX;mapMaxX=d.maxX;mapMinZ=d.minZ;mapMaxZ=d.maxZ;mapBoundsValid=true;}
        mapMinX=std::min(mapMinX,d.minX-2);mapMaxX=std::max(mapMaxX,d.maxX+2);mapMinZ=std::min(mapMinZ,d.minZ-2);mapMaxZ=std::max(mapMaxZ,d.maxZ+2);
    }
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float width = std::min(g_minimapWidth * g_dpiScale, std::max(60.f, display.x - 24));
    float height = std::min(g_minimapHeight * g_dpiScale, std::max(60.f, display.y - 24));
    if (minimapLockAspect) width = height = std::min(width, height);
    ImGui::SetNextWindowSize(ImVec2(width, height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((g_mapOptions.anchor==1||g_mapOptions.anchor==3)?std::max(12.f,display.x-width-12):12,
        g_mapOptions.anchor>=2?std::max(12.f,display.y-height-12):12),ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::Begin("Minimap", &g_showMinimapWindow,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing);

    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    if (isHovered)
        dl->AddRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), IM_COL32(80, 80, 80, 200), 0.0f, 0, 1.5f);

    ImVec2 center = ImVec2(winPos.x + winSize.x * 0.5f, winPos.y + winSize.y * 0.5f);
    float radius = std::min(winSize.x, winSize.y) * (width>=180*g_dpiScale?.42f:.45f);

    if (!mapBoundsValid || (mapMaxX - mapMinX) < 0.01f || (mapMaxZ - mapMinZ) < 0.01f) {
        ImGui::TextDisabled("Waiting for map bounds...");
        if(IsHideSeekScene(g_pShared->currentSceneIndex))dl->AddText(ImVec2(winPos.x+8,winPos.y+8),IM_COL32(255,177,76,255),"WIP");
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        return;
    }

    MapTransform transform(center,radius,g_mapOptions,px,pz,rotY,mapMinX,mapMaxX,mapMinZ,mapMaxZ);
    auto w2s_rot=[&](float x,float z){return transform.Project(x,z);};

    auto mapColor=[&](AppearancePreferences::Color c,float alpha=1.f){return ImGui::ColorConvertFloat4ToU32(suite_ui::Color(c,g_minimapOpacity*alpha));};
    const auto mapInk=suite_ui::Color(AppearancePreferences::MapWalls);
    auto wallFill=ImGui::ColorConvertFloat4ToU32(suite_ui::Alpha(suite_ui::Blend(suite_ui::Color(AppearancePreferences::MapBackground),mapInk,.34f),g_minimapOpacity));
    auto floorFill=ImGui::ColorConvertFloat4ToU32(suite_ui::Alpha(suite_ui::Blend(suite_ui::Color(AppearancePreferences::MapBackground),mapInk,.16f),g_minimapOpacity));
    dl->AddCircleFilled(center,radius+3*g_dpiScale,mapColor(AppearancePreferences::MapBackground,.48f));
    dl->AddCircleFilled(center,radius,mapColor(AppearancePreferences::MapBackground,.94f));
    ImGui::PushClipRect(winPos, ImVec2(winPos.x + winSize.x, winPos.y + winSize.y), true);

    int wallAlpha = (int)(g_minimapOpacity * 255);
    bool detailed=fullGeometry&&geometry->data->physicalCaptured;
    auto drawWall=[&](const float* corners,int count=4,bool mainWall=true){
        static std::vector<ImVec2> screenPoly, clipped, scratch;
        screenPoly.clear();
        bool inside=true;
        for (int j = 0; j < count; j++) {
            ImVec2 point=w2s_rot(corners[j*2], corners[j*2+1]);screenPoly.push_back(point);
            float dx=point.x-center.x,dy=point.y-center.y;inside &= dx*dx+dy*dy<=radius*radius;
        }
        float minX=screenPoly[0].x,maxX=minX,minY=screenPoly[0].y,maxY=minY;
        for(const auto& p:screenPoly){minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);}
        if(maxX<center.x-radius||minX>center.x+radius||maxY<center.y-radius||minY>center.y+radius)return;
        float area=0;for(int j=0;j<count;j++)area+=screenPoly[j].x*screenPoly[(j+1)%count].y-screenPoly[(j+1)%count].x*screenPoly[j].y;
        if(area<0)std::reverse(screenPoly.begin(),screenPoly.end());
        ImU32 fill=mainWall?wallFill:floorFill;
        if(inside&&count>=3) dl->AddConvexPolyFilled(screenPoly.data(),count,fill);
        else {
            ClipPolygonToCircleReusable(screenPoly, center, radius, clipped, scratch);
            if (clipped.size() >= 3) dl->AddConvexPolyFilled(clipped.data(),(int)clipped.size(),fill);
        }
        if(hideSeek||detailed)for(int j=0;j<count;j++){auto a=screenPoly[j],b=screenPoly[(j+1)%count];if(ClipLineToCircle(a,b,center,radius))dl->AddLine(a,b,mapColor(AppearancePreferences::MapWalls,mainWall?.94f:.64f),(mainWall?1.5f:1.f)*g_dpiScale);}
    };
    if(g_mapOptions.walls){
        if((hideSeek||detailed)&&fullGeometry){
            static std::vector<ImVec2> triangle,clipped,scratch;auto flags=dl->Flags;dl->Flags&=~ImDrawListFlags_AntiAliasedFill;
            if(geometry->floor)for(const auto& t:*geometry->floor){auto a=t[0],b=t[1],c=t[2];
                triangle={w2s_rot(a.x,a.z),w2s_rot(b.x,b.z),w2s_rot(c.x,c.z)};
                float area=0;for(int j=0;j<3;j++)area+=triangle[j].x*triangle[(j+1)%3].y-triangle[(j+1)%3].x*triangle[j].y;
                if(area<0)std::reverse(triangle.begin(),triangle.end());
                bool inside=true;float x0=triangle[0].x,x1=x0,y0=triangle[0].y,y1=y0;
                for(auto p:triangle){inside&=(p.x-center.x)*(p.x-center.x)+(p.y-center.y)*(p.y-center.y)<=radius*radius;x0=std::min(x0,p.x);x1=std::max(x1,p.x);y0=std::min(y0,p.y);y1=std::max(y1,p.y);}
                if(x1<center.x-radius||x0>center.x+radius||y1<center.y-radius||y0>center.y+radius)continue;
                if(inside)dl->AddConvexPolyFilled(triangle.data(),3,floorFill);
                else{ClipPolygonToCircleReusable(triangle,center,radius,clipped,scratch);if(clipped.size()>=3)dl->AddConvexPolyFilled(clipped.data(),int(clipped.size()),floorFill);}
            }
            dl->Flags=flags;
        }
        else if(!hideSeek&&!detailed)for(int i=0;i<wallCount&&i<200;i++){const float* w=g_pShared->wallBoxes+i*4;float p[8]={w[0],w[1],w[0],w[3],w[2],w[3],w[2],w[1]};drawWall(p);}
        if(detailed){
            static std::vector<float> points;for(const auto& part:geometry->data->physicalParts){points.clear();for(auto v:part.points){points.push_back(v.x);points.push_back(v.z);}drawWall(points.data(),int(part.points.size()),(part.flags&1)!=0);}
        } else if(hideSeek){
            if(fullGeometry)for(const auto& wall:geometry->data->obstacles){if(wall.approximate)continue;float p[8];for(int j=0;j<4;j++){p[j*2]=wall.corners[j].x;p[j*2+1]=wall.corners[j].z;}drawWall(p);}
            else for(int i=0;i<g_pShared->wallOutlineCount;i++)if(!g_pShared->wallOutlineApproximate[i])drawWall(g_pShared->wallOutlines+i*8);
        }
    }

    static std::vector<std::pair<ImVec2, ImVec2>> edges;
    static std::vector<float> previousBoxes;
    if (previousBoxes.size() != static_cast<size_t>(wallCount * 4) ||
        !std::equal(previousBoxes.begin(), previousBoxes.end(), g_pShared->wallBoxes)) {
        previousBoxes.assign(g_pShared->wallBoxes, g_pShared->wallBoxes + wallCount * 4);
        ExtractUniqueEdges(previousBoxes, wallCount, edges);
    }
    for (const auto& e : edges) if(!hideSeek&&!detailed) {
        ImVec2 a = w2s_rot(e.first.x, e.first.y), b = w2s_rot(e.second.x, e.second.y);
        if (g_mapOptions.walls && ClipLineToCircle(a, b, center, radius))
            dl->AddLine(a, b, mapColor(AppearancePreferences::MapWalls,.8f), 1.5f);
    }

    if(fullGeometry&&!detailed&&geometry->outline&&g_mapOptions.walls)for(const auto& edge:*geometry->outline){
        auto a=w2s_rot(edge.first.x,edge.first.z),b=w2s_rot(edge.second.x,edge.second.z);
        if(ClipLineToCircle(a,b,center,radius))dl->AddLine(a,b,mapColor(AppearancePreferences::MapWalls),1.5f);
    }
    dl->AddCircle(center,radius,mapColor(AppearancePreferences::Outline,.8f),64,1.5f*g_dpiScale);
    for(int tick=0;tick<24;tick++){float a=tick*6.2831853f/24;float inner=radius-(tick%6==0?6.f:3.f)*g_dpiScale;dl->AddLine({center.x+std::sin(a)*inner,center.y+std::cos(a)*inner},{center.x+std::sin(a)*radius,center.y+std::cos(a)*radius},mapColor(AppearancePreferences::MapWalls,tick%6==0?.65f:.25f),g_dpiScale);}

    for (int i = 0; g_mapOptions.crystals && i < crystalCount; i++) {
        ImVec2 sc = w2s_rot(g_pShared->crystalPositions[i * 2], g_pShared->crystalPositions[i * 2 + 1]);
        if ((sc.x - center.x) * (sc.x - center.x) + (sc.y - center.y) * (sc.y - center.y) > radius * radius) continue;
        float r=3*g_dpiScale;ImVec2 diamond[]={{sc.x,sc.y-r},{sc.x+r,sc.y},{sc.x,sc.y+r},{sc.x-r,sc.y}};dl->AddConvexPolyFilled(diamond,4,mapColor(AppearancePreferences::MapCrystal));
    }
    // Cardinals follow the same world projection as the walls. The ring is
    // screen decoration only; north remains the game's +Z axis.
    const char* cardinals[]={"N","E","S","W"};
    ImVec2 direction[]={{-transform.sine,-transform.cosine},{transform.cosine,-transform.sine},{transform.sine,transform.cosine},{-transform.cosine,transform.sine}};
    int labels=width>=180*g_dpiScale?4:1;float font=14*g_dpiScale;
    for(int i=0;i<labels;i++){auto v=direction[i];float r=radius-13*g_dpiScale;ImVec2 at(center.x+v.x*r,center.y+v.y*r);auto measure=ImGui::GetFont()->CalcTextSizeA(font,FLT_MAX,0,cardinals[i]);ImVec2 text(at.x-measure.x*.5f,at.y-measure.y*.5f);
        dl->AddRectFilled({text.x-2*g_dpiScale,text.y},{text.x+measure.x+2*g_dpiScale,text.y+measure.y},mapColor(AppearancePreferences::MapBackground,.92f),3*g_dpiScale);
        dl->AddText(ImGui::GetFont(),font,text,mapColor(i==0?AppearancePreferences::MapPlayer:AppearancePreferences::MapWalls),cardinals[i]);
    }
    if(g_mapOptions.trail) for(size_t i=1;i<g_trail.Size();i++) {
        auto first=g_trail.At(i-1),second=g_trail.At(i);
        auto a=w2s_rot(first.x,first.y),b=w2s_rot(second.x,second.y);
        if(ClipLineToCircle(a,b,center,radius)) dl->AddLine(a,b,mapColor(AppearancePreferences::MapPlayer,.38f),1.5f);
    }
    if(g_mapOptions.monster&&g_pShared->timerMode)for(int i=0;i<g_pShared->timerMonsterCount;i++){
        auto p=w2s_rot(g_pShared->timerMonsterPositions[i*2],g_pShared->timerMonsterPositions[i*2+1]);
        if(std::hypot(p.x-center.x,p.y-center.y)>radius)continue;
        int state=g_pShared->timerMonsterStates[i];
        dl->AddCircleFilled(p,3.f,IM_COL32(250,78,78,wallAlpha));
        if(state>0)dl->AddCircle(p,5.f,state==2?IM_COL32(255,140,123,wallAlpha):IM_COL32(249,187,95,wallAlpha),0,1.5f);
    }
    if (g_mapOptions.monster && monActive && !g_pShared->timerMode) {
        ImVec2 mp = w2s_rot(monX, monZ);
        if ((mp.x - center.x) * (mp.x - center.x) + (mp.y - center.y) * (mp.y - center.y) <= radius * radius)
            dl->AddCircleFilled(mp, 3.5f, IM_COL32(250,104,91,wallAlpha));
    }
    if (showPath && pathCorners > 0) {
        for (int i = 0; i < pathCorners - 1; i++) {
            ImVec2 a = w2s_rot(pathPts[i * 2], pathPts[i * 2 + 1]);
            ImVec2 b = w2s_rot(pathPts[i * 2 + 2], pathPts[i * 2 + 3]);
            if (ClipLineToCircle(a, b, center, radius))
                dl->AddLine(a, b, IM_COL32(255, 100, 0, wallAlpha), 2.0f);
        }
    }
    auto planner=DisplayedPlanner(MapLevelIndex(g_pShared->currentSceneIndex));
    if(planner&&LivePlannerRoute(*planner)) {
        const auto& line=planner->route.line;
        for(size_t i=1;i<line.size();i++) {
            auto a=w2s_rot(line[i-1].x,line[i-1].z),b=w2s_rot(line[i].x,line[i].z);
            bool estimated=!practice::RouteLegVerified(planner->route,i-1);
            if(!estimated&&ClipLineToCircle(a,b,center,radius))DrawPracticeRouteLeg(dl,a,b,mapColor(AppearancePreferences::MapRoute),2.f,false);
        }
    }

    if(g_mapOptions.monster&&g_pShared->timerMode&&g_pShared->timerMonsterOverflow>0){
        char text[64];snprintf(text,sizeof(text),"+%d enemies not shown",g_pShared->timerMonsterOverflow);
        dl->AddText(ImVec2(winPos.x+8,winPos.y+winSize.y-20),IM_COL32(249,187,95,wallAlpha),text);
    }
    // Keep the player readable when standing on a trail.
    ImVec2 player=w2s_rot(px,pz);
    float angle=(g_mapOptions.playerUp?0:PlayerMapHeading(rotY))*3.14159265358979323846f/180;
    ImVec2 forward(std::sin(angle),-std::cos(angle)),right(std::cos(angle),std::sin(angle));
    if(std::hypot(player.x-center.x,player.y-center.y)<=radius-8){
        dl->AddCircleFilled(player,10*g_dpiScale,mapColor(AppearancePreferences::MapBackground,.94f),20);
        dl->AddCircle(player,10*g_dpiScale,mapColor(AppearancePreferences::MapPlayer,.34f),20,g_dpiScale);
        dl->AddTriangleFilled(ImVec2(player.x+forward.x*7,player.y+forward.y*7),
            ImVec2(player.x-forward.x*4-right.x*4,player.y-forward.y*4-right.y*4),
            ImVec2(player.x-forward.x*4+right.x*4,player.y-forward.y*4+right.y*4),mapColor(AppearancePreferences::MapPlayer));
    }
    ImGui::PopClipRect();
    auto title=suite_ui::Color(AppearancePreferences::MapWalls,g_minimapOpacity);
    if(width>=180*g_dpiScale){char label[64];float heading=std::fmod(PlayerMapHeading(rotY)+360.f,360.f);snprintf(label,sizeof(label),"%s  /  %03.0f deg",g_mapOptions.playerUp?"PLAYER UP":"NORTH UP",heading);float font=13*g_dpiScale;auto size=ImGui::GetFont()->CalcTextSizeA(font,FLT_MAX,0,label);dl->AddText(ImGui::GetFont(),font,{winPos.x+(winSize.x-size.x)*.5f,winPos.y+winSize.y-17*g_dpiScale},ImGui::ColorConvertFloat4ToU32(title),label);}
    if(g_pShared&&IsHideSeekScene(g_pShared->currentSceneIndex))dl->AddText(ImVec2(winPos.x+8,winPos.y+8),IM_COL32(255,177,76,255),"WIP");
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}

