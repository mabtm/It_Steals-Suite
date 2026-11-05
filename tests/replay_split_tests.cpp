#include "ReplaySplitModel.h"
#include <cassert>
#include <iostream>
int main()
{
    using namespace practice;
    MapData a; a.scene = 3; a.seed = 42; a.hash = "layout"; a.selected = {0, 1, 2};
    a.points = {{0,0,0}, {1,0,0}, {2,0,0}};
    auto r = std::make_shared<RunReview>(); r->category = 1; r->selectedSeed = 42;
    r->collected = {0,1,2}; r->pickupSeconds = {2,5,9}; a.review = r;
    auto b = a; auto ref = std::make_shared<RunReview>(*r); b.review = ref;
    ref->pickupSeconds = {3,4,10};
    auto splits = PickupSplits(a, &b);
    assert(splits.size() == 3 && splits[0].delta == -1 && splits[1].delta == 1 && splits[2].segment == 4);
    ref->collected = {0,2,1}; splits = PickupSplits(a, &b);
    assert(splits[0].comparable && !splits[1].comparable && !splits[2].comparable);
    ref->rawSpawnSeed = true; assert(!MatchingSplitLayout(a,b));
    ref->rawSpawnSeed = false; ref->category = 0; assert(!MatchingSplitLayout(a,b));
    ref->category = 1; b.seed++; assert(!MatchingSplitLayout(a,b));
    b.seed--; b.points[1].x += .01f; assert(!MatchingSplitLayout(a,b));
    assert(!PickupSplits(a, nullptr)[0].comparable);
    std::cout << "Observed pickup/segment times, signed deltas, strict mode/category/recipe/layout matching and route divergence: PASS\n";
}
