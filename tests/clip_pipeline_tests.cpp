#include "ClipFrameWire.h"
#include "ClipModel.h"
#include <cassert>
#include <iostream>
#include <random>
using namespace suite_clip;
int main(){
    std::mt19937 random(790);
    for(uint64_t total:{1ull,2ull,3ull,4ull,60ull,10000ull}){
        FrameWindow window;window.Reset(total);std::vector<uint64_t> pending;std::vector<double> source;
        Options o;o.a=0;o.b=double(total)/120;o.fps=120;
        while(window.Consumed()<total){
            while(window.CanRequest())pending.push_back(window.Request());
            assert(window.InFlight()<=FrameSlots);
            if(!pending.empty()){size_t index=random()%pending.size();window.Complete(pending[index]);pending.erase(pending.begin()+std::ptrdiff_t(index));}
            while(window.Ready()){auto frame=window.Consume();assert(frame==source.size());source.push_back(o.At(frame));}
        }
        assert(source.size()==total&&window.InFlight()==0);
        for(size_t i=0;i<source.size();i++)assert(std::abs(source[i]-double(i)/120)<1e-10);
    }
    FrameWindow window;window.Reset(10);assert(window.Request()==0);assert(window.Request()==1);assert(window.Request()==2);assert(!window.CanRequest());window.Complete(2);assert(!window.Ready());window.Complete(0);assert(window.Consume()==0&&window.CanRequest());assert(window.Request()==3);window.Complete(1);assert(window.Consume()==1&&window.Consume()==2);window.Complete(3);assert(window.Consume()==3);
    bool rejected=false;try{window.Complete(0);}catch(const std::logic_error&){rejected=true;}assert(rejected);window.Reset(1);assert(window.Request()==0);window.Complete(0);assert(window.Consume()==0&&!window.CanRequest());
    std::cout<<"Production frame window: 1/2/3/4/60/10000 frames, random out-of-order completion, exact 120 FPS source timestamps, bounded backpressure, slot reuse, stale-frame rejection and cancellation/reset: PASS\n";
}
