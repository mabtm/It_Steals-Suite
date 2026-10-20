#pragma once
// Included after NavGraph: one immutable table of checked corridors per catalog.
namespace practice {
struct PreparedRoutes {
    int count=0,checkedPairs=0,recoveredPairs=0,unverifiedPairs=0,repairAttempts=0;
    std::vector<NavProjection> projected;
    std::vector<std::vector<double>> lengths;
    std::vector<std::vector<uint8_t>> estimates;
    std::vector<NavPath> paths;
    size_t Slot(int a,int b)const{return size_t(std::min(a,b))*count+std::max(a,b);}
    NavPath Between(int a,int b)const{
        auto result=paths.at(Slot(a,b));if(a>b)std::reverse(result.points.begin(),result.points.end());return result;
    }
    PreparedRoutes(const NavGraph& graph,const Cancel& canceled,const std::function<void(int,int)>& progress){
        const auto& positions=graph.CatalogPositions();count=int(positions.size());if(count>384)throw std::runtime_error("Navigation catalog exceeds table limit");
        projected.reserve(count);for(int i=0;i<count;i++){if(canceled()){throw std::runtime_error("Canceled");}projected.push_back(graph.CatalogProject(i,positions[i]));}
        paths.resize(size_t(count)*count);lengths.assign(count,std::vector<double>(count,Unreachable));estimates.assign(count,std::vector<uint8_t>(count));
        std::vector<int> parent(count);std::iota(parent.begin(),parent.end(),0);
        auto root=[&](int id){while(parent[id]!=id){parent[id]=parent[parent[id]];id=parent[id];}return id;};
        auto join=[&](int a,int b){a=root(a);b=root(b);if(a!=b)parent[a]=b;};
        size_t stored=0;auto keep=[&](int a,int b,NavPath path){
            if(!std::isfinite(path.length)||path.points.empty())return false;
            stored+=path.points.size();if(stored>2000000)throw std::runtime_error("Checked navigation table exceeds point budget");
            lengths[a][b]=lengths[b][a]=path.length;paths[Slot(a,b)]=std::move(path);join(a,b);return true;
        };
        // First validate the existing triangle/Unity corridors without doing
        // the same expensive local obstacle repair for every catalog pair.
        for(int from=0;from<count;from++){
            if(canceled()){throw std::runtime_error("Canceled");}lengths[from][from]=0;
            auto tree=graph.Search(projected[from],canceled);
            for(int to=from+1;to<count;to++){if(canceled()){throw std::runtime_error("Canceled");}if(keep(from,to,graph.Path(tree,projected[to],canceled,false)))checkedPairs++;}
            if(progress)progress(from+1,count*2);
        }
        // Refine nearby missing corridors before closure. Local connections
        // improve route quality without repairing every O(catalog^2) pair.
        std::vector<uint8_t> attempted(size_t(count)*count);std::vector<std::pair<double,std::pair<int,int>>> nearby;
        for(int a=0;a<count;a++){
            std::vector<std::pair<double,int>> nearest;
            if(projected[a].triangle<0)continue;
            for(int b=0;b<count;b++)if(a!=b&&projected[b].triangle>=0&&!std::isfinite(lengths[a][b]))nearest.push_back({Distance(projected[a].point,projected[b].point),b});
            std::sort(nearest.begin(),nearest.end());
            for(size_t i=0;i<std::min(size_t(4),nearest.size());i++){int b=nearest[i].second;nearby.push_back({nearest[i].first,{std::min(a,b),std::max(a,b)}});}
        }
        std::sort(nearby.begin(),nearby.end());int refinements=0;
        for(const auto& candidate:nearby){
            if(canceled()){throw std::runtime_error("Canceled");}int a=candidate.second.first,b=candidate.second.second;size_t slot=Slot(a,b);
            if(attempted[slot]||std::isfinite(lengths[a][b]))continue;
            if(refinements>=512)break;
            attempted[slot]=1;refinements++;repairAttempts++;
            if(keep(a,b,graph.Path(graph.Search(projected[a],canceled),projected[b],canceled,true)))checkedPairs++;
        }
        // Repair only bridges between disconnected checked components. Every
        // resulting edge still uses the existing floor + capsule checks.
        struct Bridge {int a,b;double lower;};std::vector<Bridge> bridges;
        for(int a=0;a<count;a++)for(int b=a+1;b<count;b++)if(root(a)!=root(b)&&projected[a].triangle>=0&&projected[b].triangle>=0)bridges.push_back({a,b,Distance(projected[a].point,projected[b].point)});
        std::sort(bridges.begin(),bridges.end(),[](const auto& a,const auto& b){return a.lower<b.lower||(a.lower==b.lower&&std::pair(a.a,a.b)<std::pair(b.a,b.b));});
        for(const auto& edge:bridges){
            if(canceled()){throw std::runtime_error("Canceled");}if(root(edge.a)==root(edge.b))continue;
            if(attempted[Slot(edge.a,edge.b)])continue;
            if(repairAttempts-refinements>=std::min(256,count*4)){break;}repairAttempts++;attempted[Slot(edge.a,edge.b)]=1;
            auto tree=graph.Search(projected[edge.a],canceled);if(keep(edge.a,edge.b,graph.Path(tree,projected[edge.b],canceled,true)))checkedPairs++;
        }
        // Shortest-path closure reuses real, checked polylines. A missing
        // direct corridor is not permission to replace it by a Euclidean leg.
        auto distance=lengths;std::vector<int16_t> next(size_t(count)*count,-1);
        for(int a=0;a<count;a++)for(int b=0;b<count;b++)if(a!=b&&std::isfinite(distance[a][b]))next[size_t(a)*count+b]=int16_t(b);
        for(int k=0;k<count;k++){
            if(canceled()){throw std::runtime_error("Canceled");}
            for(int a=0;a<count;a++)if(a!=k&&std::isfinite(distance[a][k]))for(int b=0;b<count;b++){
                double through=distance[a][k]+distance[k][b];if(through+1e-7<distance[a][b]){distance[a][b]=through;next[size_t(a)*count+b]=next[size_t(a)*count+k];}
            }
        }
        auto base=std::move(paths);paths.resize(size_t(count)*count);stored=0;
        for(int a=0;a<count;a++){
            for(int b=a+1;b<count;b++){
                if(canceled()){throw std::runtime_error("Canceled");}NavPath route;
                if(std::isfinite(distance[a][b])){
                    int at=a,steps=0;
                    while(at!=b){
                        if(++steps>count){throw std::runtime_error("Cyclic checked navigation table");}int to=next[size_t(at)*count+b];if(to<0||to==at)throw std::runtime_error("Missing checked navigation link");
                        const auto& leg=base[Slot(at,to)];if(!std::isfinite(leg.length)||leg.points.empty())throw std::runtime_error("Checked navigation link lost its polyline");
                        auto append=[&](Vec3 point){if(route.points.empty()||!Same(route.points.back(),point))route.points.push_back(point);};
                        if(at<to)for(auto point:leg.points)append(point);else for(auto it=leg.points.rbegin();it!=leg.points.rend();++it)append(*it);
                        at=to;
                    }
                    route.length=0;for(size_t i=1;i<route.points.size();i++)route.length+=Distance(route.points[i-1],route.points[i]);
                    auto shorter=graph.Polish(route,canceled);if(std::isfinite(shorter.length)&&shorter.length<=route.length+1e-6)route=std::move(shorter);
                    if(!std::isfinite(lengths[a][b]))recoveredPairs++;
                }else{
                    // Retain accounting for playable seeds with genuinely
                    // missing capture data, while marking only these legs.
                    Vec3 from=projected[a].triangle>=0?projected[a].point:positions[a],to=projected[b].triangle>=0?projected[b].point:positions[b];
                    route={{from,to},Distance(from,to)};estimates[a][b]=estimates[b][a]=1;unverifiedPairs++;
                }
                stored+=route.points.size();if(stored>2000000)throw std::runtime_error("Checked navigation table exceeds point budget");
                lengths[a][b]=lengths[b][a]=route.length;paths[Slot(a,b)]=std::move(route);
            }
            if(progress)progress(count+a+1,count*2);
        }
    }
};
inline std::shared_ptr<const PreparedRoutes> NavGraph::Routes(const Cancel& canceled,const std::function<void(int,int)>& progress)const{
    std::call_once(preparedOnce,[&]{preparedRoutes=std::make_shared<const PreparedRoutes>(*this,canceled,progress);});
    if(canceled()){throw std::runtime_error("Canceled");}if(progress)progress(preparedRoutes->count*2,preparedRoutes->count*2);return preparedRoutes;
}
}
