#include "TrainerPreferences.h"
#include "InspectorModel.h"
#include <cassert>
#include <iostream>
#include <numeric>

int main(){
    using namespace inspector_fx;
    TrainerPreferences p;p.inspector={false,false,.28f,.77f,3.5f};
    auto q=TrainerPreferences::Decode(p.Encode());assert(!q.inspector.sounds&&!q.inspector.effects&&std::abs(q.inspector.volume-.28f)<1e-6&&std::abs(q.inspector.opacity-.77f)<1e-6&&q.inspector.outline==3.5f);
    auto old=TrainerPreferences::Decode("version=18\nseed=7114\n");assert(old.seed==7114&&old.inspector.sounds&&old.inspector.effects&&old.inspector.opacity==.96f);
    auto bad=TrainerPreferences::Decode("inspector.volume=99\ninspector.opacity=-20\ninspector.outline=1e99\ninspector.sounds=2\ninspector.effects=nan\n");assert(bad.inspector.volume==1&&bad.inspector.opacity==.65f&&bad.inspector.outline==4&&bad.inspector.sounds&&bad.inspector.effects);
    Feedback f;Preferences local;ExperiencePreferences master;
    f.Emit(suite_ui::Tone::InspectorOpen,10,local,master,false);assert(f.Window(10.1,local,master));auto cue=f.Take(10.01,true);assert(cue.tone==suite_ui::Tone::InspectorOpen&&cue.gain==local.volume&&f.Take(10.02,true).tone==suite_ui::Tone::None);assert(f.Window(10.1,local,master)&&!f.Window(10.2,local,master));
    f.Emit(suite_ui::Tone::InspectorPin,11,local,master,false);assert(f.Take(11.2,true).tone==suite_ui::Tone::None);
    for(int reason=0;reason<5;reason++){f.Reset();local={};master={};if(reason==0)local.sounds=false;if(reason==1)local.volume=0;if(reason==2)master.sounds=false;if(reason==3)master.volume=0;f.Emit(suite_ui::Tone::InspectorSelect,20,local,master,reason==4);assert(!f.Window(20,local,master)&&f.Take(20,true).tone==suite_ui::Tone::None);}
    local={};master={};f.Emit(suite_ui::Tone::InspectorClose,21,local,master,false);assert(f.Take(21.01,false).tone==suite_ui::Tone::None&&f.Take(21.02,true).tone==suite_ui::Tone::None);
    for(auto tone:{suite_ui::Tone::InspectorOpen,suite_ui::Tone::InspectorSelect,suite_ui::Tone::InspectorPin,suite_ui::Tone::InspectorClose}){
        auto full=suite_ui::ToneSamples(tone,1),half=suite_ui::ToneSamples(tone,.5f),quiet=suite_ui::ToneSamples(tone,.16f*.65f);assert(full.size()<=4000&&full.front()==0&&full.back()==0&&full.size()==quiet.size());int peak=0;for(size_t i=0;i<full.size();i++){peak=std::max(peak,std::abs(int(full[i])));assert(std::abs(int(full[i])-2*int(half[i]))<=3&&std::abs(int(quiet[i]))<700);}assert(peak>1000&&peak<6000);double mean=std::accumulate(full.begin(),full.end(),0.)/full.size();assert(std::abs(mean)<50);
    }
    Changes changes;inspector::Detail d;d.header.pid=1;d.header.session=2;d.header.epoch=3;d.header.revision=1;d.selected=101;inspector::Value v;v.key=42;v.value="Idle";d.values.push_back(v);changes.Observe(d,0);assert(changes.Pulse(42,0)==0);
    d.values[0].value="Moving";d.header.revision++;changes.Observe(d,1);assert(changes.Pulse(42,1)==1);changes.Observe(d,1.2);assert(changes.Pulse(42,1.2)>.6f&&changes.Pulse(42,1.7)==0);
    d.header.session++;changes.Observe(d,2);assert(changes.Pulse(42,2)==0);d.header.epoch++;d.header.revision=1;changes.Observe(d,3);assert(changes.Pulse(42,3)==0);d.selected++;changes.Observe(d,4);assert(changes.Pulse(42,4)==0);
    for(size_t i=0;i<1500;i++){v.key=100+i;d.values.push_back(v);}d.header.revision++;changes.Observe(d,5);assert(changes.Size()==Changes::Capacity);
    std::cout<<"7.11.5 Inspector presentation: settings migration/round-trip, independent/master mute, active-run lock, focus discard, short expiring cues, PCM gain/envelopes, publication-based value pulses, identity reset and bounded cache: PASS\n";
}
