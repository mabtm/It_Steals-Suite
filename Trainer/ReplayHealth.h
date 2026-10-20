#pragma once
#include "ReplayMotion.h"
#include <sstream>
#include <iomanip>
#include <atomic>
namespace practice {
enum class RecordingStatus {Complete,Partial,Missing,Unknown,NotNeeded};
struct RecordingChannel {
    const char* name="";RecordingStatus status=RecordingStatus::Unknown;std::string detail;
    size_t samples=0;double through=0;std::vector<CoverageGap> gaps;
};
struct RecordingCheck {std::array<RecordingChannel,6> channels;int complete=0,partial=0,missing=0,unknown=0;};
inline const char* RecordingStatusName(RecordingStatus status){
    switch(status){case RecordingStatus::Complete:return "Complete";case RecordingStatus::Partial:return "Partial";case RecordingStatus::Missing:return "Missing";case RecordingStatus::NotNeeded:return "Not needed";default:return "Unknown";}
}
inline void RecordingIssue(RecordingChannel& c,std::string reason,bool missing=false){
    if(missing)c.status=RecordingStatus::Missing;else if(c.status!=RecordingStatus::Missing)c.status=RecordingStatus::Partial;
    if(!c.detail.empty())c.detail+=" ";
    c.detail+=std::move(reason);
}
inline RecordingChannel CoverageChannel(const ReplayMotion& motion,const char* name,size_t channel,size_t samples,bool optional=false){
    RecordingChannel out;out.name=name;out.samples=samples;
    if(!motion.coverage.recorded){out.detail="Older recording without observation heartbeats. Sparse or unchanged values do not prove continuous capture.";return out;}
    const auto& span=motion.coverage.spans[channel];out.through=span.through;out.gaps=span.gaps;
    if(optional&&span.started&&span.sources==0){out.status=RecordingStatus::NotNeeded;out.detail="No sources of this type were bound during this recording.";return out;}
    if(!span.started||span.observations==0){out.status=RecordingStatus::Missing;out.detail="No successful observations were recorded for this channel.";return out;}
    out.status=RecordingStatus::Complete;out.detail=std::to_string(span.observations)+" observation checks / "+std::to_string(span.sources)+" sources. Complete at the recorded observation resolution.";
    if(span.errors)RecordingIssue(out,std::to_string(span.errors)+" observation errors.");
    if(span.gapCount)RecordingIssue(out,std::to_string(span.gapCount)+" observation gaps (showing at most 16).");
    if(span.through+CoverageLimits[channel]+.000001<motion.seconds&&span.gapCount==0){RecordingIssue(out,"Observation ended before the run.");out.gaps.push_back({span.through,motion.seconds});}
    return out;
}
template<class T>inline void RecordingTimes(RecordingChannel& c,const std::vector<T>& samples,double duration,double limit){
    if(samples.empty()){RecordingIssue(c,"No recorded samples.",true);return;}
    double prior=0;int gaps=0;
    for(const auto& sample:samples){if(sample.seconds-prior>limit+.000001){gaps++;if(c.gaps.size()<32)c.gaps.push_back({prior,sample.seconds});}prior=sample.seconds;}
    if(duration-prior>limit+.000001){gaps++;if(c.gaps.size()<32)c.gaps.push_back({prior,duration});}
    c.through=std::max(c.through,prior);if(gaps)RecordingIssue(c,std::to_string(gaps)+" gaps in the stored sample timeline.");
}
inline std::shared_ptr<const RecordingCheck> BuildRecordingCheck(const ReplayMotion& m){
    auto check=std::make_shared<RecordingCheck>();auto& rows=check->channels;
    rows[0]=CoverageChannel(m,"Movement",0,m.ticks.size());RecordingTimes(rows[0],m.ticks,m.seconds,.12);
    rows[1]=CoverageChannel(m,"Camera",1,m.frames.size());RecordingTimes(rows[1],m.frames,m.seconds,.25);
    rows[2]=CoverageChannel(m,"Inputs",2,m.devices.size());
    if(m.devices.empty())RecordingIssue(rows[2],"No device history is available.",true);
    else if(m.deviceThrough+.25<m.seconds)RecordingIssue(rows[2],"Device observation ended before the run.");
    if(m.deviceDropped)RecordingIssue(rows[2],std::to_string(m.deviceDropped)+" omitted device observations.");
    rows[3]=CoverageChannel(m,"Colliders",3,size_t(m.colliders.count));
    if(!m.colliders.recorded||m.colliders.tracks.empty())RecordingIssue(rows[3],"No recorded collider shapes are available.",true);
    else if(m.colliders.flags||m.colliders.dropped)RecordingIssue(rows[3],"Some collider shapes were unsupported, unavailable or omitted.");
    rows[4]=CoverageChannel(m,"Animation / visuals",4,size_t(m.visualSamples),true);
    if(!m.visualsRecorded||!m.visualSamples)RecordingIssue(rows[4],"No visual samples were recorded.",true);
    if(m.visualDropped)RecordingIssue(rows[4],std::to_string(m.visualDropped)+" omitted visual observations.");
    if(m.coverage.recorded){
        auto meta=CoverageChannel(m,"Animation metadata",5,size_t(m.experience.animations.size()),true);
        if(meta.status==RecordingStatus::Partial||meta.status==RecordingStatus::Missing){RecordingIssue(rows[4],"Animator/shape metadata: "+meta.detail);for(const auto& gap:meta.gaps)if(rows[4].gaps.size()<32)rows[4].gaps.push_back(gap);}
        for(const auto& track:m.experience.animations)if(track.frames.empty()){RecordingIssue(rows[4],"An animation track has no observations.");break;}
    }
    auto& sound=rows[5];sound.name="Audio";const auto& audio=m.experience.audio;sound.samples=size_t(std::max<int64_t>(0,audio.frames));sound.through=audio.Duration();
    if(!audio.recorded){sound.status=RecordingStatus::Missing;sound.detail="No recorded game audio is available.";}
    else if(!m.audioError.empty()){sound.status=RecordingStatus::Missing;sound.detail=m.audioError;}
    else if(m.audioFile.empty()){sound.status=RecordingStatus::Unknown;sound.detail="Audio companion has not been verified.";}
    else{sound.status=m.coverage.recorded?RecordingStatus::Complete:RecordingStatus::Unknown;sound.detail="Audio identity and full checksum verified. ";sound.detail+=m.coverage.recorded?"No reported gaps or missing routes.":"Historical recording without the current coverage checks.";}
    if(audio.recorded&&(audio.flags||audio.gaps)){
        if(sound.status!=RecordingStatus::Missing)sound.status=RecordingStatus::Partial;
        if((audio.flags&1)||audio.gaps)sound.detail+=" Missing PCM blocks or queue overrun.";
        if(audio.flags&2)sound.detail+=" Audio file limit reached.";
        if(audio.flags&4)sound.detail+=" An audible route could not be observed reliably.";
        if(audio.flags&8)sound.detail+=" Unsupported channels or invalid samples.";
        if(audio.flags&16)sound.detail+=" Audio storage error.";
    }
    if(audio.recorded&&sound.through+1.0/std::max(1,audio.rate)+.000001<m.seconds&&sound.status!=RecordingStatus::Missing){RecordingIssue(sound,"Audio ends before the run.");sound.gaps.push_back({sound.through,m.seconds});}
    if(m.coverage.recorded)rows[4].detail+=" Adaptive poses up to 120 Hz; position tolerance 0.00075 u / angle tolerance 0.15 degrees at sampled poses.";
    for(auto& row:rows){
        std::sort(row.gaps.begin(),row.gaps.end(),[](auto a,auto b){return a.begin<b.begin;});std::vector<CoverageGap> merged;
        for(auto gap:row.gaps){if(!merged.empty()&&gap.begin<=merged.back().end+.000001)merged.back().end=std::max(merged.back().end,gap.end);else if(merged.size()<32)merged.push_back(gap);}row.gaps=std::move(merged);
    }
    for(const auto& c:rows)switch(c.status){case RecordingStatus::Complete:check->complete++;break;case RecordingStatus::Partial:check->partial++;break;case RecordingStatus::Missing:check->missing++;break;case RecordingStatus::Unknown:check->unknown++;break;default:break;}
    return check;
}
inline std::shared_ptr<const RecordingCheck> RecordingCheckFor(const ReplayMotion& m){
    auto check=std::atomic_load(&m.recordingCheck);if(check)return check;
    auto created=BuildRecordingCheck(m);std::shared_ptr<const RecordingCheck> empty;
    if(std::atomic_compare_exchange_strong(&m.recordingCheck,&empty,created))return created;
    return empty;
}
inline std::string RecordingCheckReport(const ReplayMotion& m,const RecordingCheck& check){
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::fixed<<std::setprecision(6)<<"PracticeSuite recording check\nRun: "<<m.id<<" / scene: "<<m.scene<<" / seed: "<<m.seed<<" / seconds: "<<m.seconds<<" / input format: "<<m.inputFormat<<"\nRecording coverage at observed resolution; separate from run validity and origin verification.\n";
    for(const auto& c:check.channels){out<<c.name<<": "<<RecordingStatusName(c.status)<<" / samples: "<<c.samples<<" / observed through: "<<c.through<<"\n"<<c.detail<<"\n";for(const auto& gap:c.gaps)out<<"  Gap: "<<gap.begin<<" - "<<gap.end<<" s\n";}
    return out.str();
}
}
