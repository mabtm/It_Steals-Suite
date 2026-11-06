#include "SplitDisplay.h"
#include <cassert>
#include <iostream>
int main()
{
    // The real wire struct deliberately has unaligned tail fields. Exercise
    // live math on it, rather than substituting aligned vector-only fixtures.
    alignas(16) GameData d{};
    d.pid=725;d.runCategory=1;d.timerState=1;d.runValidity=1;
    d.splitRunId=725;d.splitReferenceId=724;d.splitReferenceComplete=d.splitHistoryReady=1;
    d.splitPickupCount=d.splitReferenceCount=d.splitExpectedPickups=3;
    d.splitReferenceFinish=12;d.splitTheoreticalBest=10;d.splitBestExit=1.5;d.currentRunTimeSeconds=8;
    for(int i=0;i<3;i++)
    {
        d.splitPickupIds[i]=d.splitReferenceIds[i]=i;
        d.splitPickupSeconds[i]=2+i*3;d.splitReferenceSeconds[i]=3+i*3;
    }
    d.splitSectorBest[0]=2.5;d.splitSectorBest[1]=2.9;d.splitSectorBest[2]=3;
    auto rows=suite_splits::Live(d);
    assert(rows.size()==3 && rows[0].gold && !rows[1].gold && rows[1].sectorDelta==0);
    assert(suite_splits::Pace(d,rows,8)==11 && suite_splits::Pace(d,rows,13)==13);
    assert(std::abs(*suite_splits::Theoretical(d,rows)-9.5)<1e-6);
    d.runValidity=2;rows=suite_splits::Live(d);
    assert(!rows[0].gold && suite_splits::Theoretical(d,rows)==10);
    d.runValidity=1;d.timerState=2;d.runOutcome=1;d.currentRunTimeSeconds=10;
    rows=suite_splits::Live(d);
    assert(suite_splits::Pace(d,rows,10)==10 && std::abs(*suite_splits::Theoretical(d,rows)-9.4)<1e-6);
    d.splitHistoryReady=0;rows=suite_splits::Live(d);
    assert(!suite_splits::Theoretical(d,rows)); // unknown/corrupt historical data cannot claim a best
    d.timerState=1;d.runOutcome=0;d.splitHistoryReady=1;d.splitRunId++;
    int first=d.splitPickupIds[1];d.splitPickupIds[1]=d.splitPickupIds[2];d.splitPickupIds[2]=first;rows=suite_splits::Live(d);
    assert(rows[1].comparable && !rows[1].sectorComparable && !suite_splits::Pace(d,rows,8));
    assert(!suite_splits::Theoretical(d,rows));
    d.splitRunId++;d.splitPickupCount=0;rows=suite_splits::Live(d);assert(rows.empty());
    assert(suite_splits::Pace(d,rows,0)==12 && suite_splits::Theoretical(d,rows)==10);
    d.splitReferenceComplete=0;assert(!suite_splits::Pace(d,rows,0));
    d.splitReferenceComplete=1;d.splitRunId++;d.splitPickupCount=3;
    for(int i=0;i<3;i++){d.splitPickupIds[i]=i;d.splitPickupSeconds[i]=i==2?1:0;d.splitReferenceSeconds[i]=i==2?2:0;d.splitSectorBest[i]=-1;}
    d.splitReferenceFinish=3;rows=suite_splits::Live(d);
    assert(rows[0].segment==0 && rows[1].segment==0 && rows[1].sectorComparable && rows[1].sectorDelta==0);
    assert(suite_splits::Pace(d,rows,1)==2);
    std::cout<<"Actual packed IPC live splits: sector/total/gold, precise pace, overdue sectors, route isolation, provisional/finished theory, missing history, zero sectors and reset invalidation: PASS\n";
}
