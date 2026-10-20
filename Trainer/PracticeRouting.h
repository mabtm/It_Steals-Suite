#pragma once
#include "PracticeMapData.h"
#include "RecordedTiming.h"
#include "PhysicalNavigation.h"
#include <array>
#include <bit>
#include <functional>
#include <limits>
#include <map>
#include <mutex>
#include <numeric>
#include <queue>
#include <set>
#include <tuple>

namespace practice {
inline constexpr double Unreachable=std::numeric_limits<double>::infinity();
using Cancel=std::function<bool()>;
inline double Area(Vec3 a,Vec3 b,Vec3 c) {return double(c.x-a.x)*(b.z-a.z)-double(b.x-a.x)*(c.z-a.z);}
inline bool Same(Vec3 a,Vec3 b) {return Distance(a,b)<.0001;}
struct NavPath {std::vector<Vec3> points;double length=Unreachable;};
struct NavProjection {int triangle=-1;Vec3 point;};
struct PreparedRoutes;
class NavGraph {
    struct Triangle {std::array<int,3> ids;Vec3 center,min,max;bool usable=false;std::vector<int> neighbors;};
    std::vector<Vec3> vertices;
    std::vector<Triangle> triangles;
    std::vector<std::pair<Vec3,Vec3>> boundary;
    mutable std::once_flag preparedOnce;mutable std::shared_ptr<const PreparedRoutes> preparedRoutes;
    bool captured=false;std::shared_ptr<const PhysicalNavigation> physical;int catalogSpawnCount=0;std::vector<PickupRegion> pickupRegions;
    std::vector<Vec3> routeNodes,catalogPositions;
    std::vector<int> anchors,components;
    std::vector<std::vector<std::pair<int,double>>> routeEdges;
    void LabelComponents(const Cancel& canceled) {
        const size_t count=captured?routeNodes.size():triangles.size();components.assign(count,-1);int component=0;std::vector<int> todo;
        for(size_t node=0;node<count;node++)if(components[node]<0&&(captured||triangles[node].usable)){
            components[node]=component;todo.push_back(static_cast<int>(node));
            for(size_t i=0;i<todo.size();i++){
                if((i&255)==0&&canceled())throw std::runtime_error("Canceled");
                int at=todo[i];auto visit=[&](int next){if(components[next]<0){components[next]=component;todo.push_back(next);}};
                if(captured)for(auto edge:routeEdges[at])visit(edge.first);else for(int next:triangles[at].neighbors)visit(next);
            }
            todo.clear();component++;
        }
    }
    static Vec3 Closest(Vec3 p,Vec3 a,Vec3 b,Vec3 c) {
        // Project onto the triangle in X/Z and retain the actual floor height.
        double denominator=double(b.z-c.z)*(a.x-c.x)+double(c.x-b.x)*(a.z-c.z);
        double u=(double(b.z-c.z)*(p.x-c.x)+double(c.x-b.x)*(p.z-c.z))/denominator;
        double v=(double(c.z-a.z)*(p.x-c.x)+double(a.x-c.x)*(p.z-c.z))/denominator;
        if(u>=0&&v>=0&&u+v<=1)return {p.x,static_cast<float>(u*a.y+v*b.y+(1-u-v)*c.y),p.z};
        Vec3 best=a;double bestDistance=Unreachable;
        for(auto edge:{std::pair(a,b),std::pair(b,c),std::pair(c,a)}) {
            double dx=edge.second.x-edge.first.x,dz=edge.second.z-edge.first.z,length=dx*dx+dz*dz;
            double t=length>0?std::clamp(((p.x-edge.first.x)*dx+(p.z-edge.first.z)*dz)/length,0.,1.):0;
            Vec3 q{static_cast<float>(edge.first.x+t*dx),static_cast<float>(edge.first.y+t*(edge.second.y-edge.first.y)),static_cast<float>(edge.first.z+t*dz)};
            double distance=Distance(p,q);if(distance<bestDistance){best=q;bestDistance=distance;}
        }
        return best;
    }
    std::pair<Vec3,Vec3> Portal(int a,int b) const {
        std::vector<int> common;for(int id:triangles[a].ids)for(int other:triangles[b].ids)if(id==other)common.push_back(id);
        if(common.size()!=2)throw std::runtime_error("Invalid NavMesh portal");
        Vec3 left=vertices[common[0]],right=vertices[common[1]],from=triangles[a].center,to=triangles[b].center;
        if(Area(from,to,left)>Area(from,to,right))std::swap(left,right);
        return {left,right};
    }
    bool LineInCorridor(Vec3 a,Vec3 b,const std::vector<int>& corridor,const Cancel& canceled) const {
        // Validate the funnel result. A failed shortcut falls back to portal
        // midpoints; never draw a straight path across a hole or another floor.
        std::vector<std::pair<double,double>> intervals;
        int checked=0;for(int id:corridor) {
            if((++checked&127)==0&&canceled())throw std::runtime_error("Canceled");
            const auto& tri=triangles[id];double lo=0,hi=1;
            Vec3 p=vertices[tri.ids[0]],q=vertices[tri.ids[1]],r=vertices[tri.ids[2]];
            double sign=Area(p,q,r)<0?-1:1;
            for(int e=0;e<3;e++) {
                Vec3 x=vertices[tri.ids[e]],y=vertices[tri.ids[(e+1)%3]];
                double begin=Area(x,y,a)*sign,end=Area(x,y,b)*sign;
                if(begin<-.0001&&end<-.0001){lo=2;break;}
                double delta=end-begin;
                if(std::abs(delta)>.0000001){double t=(-.0001-begin)/delta;if(delta>0)lo=std::max(lo,t);else hi=std::min(hi,t);}
            }
            if(lo>hi||lo>1||hi<0)continue;
            double middle=(std::max(0.,lo)+std::min(1.,hi))*.5;
            Vec3 probe{static_cast<float>(a.x+(b.x-a.x)*middle),static_cast<float>(a.y+(b.y-a.y)*middle),static_cast<float>(a.z+(b.z-a.z)*middle)};
            if(Distance(probe,Closest(probe,p,q,r))>.3)continue;
            intervals.emplace_back(std::max(0.,lo),std::min(1.,hi));
        }
        std::sort(intervals.begin(),intervals.end());double covered=0;
        for(auto interval:intervals){if(interval.first>covered+.0001)return false;covered=std::max(covered,interval.second);if(covered>=.9999)return true;}return false;
    }

