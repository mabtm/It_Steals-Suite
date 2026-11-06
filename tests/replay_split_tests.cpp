#include "ReplaySplitModel.h"
#include <cassert>
#include <iostream>
int main()
{
    using namespace practice;
    MapData a; a.scene = 3; a.seed = 42; a.hash = "layout"; a.selected = {0, 1, 2}; a.spawn = 0; a.spawns = {{0,0,0}};
    a.points = {{0,0,0}, {1,0,0}, {2,0,0}};
    auto r = std::make_shared<RunReview>(); r->category = 1; r->selectedSeed = 42;
    r->collected = {0,1,2}; r->pickupSeconds = {2,5,9}; a.review = r;
    auto b = a; auto ref = std::make_shared<RunReview>(*r); b.review = ref;
    ref->pickupSeconds = {3,4,10};
    auto splits = PickupSplits(a, &b);
    assert(splits.size() == 3 && splits[0].delta == -1 && splits[1].delta == 1 && splits[2].segment == 4);
    ref->collected = {0,2,1}; splits = PickupSplits(a, &b);
    assert(splits[0].comparable && splits[1].comparable && splits[2].comparable);
    assert(splits[1].delta == -5 && splits[2].delta == 5);
    assert(splits[0].sectorComparable && !splits[1].sectorComparable && !splits[2].sectorComparable);
    ref->collected={0,1,2}; ref->pickupSeconds={3,6,10};
    splits=PickupSplits(a,&b);
    assert(splits[0].sectorDelta==-1 && splits[1].sectorDelta==0 && splits[2].sectorDelta==0);
    auto estimate=[&](double now,bool complete=false) { return ProjectedFinish(splits,ref->collected,ref->pickupSeconds,12,now,complete); };
    assert(estimate(0)==12 && estimate(2)==11 && estimate(5)==11 && estimate(9)==11);
    assert(estimate(7)==11 && estimate(8)==11);
    auto slow=splits; slow[2].at=11;
    assert(ProjectedFinish(slow,ref->collected,ref->pickupSeconds,12,10)==12); // overdue sector raises forecast before pickup
    assert(estimate(20)==20 && estimate(10,true)==10);
    auto diverged=splits; diverged[1].crystal=2;
    assert(!ProjectedFinish(diverged,ref->collected,ref->pickupSeconds,12,5));
    assert(!ProjectedFinish(splits,ref->collected,ref->pickupSeconds,4,5));
    assert(!ProjectedFinish(splits,ref->collected,ref->pickupSeconds,12,5,true));
    ref->rawSpawnSeed = true; assert(!MatchingSplitLayout(a,b));
    ref->rawSpawnSeed = false; ref->category = 0; assert(!MatchingSplitLayout(a,b));
    ref->category = 1; b.seed++; assert(!MatchingSplitLayout(a,b));
    b.seed--; b.points[1].x += .01f; assert(!MatchingSplitLayout(a,b));
    assert(!PickupSplits(a, nullptr)[0].comparable);
    std::cout << "Observed pickup/segment times, signed deltas, strict mode/category/recipe/layout matching and route divergence: PASS\n";
}
