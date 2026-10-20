#pragma once
#include <span>
namespace practice {
// Finite contact samples preserve one continuous position per pickup. Pair
// minima are used only as lower bounds, never as a finished route's score.
class PickupRouting {
    static constexpr int Samples=137; // center + 8 boundary samples + at most 128 spawns
    struct Leg {
        // Mode bits: 1 interior pivots, 2/4 center connectors, 8 full center path, 16 unverified.
        NavPath path;double lower=Unreachable;std::vector<double> cost;std::vector<uint8_t> mode;
    };
    const NavGraph& graph;
    std::vector<std::vector<Vec3>> contacts;
    std::vector<std::vector<uint8_t>> toCenter;
    std::vector<Leg> legs;
    bool active=false;
    size_t Slot(int a,int b)const{return size_t(a)*contacts.size()+b;}
    const Leg& Pair(int a,int b)const{return legs[Slot(std::min(a,b),std::max(a,b))];}
    double Cost(int a,int ca,int b,int cb)const{return a<b?Pair(a,b).cost[ca*contacts[b].size()+cb]:Pair(a,b).cost[cb*contacts[a].size()+ca];}
public:
    PickupRouting(const MapData& data,const NavGraph& nav,const std::vector<int>& ids,const Cancel& canceled=[] {return false;}):graph(nav){
        const int spawns=int(data.spawns.size());contacts.resize(ids.size());toCenter.resize(ids.size());
        bool enabled=data.physicalCaptured&&data.physicalOmitted==0&&ids.size()<=256&&data.pickupRegions.size()==data.points.size();
        for(size_t i=0;i<ids.size();i++){
            if(canceled())throw std::runtime_error("Canceled");
            int id=ids[i],point=id-spawns;Vec3 original=id<spawns?data.spawns[id]:data.points[point];auto p=graph.CatalogProject(id,original);
            // A missing captured center anchor must not erase valid contacts
            // on the common floor triangulation. This does not insert a floor
            // triangle index into a captured-corridor search tree.
            bool recovered=false;if(enabled&&point>=0&&data.pickupRegions[point].verified&&p.triangle<0){
                auto floor=graph.FloorProject(original);const auto& region=data.pickupRegions[point];
                if(floor.triangle>=0&&std::hypot(floor.point.x-region.center.x,floor.point.z-region.center.z)<=region.radius+.001&&graph.Visible(floor.point,floor.point)){p=floor;recovered=true;}
            }
            Vec3 center=p.triangle>=0?p.point:original;contacts[i].push_back(center);toCenter[i].push_back(1);
            if(!enabled||point<0||!data.pickupRegions[point].verified||p.triangle<0)continue;
            const auto& region=data.pickupRegions[point];
            if(std::hypot(center.x-region.center.x,center.z-region.center.z)>region.radius+.001)continue;
            active|=recovered;
            for(int k=0;k<8;k++){
                double angle=k*.7853981633974483;Vec3 sample{float(region.center.x+std::cos(angle)*region.radius),center.y,float(region.center.z+std::sin(angle)*region.radius)};
                auto q=graph.FloorProject(sample);
                if(q.triangle<0||std::hypot(q.point.x-sample.x,q.point.z-sample.z)>.005||std::abs(q.point.y-center.y)>.3||std::hypot(q.point.x-region.center.x,q.point.z-region.center.z)>region.radius+.001||!graph.Visible(q.point,q.point))continue;
                if(Distance(q.point,center)<.002)continue;
                contacts[i].push_back(q.point);toCenter[i].push_back(graph.Visible(q.point,center));active=true;
            }
            // A run can spawn already overlapping a trigger. Preserve that
            // exact contact, instead of making it walk to a sampled boundary.
            for(size_t spawn=0;spawn<data.spawns.size();spawn++){
                auto start=graph.CatalogProject(int(spawn),data.spawns[spawn]);if(start.triangle<0||std::hypot(start.point.x-region.center.x,start.point.z-region.center.z)>region.radius||std::abs(start.point.y-center.y)>.3||!graph.Visible(start.point,start.point))continue;
                if(std::any_of(contacts[i].begin(),contacts[i].end(),[&](Vec3 p){return Same(p,start.point);}))continue;
                contacts[i].push_back(start.point);toCenter[i].push_back(graph.Visible(start.point,center));active=true;
            }
        }
        size_t total=0;for(const auto& points:contacts)total+=points.size();if(total>2000)active=false; // bounded shared distance storage
        if(active)legs.resize(contacts.size()*contacts.size());
    }
    bool Active()const{return active;}
    Vec3 Anchor(int node)const{return contacts.at(node).front();}
    void SetLeg(int a,int b,NavPath path,bool estimated,const Cancel& canceled=[] {return false;}){
        if(!active||a==b)return;
        if(a>b){std::swap(a,b);std::reverse(path.points.begin(),path.points.end());}
        auto& leg=legs[Slot(a,b)];leg.path=std::move(path);const auto& line=leg.path.points;if(line.empty())return;leg.cost.assign(contacts[a].size()*contacts[b].size(),Unreachable);leg.mode.assign(leg.cost.size(),16);
        const bool inner=line.size()>2;double middle=0;if(inner)for(size_t k=2;k+1<line.size();k++)middle+=Distance(line[k-1],line[k]);
        std::array<double,Samples> left{},right{};std::array<uint8_t,Samples> lm{},rm{};
        if(inner){
            auto connectors=[&](int node,bool first,auto& costs,auto& modes){
                Vec3 pivot=first?line[1]:line[line.size()-2],center=first?line.front():line.back();
                for(size_t i=0;i<contacts[node].size();i++){
                    Vec3 p=contacts[node][i];if(i==0||(!estimated&&graph.Visible(p,pivot))){costs[i]=Distance(p,pivot);modes[i]=0;}
                    else if(!estimated&&toCenter[node][i]){costs[i]=Distance(p,center)+Distance(center,pivot);modes[i]=1;}else costs[i]=Unreachable;
                }
            };connectors(a,true,left,lm);connectors(b,false,right,rm);
        }
        for(size_t i=0;i<contacts[a].size();i++){if(canceled())throw std::runtime_error("Canceled");for(size_t j=0;j<contacts[b].size();j++){
            size_t slot=i*contacts[b].size()+j;
            if(!estimated&&inner){leg.cost[slot]=left[i]+middle+right[j];leg.mode[slot]=uint8_t(1|(lm[i]?2:0)|(rm[j]?4:0));}
            else if(i==0&&j==0){leg.cost[slot]=leg.path.length;leg.mode[slot]=estimated?16:0;}
            else if(!estimated&&toCenter[a][i]&&toCenter[b][j]){leg.cost[slot]=Distance(contacts[a][i],line.front())+leg.path.length+Distance(line.back(),contacts[b][j]);leg.mode[slot]=8;}
            // A trigger may extend around a corner or through a thin wall.
            // A directly visible physical contact need not follow the center's
            // detour. Only the movement capsule must stay clear of the wall.
            // Validate contacts independently even when no center path exists.
            // A physically verified connection takes priority over an unknown
            // straight-line estimate, while its real length remains unchanged.
            if(graph.Visible(contacts[a][i],contacts[b][j])){double direct=Distance(contacts[a][i],contacts[b][j]);if((leg.mode[slot]&16)||direct<leg.cost[slot]){leg.cost[slot]=direct;leg.mode[slot]=0;}}
        }}
        // The original checked path is always a reachable fallback, including
        // archives whose recorded goal lies just beyond a NavMesh boundary.
        if(!(leg.mode[0]&16))leg.cost[0]=std::min(leg.cost[0],leg.path.length);
        leg.lower=*std::min_element(leg.cost.begin(),leg.cost.end());
    }
    double Lower(int a,int b)const{if(a==b)return 0;return Pair(a,b).lower;}
    bool Estimated(int a,int ca,int b,int cb)const{if(a>b){std::swap(a,b);std::swap(ca,cb);}return (Pair(a,b).mode[ca*contacts[b].size()+cb]&16)!=0;}
    void Optimize(RouteResult& result,std::span<const int> nodes,const Cancel& canceled)const{
        if(!active||!result.complete||result.order.empty())return;
        std::array<double,Samples> previous,next;previous.fill(Unreachable);previous[0]=0;
        std::array<uint8_t,Samples> previousUnknown,nextUnknown;previousUnknown.fill(255);previousUnknown[0]=0;
        std::array<std::array<int16_t,Samples>,64> parent;int from=nodes[0];
        for(size_t layer=0;layer<result.order.size();layer++){
            if(canceled())throw std::runtime_error("Canceled");
            int to=nodes[result.order[layer]+1];std::fill_n(next.begin(),contacts[to].size(),Unreachable);std::fill_n(nextUnknown.begin(),contacts[to].size(),uint8_t{255});
            for(size_t j=0;j<contacts[to].size();j++)for(size_t i=0;i<contacts[from].size();i++){
                double cost=previous[i]+Cost(from,int(i),to,int(j));if(!std::isfinite(cost))continue;
                int unknown=previousUnknown[i]+int(Estimated(from,int(i),to,int(j)));
                if(unknown<nextUnknown[j]||(unknown==nextUnknown[j]&&cost<next[j])){next[j]=cost;nextUnknown[j]=uint8_t(unknown);parent[layer][j]=int16_t(i);}
            }
            std::copy_n(next.begin(),contacts[to].size(),previous.begin());std::copy_n(nextUnknown.begin(),contacts[to].size(),previousUnknown.begin());from=to;
        }
        int at=0;for(size_t i=1;i<contacts[from].size();i++)if(previousUnknown[i]<previousUnknown[at]||(previousUnknown[i]==previousUnknown[at]&&previous[i]<previous[at]))at=int(i);
        if(!std::isfinite(previous[at]))return;
        result.estimated=previousUnknown[at]!=0;result.length=previous[at];result.contacts.resize(result.order.size());for(int layer=int(result.order.size())-1;layer>=0;layer--){result.contacts[layer]=at;at=parent[layer][at];}
        result.pickupOptimized=true;result.exact=false;result.message="Continuous standing/level-camera pickup contacts from the actual camera trigger; center-based visit order with 8 boundary samples. No claim of a global shortest route.";
    }
    std::vector<Vec3> Line(int a,int ca,int b,int cb)const{
        bool reverse=a>b;if(reverse){std::swap(a,b);std::swap(ca,cb);}const auto& leg=Pair(a,b);size_t slot=ca*contacts[b].size()+cb;uint8_t mode=leg.mode[slot];const auto& p=leg.path.points;std::vector<Vec3> line;
        auto append=[&](Vec3 v){if(line.empty()||!Same(line.back(),v))line.push_back(v);};
        append(contacts[a][ca]);
        if(mode&8){for(auto v:p)append(v);}
        else if(mode&1){if(mode&2)append(p.front());for(size_t i=1;i+1<p.size();i++)append(p[i]);if(mode&4)append(p.back());}
        append(contacts[b][cb]);if(reverse)std::reverse(line.begin(),line.end());return line;
    }
};
}