    NavPath Checked(NavPath path,const Cancel& canceled,bool repair=true)const{
        if(!physical||!physical->Enabled()||!std::isfinite(path.length))return path;
        if(path.points.empty()||!physical->Clear(path.points.front(),path.points.front())||!physical->Clear(path.points.back(),path.points.back()))return {};
        if(path.points.size()>1&&physical->Visible(path.points.front(),path.points.back()))return {{path.points.front(),path.points.back()},Distance(path.points.front(),path.points.back())};
        std::vector<Vec3> anchors{path.points.front()};
        for(size_t i=1;i+1<path.points.size();i++){
            Vec3 point=path.points[i];if(!physical->Clear(point,point)){if(!physical->AdjustOnFloor(point))continue;}
            if(!Same(anchors.back(),point))anchors.push_back(point);
        }
        if(!Same(anchors.back(),path.points.back()))anchors.push_back(path.points.back());
        std::vector<Vec3> line{anchors.front()};
        for(size_t i=1;i<anchors.size();i++){
            if(canceled())throw std::runtime_error("Canceled");
            if(physical->Visible(anchors[i-1],anchors[i]))line.push_back(anchors[i]);
            else {if(!repair)return {};auto detour=physical->Repair(anchors[i-1],anchors[i],canceled);if(detour.empty()){detour=physical->Repair(path.points.front(),path.points.back(),canceled);if(detour.empty())return {};path.points=std::move(detour);path.length=0;for(size_t k=1;k<path.points.size();k++)path.length+=Distance(path.points[k-1],path.points[k]);return path;}line.insert(line.end(),detour.begin()+1,detour.end());}
        }
        if(line.size()>2){std::vector<Vec3> pulled{line.front()};size_t anchor=0;for(size_t next=2;next<line.size();next++)if(!physical->Visible(line[anchor],line[next])){if(!Same(pulled.back(),line[next-1]))pulled.push_back(line[next-1]);anchor=next-1;}if(!Same(pulled.back(),line.back()))pulled.push_back(line.back());line=std::move(pulled);}
        path.points=std::move(line);path.length=0;for(size_t i=1;i<path.points.size();i++)path.length+=Distance(path.points[i-1],path.points[i]);return path;
    }
public:
    struct Tree {NavProjection start;std::vector<int> parent;std::vector<double> cost;};
    explicit NavGraph(const MapData& data,const Cancel& canceled=[] {return false;},bool floorOnly=false) {
        catalogSpawnCount=int(data.spawns.size());pickupRegions=data.pickupRegions;catalogPositions=data.spawns;catalogPositions.insert(catalogPositions.end(),data.points.begin(),data.points.end());
        // Unity may return duplicated vertices at tile edges. Weld only equal
        // 3D positions, so different floors never become connected in X/Z.
        std::map<std::tuple<int64_t,int64_t,int64_t>,int> unique;std::vector<int> remap;
        for(Vec3 v:data.vertices) {
            auto key=std::make_tuple(std::llround(v.x*10000),std::llround(v.y*10000),std::llround(v.z*10000));
            auto [it,inserted]=unique.emplace(key,static_cast<int>(vertices.size()));
            if(inserted)vertices.push_back(v);
            remap.push_back(it->second);
        }
        std::map<std::pair<int,int>,std::vector<int>> edges;std::set<std::array<int,3>> uniqueTriangles;
        for(size_t i=0;i<data.indices.size();i+=3) {
            if((i&1023)==0&&canceled())throw std::runtime_error("Canceled");
            Triangle t;for(int j=0;j<3;j++)t.ids[j]=remap.at(data.indices[i+j]);
            auto key=t.ids;std::sort(key.begin(),key.end());if(!uniqueTriangles.insert(key).second)continue;
            auto a=vertices[t.ids[0]],b=vertices[t.ids[1]],c=vertices[t.ids[2]];
            t.min={std::min({a.x,b.x,c.x}),std::min({a.y,b.y,c.y}),std::min({a.z,b.z,c.z})};t.max={std::max({a.x,b.x,c.x}),std::max({a.y,b.y,c.y}),std::max({a.z,b.z,c.z})};
            t.center={(a.x+b.x+c.x)/3,(a.y+b.y+c.y)/3,(a.z+b.z+c.z)/3};t.usable=std::abs(Area(a,b,c))>.000001;
            // Filter BEFORE extracting boundaries and labeling connectivity:
            // roof triangles must not appear as walls over ground passages.
            if(data.physicalCaptured||IsHideSeekScene(data.scene))t.usable=t.usable&&FloorSlope(data,a,b,c);
            int id=static_cast<int>(triangles.size());triangles.push_back(t);
            if(t.usable)for(int e=0;e<3;e++){int x=t.ids[e],y=t.ids[(e+1)%3];if(x>y)std::swap(x,y);edges[{x,y}].push_back(id);}
        }
        for(const auto& [edge,owners]:edges){if(owners.size()==2){triangles[owners[0]].neighbors.push_back(owners[1]);triangles[owners[1]].neighbors.push_back(owners[0]);}else if(owners.size()==1)boundary.emplace_back(vertices[edge.first],vertices[edge.second]);}
        // Keep the established triangle model when it covers the whole
        // catalog. A newer, incomplete enemy NavMesh must not replace it.
        LabelComponents(canceled);
        if(data.physicalCaptured||IsHideSeekScene(data.scene)){
            // Select the surfaces supporting the actual scene's spawn/point
            // catalog, rather than comparing a baked NavMesh to the physical
            // capsule bottom. Unity's voxelized surface can differ by more
            // than a step; the first capture can also occur while falling.
            // A roof above a target is never a supporting surface. Connected
            // ramps remain part of the selected component at any height.
            std::set<int> floorComponents;
            auto support=[&](Vec3 point){
                if(canceled())throw std::runtime_error("Canceled");
                int best=-1;double score=Unreachable;
                for(size_t i=0;i<triangles.size();i++){
                    const auto& t=triangles[i];if(!t.usable)continue;
                    Vec3 p=Closest(point,vertices[t.ids[0]],vertices[t.ids[1]],vertices[t.ids[2]]);
                    double horizontal=std::hypot(p.x-point.x,p.z-point.z),gap=point.y-p.y;
                    if(horizontal>2||gap<-.15||gap>4)continue;
                    double distance=horizontal*horizontal+gap*gap;
                    if(distance<score){score=distance;best=int(i);}
                }
                if(best>=0)floorComponents.insert(components[best]);
            };
            for(Vec3 point:data.spawns)support(point);
            for(Vec3 point:data.points)support(point);
            for(size_t i=0;i<triangles.size();i++)if(!floorComponents.contains(components[i]))triangles[i].usable=false;
            boundary.clear();for(const auto& [edge,owners]:edges){int count=0;for(int owner:owners)if(triangles[owner].usable)count++;if(count==1)boundary.emplace_back(vertices[edge.first],vertices[edge.second]);}
            LabelComponents(canceled);
        }
        physical=std::make_shared<PhysicalNavigation>(data,Floor());
        if(floorOnly)return;
        int catalogComponent=-1,floorValid=0;std::map<int,int> floorCoverage;bool covered=!data.spawns.empty()&&!data.points.empty();
        auto check=[&](Vec3 point){if(canceled())throw std::runtime_error("Canceled");int part=Component(Project(point));if(part<0)covered=false;else {floorValid++;floorCoverage[part]++;if(catalogComponent<0)catalogComponent=part;else if(part!=catalogComponent)covered=false;}};
        for(Vec3 point:data.spawns)check(point);
        for(Vec3 point:data.points)check(point);
        // Preserve already captured checked corridors from older archives;
        // otherwise every mode uses the same coverage/funnel/fallback choice.
        // A failed/partial AI capture (including an old wrong agent type)
        // must not replace more useful player-floor connectivity wholesale.
        int nativeValid=0;int64_t floorPairs=0,nativePairs=0;std::vector<int> nativeParent(data.navigationNodes.size());std::iota(nativeParent.begin(),nativeParent.end(),0);
        auto root=[&](int id){if(id<0||id>=int(nativeParent.size()))throw std::runtime_error("Invalid captured navigation index");while(nativeParent[id]!=id){nativeParent[id]=nativeParent[nativeParent[id]];id=nativeParent[id];}return id;};
        if(data.navigationEdges.size()%2)throw std::runtime_error("Incomplete captured navigation edge");
        for(size_t i=0;i<data.navigationEdges.size();i+=2){int a=root(data.navigationEdges[i]),b=root(data.navigationEdges[i+1]);if(a!=b)nativeParent[a]=b;}
        std::map<int,int> nativeCoverage;for(int anchor:data.navigationAnchors)if(anchor>=0){nativeValid++;nativeCoverage[root(anchor)]++;}
        for(auto [id,count]:floorCoverage){(void)id;floorPairs+=int64_t(count)*(count-1)/2;}
        for(auto [id,count]:nativeCoverage){(void)id;nativePairs+=int64_t(count)*(count-1)/2;}
        captured=data.navigationStatus==2&&nativeValid>0&&(data.navigationModel==1||(!covered&&nativeValid>=floorValid&&nativePairs>=floorPairs));
        if(captured){
            routeNodes=data.navigationNodes;anchors=data.navigationAnchors;
            routeEdges.resize(routeNodes.size());
            if(!routeNodes.empty())for(size_t i=0;i<data.navigationEdges.size();i+=2){int a=data.navigationEdges[i],b=data.navigationEdges[i+1];double length=Distance(routeNodes[a],routeNodes[b]);routeEdges.at(a).emplace_back(b,length);routeEdges.at(b).emplace_back(a,length);}
        }
        if(captured)LabelComponents(canceled);
    }
    const auto& CatalogPositions()const{return catalogPositions;}
    std::shared_ptr<const PreparedRoutes> Routes(const Cancel& canceled=[] {return false;},const std::function<void(int,int)>& progress={})const;
    NavPath Polish(NavPath path,const Cancel& canceled)const{return Checked(std::move(path),canceled,false);}
    bool Visible(Vec3 a,Vec3 b)const{return physical&&physical->Visible(a,b);}
    bool Empty() const {return captured?routeNodes.empty():std::none_of(triangles.begin(),triangles.end(),[](const auto& t){return t.usable;});}
    std::vector<std::array<Vec3,3>> Floor()const {std::vector<std::array<Vec3,3>> out;for(const auto& t:triangles)if(t.usable)out.push_back({vertices[t.ids[0]],vertices[t.ids[1]],vertices[t.ids[2]]});return out;}
    bool UsesUnityCorridors() const {return captured;}
    int Component(NavProjection p)const{return p.triangle>=0&&p.triangle<static_cast<int>(components.size())?components[p.triangle]:-1;}
    NavProjection CatalogProject(int index,Vec3 point)const{
        NavProjection result;
        if(!captured)result=FloorProject(point);
        else {
            if(index<0||index>=static_cast<int>(anchors.size())||anchors[index]<0)return {};
            result={anchors[index],routeNodes[anchors[index]]};
            if(physical&&physical->Enabled()){
                auto floor=FloorProject(point);if(floor.triangle>=0)result.point=floor.point;
                else if(!physical->Clear(result.point,result.point))return {};
            }
        }
        int crystal=index-catalogSpawnCount;
        if(result.triangle>=0&&crystal>=0&&crystal<int(pickupRegions.size())&&pickupRegions[crystal].verified&&std::hypot(result.point.x-pickupRegions[crystal].center.x,result.point.z-pickupRegions[crystal].center.z)>pickupRegions[crystal].radius+.001)return {};
        return result;
    }
    const auto& Boundary()const{return boundary;}
    NavProjection Project(Vec3 point) const {
        NavProjection result;double best=Unreachable;
        if(captured){
            for(size_t i=0;i<catalogPositions.size();i++)if(Same(catalogPositions[i],point))return CatalogProject(static_cast<int>(i),point);
            for(size_t i=0;i<routeNodes.size();i++){auto p=routeNodes[i];double distance=Distance(point,p);if(std::hypot(p.x-point.x,p.z-point.z)<=2&&std::abs(p.y-point.y)<=4&&distance<best){best=distance;result={static_cast<int>(i),p};}}
            return result;
        }
        return FloorProject(point);
    }
    NavProjection FloorProject(Vec3 point)const{
        NavProjection result;double best=Unreachable;
        for(size_t i=0;i<triangles.size();i++) {
            const auto& t=triangles[i];if(!t.usable)continue;
            double dx=std::max({double(t.min.x-point.x),0.,double(point.x-t.max.x)}),dz=std::max({double(t.min.z-point.z),0.,double(point.z-t.max.z)});
            if(dx*dx+dz*dz>std::min(4.,best*best)||point.y<t.min.y-4||point.y>t.max.y+4)continue;
            Vec3 p=Closest(point,vertices[t.ids[0]],vertices[t.ids[1]],vertices[t.ids[2]]);
            if(Distance(point,p)>=best||std::hypot(p.x-point.x,p.z-point.z)>2||std::abs(p.y-point.y)>4)continue;
            if(physical&&physical->Enabled()&&!physical->Clear(p,p)){
                if(!physical->Adjust(p))continue;
                Vec3 on=Closest(p,vertices[t.ids[0]],vertices[t.ids[1]],vertices[t.ids[2]]);
                if(Distance(p,on)>.002||std::hypot(p.x-point.x,p.z-point.z)>2)continue;
                p=on;
            }
            double score=Distance(point,p);if(score<best){best=score;result={static_cast<int>(i),p};}
        }
        return result;
    }
    Tree Search(NavProjection source,const Cancel& canceled=[] {return false;}) const {
        Tree result;result.start=source;const size_t count=captured?routeNodes.size():triangles.size();result.parent.assign(count,-1);result.cost.assign(count,Unreachable);
        if(source.triangle<0)return result;
        using Item=std::pair<double,int>;std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;
        result.cost[source.triangle]=0;queue.push({0,source.triangle});int steps=0;
        while(!queue.empty()) {
            auto [cost,id]=queue.top();queue.pop();if(cost>result.cost[id])continue;
            if((++steps&127)==0&&canceled())throw std::runtime_error("Canceled");
            if(captured){for(auto edge:routeEdges[id]){int next=edge.first;double candidate=cost+edge.second;if(candidate<result.cost[next]){result.cost[next]=candidate;result.parent[next]=id;queue.push({candidate,next});}}continue;}
            for(int next:triangles[id].neighbors) {
                double candidate=cost+Distance(triangles[id].center,triangles[next].center);
                if(candidate<result.cost[next]){result.cost[next]=candidate;result.parent[next]=id;queue.push({candidate,next});}
            }
        }
        return result;
    }
    NavPath Path(const Tree& tree,NavProjection target,const Cancel& canceled=[] {return false;},bool repair=true) const {
        NavPath result;if(tree.start.triangle<0||target.triangle<0||target.triangle>=int(tree.cost.size()))return result;
        if(physical&&physical->Enabled()&&physical->Visible(tree.start.point,target.point))return {{tree.start.point,target.point},Distance(tree.start.point,target.point)};
        if(!std::isfinite(tree.cost[target.triangle])){
            if(repair&&physical&&physical->Enabled()){result.points=physical->Repair(tree.start.point,target.point,canceled);if(!result.points.empty()){result.length=0;for(size_t i=1;i<result.points.size();i++)result.length+=Distance(result.points[i-1],result.points[i]);}}return result;
        }
        if(captured){
            for(int id=target.triangle;id>=0;id=tree.parent[id]){if(canceled())throw std::runtime_error("Canceled");result.points.push_back(routeNodes[id]);if(result.points.size()>routeNodes.size())throw std::runtime_error("Cyclic captured navigation path");if(id==tree.start.triangle)break;}
            std::reverse(result.points.begin(),result.points.end());
            if(physical&&physical->Enabled()&&!result.points.empty()){if(result.points.size()==1&&!Same(tree.start.point,target.point))result.points={tree.start.point,target.point};else {result.points.front()=tree.start.point;result.points.back()=target.point;}}
            result.length=tree.cost[target.triangle];return Checked(std::move(result),canceled,repair);
        }
        std::vector<int> corridor;for(int id=target.triangle;id>=0;id=tree.parent[id]) {
            if(canceled())throw std::runtime_error("Canceled");
            corridor.push_back(id);if(corridor.size()>4096)return result;if(id==tree.start.triangle)break;
        }
        if(corridor.back()!=tree.start.triangle)return result;
        std::reverse(corridor.begin(),corridor.end());
        std::vector<std::pair<Vec3,Vec3>> portals{{tree.start.point,tree.start.point}};
        for(size_t i=1;i<corridor.size();i++)portals.push_back(Portal(corridor[i-1],corridor[i]));
        portals.push_back({target.point,target.point});
        std::vector<Vec3> pulled{tree.start.point};Vec3 apex=tree.start.point,left=apex,right=apex;
        size_t apexIndex=0,leftIndex=0,rightIndex=0;
        size_t iterations=0;bool funnelFinished=true;
        for(size_t i=1;i<portals.size();i++) {
            if(canceled())throw std::runtime_error("Canceled");
            if(++iterations>portals.size()*4){funnelFinished=false;break;}
            Vec3 newLeft=portals[i].first,newRight=portals[i].second;
            if(Area(apex,right,newRight)<=0) {
                if(Same(apex,right)||Area(apex,left,newRight)>0){right=newRight;rightIndex=i;}
                else {pulled.push_back(left);apex=left;apexIndex=leftIndex;left=right=apex;leftIndex=rightIndex=apexIndex;i=apexIndex;continue;}
            }
            if(Area(apex,left,newLeft)>=0) {
                if(Same(apex,left)||Area(apex,right,newLeft)<0){left=newLeft;leftIndex=i;}
                else {pulled.push_back(right);apex=right;apexIndex=rightIndex;left=right=apex;leftIndex=rightIndex=apexIndex;i=apexIndex;continue;}
            }
        }
        if(!Same(pulled.back(),target.point))pulled.push_back(target.point);
        bool valid=funnelFinished;for(size_t i=1;valid&&i<pulled.size();i++)if(!LineInCorridor(pulled[i-1],pulled[i],corridor,canceled)){valid=false;break;}
        if(!valid){pulled.clear();for(const auto& portal:portals)pulled.push_back(Midpoint(portal.first,portal.second));}
        result.points=std::move(pulled);result.length=0;for(size_t i=1;i<result.points.size();i++)result.length+=Distance(result.points[i-1],result.points[i]);return Checked(std::move(result),canceled,repair);
    }
    double Length(const Tree& tree,NavProjection target,const Cancel& canceled=[] {return false;})const{
        if(!captured||(physical&&physical->Enabled()))return Path(tree,target,canceled).length;
        if(tree.start.triangle<0||target.triangle<0||target.triangle>=static_cast<int>(tree.cost.size()))return Unreachable;
        return tree.cost[target.triangle]; // Draw only top routes; no all-pairs polyline allocations.
    }
};
}
#include "PreparedRoutes.h"
namespace practice {
struct RouteResult {
    std::vector<int> order,contacts;std::vector<Vec3> line;
    std::vector<uint8_t> estimatedLine;
    double length=0;bool exact=false,complete=false,estimated=false,pickupOptimized=false;
    std::string message;
};
inline void AppendRoutePoint(RouteResult& route,Vec3 point,bool estimated) {
    if(!route.line.empty()&&Same(route.line.back(),point))return;
    if(!route.line.empty())route.estimatedLine.push_back(estimated?1:0);
    route.line.push_back(point);
}
inline void DescribeRouteEstimate(RouteResult& route,bool estimated) {
    route.estimated=estimated;
    if(estimated)route.message="Estimated distances: missing navigation pairs use straight-line distances. Unverified legs are hidden from the map. "+route.message;
}
// Each route worker owns this scratch space. Matrices and the exact solver's
// tables are reused across seeds. Exact-DP ties and all heuristic starts are retained.
struct RouteWorkspace {
    std::array<double,65*65> matrix;
    std::vector<double> dp;
    std::vector<int8_t> parent;
    bool estimated=false;
};
// Any complete open tour contains a spanning tree of the crystal vertices
// plus an initial spawn edge. This bound is valid for the modeled symmetric
// matrix, including its corridor approximations; no triangle inequality used.
inline double MstRouteLowerBound(const double* matrix,int size,const Cancel& canceled) {
    int n=size-1;if(n<1||n>64)return Unreachable;
    for(int i=0;i<size*size;i++)if(!std::isfinite(matrix[i]))return Unreachable;
    std::array<double,64> costs;costs.fill(Unreachable);costs[0]=0;
    uint64_t used=0;double bound=Unreachable;
    for(int i=1;i<size;i++)bound=std::min(bound,matrix[i]);
    for(int step=0;step<n;step++) {
        if((step&7)==0&&canceled())throw std::runtime_error("Canceled");
        int next=-1;for(int i=0;i<n;i++)if(!(used&(uint64_t{1}<<i))&&(next<0||costs[i]<costs[next]))next=i;
        if(next<0||!std::isfinite(costs[next]))return Unreachable;
        used|=uint64_t{1}<<next;bound+=costs[next];
        for(int i=0;i<n;i++)if(!(used&(uint64_t{1}<<i)))costs[i]=std::min(costs[i],matrix[(next+1)*size+i+1]);
    }
    return bound;
}
inline RouteResult SolveOrderFlat(const double* matrix,int size,RouteWorkspace& work,const Cancel& canceled,bool exactWhenPossible) {
    RouteResult result;int n=size-1;if(n<=0||n>64){result.message="Routing supports 1 to 64 crystals.";return result;}
    if(canceled())throw std::runtime_error("Canceled");
    for(int i=0;i<size*size;i++)if(!std::isfinite(matrix[i])){result.message="Navigation distances are incomplete; this does not mean the seed is unplayable.";return result;}
    auto distance=[&](int from,int to){return matrix[from*size+to];};
    if(n<=15&&exactWhenPossible) {
        result.exact=true;unsigned masks=1u<<n;size_t entries=static_cast<size_t>(masks)*n;
        work.dp.resize(entries);work.parent.resize(entries);
        for(int end=0;end<n;end++){size_t at=(size_t{1}<<end)*n+end;work.dp[at]=distance(0,end+1);work.parent[at]=-1;}
        for(unsigned mask=1;mask<masks;mask++) {
            if((mask&255)==0&&canceled())throw std::runtime_error("Canceled");
            for(unsigned ends=mask;ends;ends&=ends-1) {
                int end=std::countr_zero(ends);unsigned previous=mask^(1u<<end);if(!previous)continue;
                double best=Unreachable;int8_t parent=-1;
                for(unsigned froms=previous;froms;froms&=froms-1) {
                    int from=std::countr_zero(froms);double candidate=work.dp[previous*n+from]+distance(from+1,end+1);
                    if(candidate<best){best=candidate;parent=static_cast<int8_t>(from);}
                }
                work.dp[mask*n+end]=best;work.parent[mask*n+end]=parent;
            }
        }
        int end=0;for(int i=1;i<n;i++)if(work.dp[(masks-1)*n+i]<work.dp[(masks-1)*n+end])end=i;
        result.length=work.dp[(masks-1)*n+end];unsigned mask=masks-1;
        if(!std::isfinite(result.length)){result.message="Route distances exceed supported numeric limits.";return result;}
        while(mask){result.order.push_back(end);int previous=work.parent[mask*n+end];mask^=1u<<end;end=previous;}
        std::reverse(result.order.begin(),result.order.end());
    } else {
        result.length=Unreachable;std::array<int,64> order;
        std::array<std::array<uint8_t,64>,64> nearest;
        for(int from=0;from<n;from++){
            auto& row=nearest[from];for(int i=0;i<n;i++)row[i]=uint8_t(i);
            std::sort(row.begin(),row.begin()+n,[&](int a,int b){double x=distance(from+1,a+1),y=distance(from+1,b+1);return x<y||(x==y&&a<b);});
        }
        const uint64_t all=n==64?~uint64_t{0}:(uint64_t{1}<<n)-1;
        for(int first=0;first<n;first++) {
            if(canceled())throw std::runtime_error("Canceled");
            order[0]=first;uint64_t unused=all^(uint64_t{1}<<first);
            for(int count=1;count<n;count++) {
                int next=-1;
                for(int rank=0;rank<n;rank++){int candidate=nearest[order[count-1]][rank];if(unused&(uint64_t{1}<<candidate)){next=candidate;break;}}
                order[count]=next;unused^=uint64_t{1}<<next;
            }
            double current=0;int from=0;for(int i=0;i<n;i++){current+=distance(from,order[i]+1);from=order[i]+1;}
            // Same multi-start open-path 2-opt, including the final section.
            for(int pass=0;pass<32;pass++) {
                if(canceled())throw std::runtime_error("Canceled");
                bool improved=false;
                for(int a=0;a<n-1;a++)for(int b=a+1;b<n;b++) {
                    int prev=a?order[a-1]+1:0,next=b+1<n?order[b+1]+1:-1;
                    double delta=distance(prev,order[b]+1)-distance(prev,order[a]+1);
                    if(next>=0)delta+=distance(order[a]+1,next)-distance(order[b]+1,next);
                    if(delta<-.000001){std::reverse(order.begin()+a,order.begin()+b+1);current+=delta;improved=true;}
                }
                if(!improved)break;
            }
            if(current<result.length){result.length=current;result.order.assign(order.begin(),order.begin()+n);}
        }
        if(!std::isfinite(result.length)){result.message="Route distances exceed supported numeric limits.";return result;}
        for(int pass=0;pass<16;pass++){
            if(canceled()){throw std::runtime_error("Canceled");}bool improved=false;
            for(int a=0;a<n;a++)for(int after=-1;after<n;after++){
                if(after==a||after==a-1)continue;
                int node=result.order[a]+1,previous=a?result.order[a-1]+1:0,next=a+1<n?result.order[a+1]+1:-1;
                int left=after<0?0:result.order[after]+1,right=after+1<n?result.order[after+1]+1:-1;
                double delta=-distance(previous,node)+(next>=0?distance(previous,next)-distance(node,next):0)+distance(left,node)+(right>=0?distance(node,right)-distance(left,right):0);
                if(delta<-.000001){int value=result.order[a];result.order.erase(result.order.begin()+a);int at=after<a?after+1:after;result.order.insert(result.order.begin()+at,value);result.length+=delta;improved=true;break;}
            }
            if(!improved)break;
        }
        result.length=0;int previous=0;for(int index:result.order){result.length+=distance(previous,index+1);previous=index+1;}
    }
    result.complete=true;result.message=result.exact?"Exact visit order for the modeled NavMesh distances (up to 15 crystals).":"Multi-start nearest neighbor + 2-opt + relocation; heuristic visit order.";return result;
}
inline RouteResult SolveOrder(const std::vector<std::vector<double>>& distances,const Cancel& canceled=[] {return false;},bool exactWhenPossible=true) {
    int size=static_cast<int>(distances.size());RouteWorkspace work;
    if(size<2||size>65)return {};
    for(int from=0;from<size;from++) {
        if(distances[from].size()!=static_cast<size_t>(size))throw std::runtime_error("Invalid routing matrix");
        std::copy(distances[from].begin(),distances[from].end(),work.matrix.begin()+from*size);
    }
    return SolveOrderFlat(work.matrix.data(),size,work,canceled,exactWhenPossible);
}
}
#include "PickupRouting.h"
namespace practice {
struct MapAnalysis {
    std::shared_ptr<const MapData> data;
    RouteResult route;
    RouteResult collectedRoute;
    double playedDistance=0,playedToLastPickup=0;
    TimingProfile timingProfile;TimingEstimate timing,collectedTiming;
    std::shared_ptr<const std::vector<std::pair<Vec3,Vec3>>> outline;
    std::shared_ptr<const std::vector<std::array<Vec3,3>>> floor;
    std::string error,cacheWarning;
    bool live=false,working=false,needsAnalysis=false;
    uint32_t session=0;
};
inline void ReviewTiming(MapAnalysis& analysis){
    if(!analysis.data||!analysis.data->review)return;
    const auto& d=*analysis.data;analysis.timingProfile=BuildTimingProfile(*d.review);auto movement=MovementFor(d);
    auto estimate=[&](const RouteResult& route){return movement?EstimateMovementRoute(*movement,d,route.line,route.complete&&!route.estimated):TimingEstimate{};};
    analysis.timing=estimate(analysis.route);analysis.collectedTiming=estimate(analysis.collectedRoute);
}
inline MapAnalysis AnalyzeMap(std::shared_ptr<const MapData> data,const NavGraph& graph,const Cancel& canceled=[] {return false;}) {
    MapAnalysis result;result.data=data;const auto& d=*data;result.floor=std::make_shared<const std::vector<std::array<Vec3,3>>>(graph.Floor());
    if(d.review){
        double last=d.review->pickupSeconds.empty()?0:d.review->pickupSeconds.back();
        for(size_t i=1;i<d.review->trace.size();i++){
            const auto& a=d.review->trace[i-1];const auto& b=d.review->trace[i];double length=Distance(a.point,b.point);result.playedDistance+=length;
            if(b.seconds<=last)result.playedToLastPickup+=length;
            else if(a.seconds<last&&b.seconds>a.seconds)result.playedToLastPickup+=length*(last-a.seconds)/(b.seconds-a.seconds);
        }
    }
    if(d.flags&128){result.route.message=IsHideSeekScene(d.scene)?"Capturing Hide & Seek floor and walls; route preparation will continue automatically.":"Capturing Unity navigation corridors; route preparation will continue automatically.";return result;}
    if(d.spawn<0||d.selected.empty()) {result.error=d.message;return result;}
    auto table=graph.Routes(canceled);
    int n=static_cast<int>(d.selected.size());std::vector<std::vector<NavPath>> paths(n+1,std::vector<NavPath>(n+1));
    std::vector<std::vector<uint8_t>> estimates(n+1,std::vector<uint8_t>(n+1));
    std::vector<std::vector<double>> distances(n+1,std::vector<double>(n+1,0));
    std::vector<int> contactIds{d.spawn};for(int id:d.selected)contactIds.push_back(int(d.spawns.size())+id);
    PickupRouting pickups(d,graph,contactIds,canceled);
    for(int from=0;from<=n;from++)for(int to=from+1;to<=n;to++){
        int a=contactIds[from],b=contactIds[to];paths[from][to]=table->Between(a,b);estimates[from][to]=table->estimates[a][b];
        distances[from][to]=distances[to][from]=table->lengths[a][b];pickups.SetLeg(from,to,paths[from][to],estimates[from][to]!=0,canceled);
    }
    result.route=SolveOrder(distances,canceled);
    std::vector<int> contactNodes(n+1);std::iota(contactNodes.begin(),contactNodes.end(),0);pickups.Optimize(result.route,contactNodes,canceled);
    bool routeEstimated=false;
    if(result.route.complete) {
        int previous=0,previousContact=0;size_t layer=0;
        for(int index:result.route.order) {
            int next=index+1,from=std::min(previous,next),to=std::max(previous,next);const auto& path=paths[from][to].points;
            bool legEstimated=estimates[from][to]!=0;
            if(result.route.pickupOptimized){int nextContact=result.route.contacts[layer++];legEstimated=pickups.Estimated(previous,previousContact,next,nextContact);for(Vec3 p:pickups.Line(previous,previousContact,next,nextContact))AppendRoutePoint(result.route,p,legEstimated);previousContact=nextContact;}
            else if(previous<next)for(Vec3 p:path)AppendRoutePoint(result.route,p,legEstimated);
            else for(auto it=path.rbegin();it!=path.rend();++it)AppendRoutePoint(result.route,*it,legEstimated);
            routeEstimated|=legEstimated;
            previous=next;
        }
        for(int& id:result.route.order)id=d.selected[id];
    }
    DescribeRouteEstimate(result.route,routeEstimated);
    result.route.message+=" Checked network: "+std::to_string(table->recoveredPairs)+" reused connections, "+std::to_string(table->repairAttempts)+" bounded local repair queries.";
    if(graph.UsesUnityCorridors())result.route.message="Captured Unity corridors. "+result.route.message;
    if(d.review){
        if(!d.review->collected.empty()){auto partial=std::make_shared<MapData>(d);partial->review.reset();partial->selected=d.review->collected;partial->keep=static_cast<int>(partial->selected.size());result.collectedRoute=AnalyzeMap(partial,graph,canceled).route;}
    }
    ReviewTiming(result);return result;
}
}
