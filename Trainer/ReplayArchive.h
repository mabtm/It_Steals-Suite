#pragma once
#include "PracticeMapData.h"
#include "ReplayPackage.h"
#include "ReplayAudioIO.h"
#include "ReplayHealth.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <thread>
namespace practice {
struct ReplayInfo {
    int64_t id=0;int scene=0,seed=0,outcome=0;double seconds=0;bool valid=false,saved=false;
    std::string name,category,notes,reason;std::filesystem::path file;
};
inline bool ReplayAborted(int outcome){return outcome!=1;}
inline const char* ReplayStatusName(int outcome,bool valid){return ReplayAborted(outcome)?"ABORTED":valid?"VALID":"UNVALID";}
inline std::string ReplayName(const MapData& data){
    if(!data.review)return "Run";
    int64_t id=data.review->id;constexpr int64_t epoch=621355968000000000ll;
    if(id<epoch||id>3155378975999999999ll)return "Run "+std::to_string(id);
    std::time_t time=std::time_t((id-epoch)/10000000);std::tm local{};
#ifdef _WIN32
    if(localtime_s(&local,&time)!=0)return "Run "+std::to_string(id);
#else
    if(!localtime_r(&time,&local))return "Run "+std::to_string(id);
#endif
    char stamp[32]{};std::strftime(stamp,sizeof(stamp),"%Y-%m-%d %H:%M",&local);int level=MapLevelIndex(data.scene);
    return std::string(level>=0?kMapLevelNames[level]:"Run")+" | "+stamp;
}
struct ReplayArchiveState {std::vector<ReplayInfo> runs;std::string error,motionError,exportPath;bool busy=false;uint64_t savedRevision=0;int64_t annotationRun=0;uint64_t annotationRevision=0;std::string annotationError;int64_t savedRun=0,motionRun=0,exportedRun=0,stagedRun=0;};
// All directory walks, bounded decoding, loads and atomic writes happen on a
// sleeping IO worker. The UI receives immutable snapshots, never file handles.
class ReplayArchive {
    struct Job {int kind=0;int64_t id=0;std::filesystem::path path;std::shared_ptr<const MapData> data;std::string name,category,notes;std::shared_ptr<const ReplayAnnotations> annotations;};
    struct Cached {uintmax_t size=0;std::filesystem::file_time_type modified;ReplayInfo info;};
    std::filesystem::path root,source;std::map<std::filesystem::path,Cached> cache;
    std::shared_ptr<const ReplayArchiveState> state=std::make_shared<ReplayArchiveState>();
    std::shared_ptr<const MapData> loaded;
    std::shared_ptr<const ReplayMotion> motion;
    std::shared_ptr<const ReplayAnnotations> annotations;int64_t annotationRun=0;uint64_t annotationRevision=0;std::string annotationError;
    int64_t motionRun=0,exportedRun=0,stagedRun=0;std::string motionError,exportPath;
    std::deque<Job> jobs;std::mutex gate;std::condition_variable wake;std::thread worker;
    bool stopping=false,scanQueued=false;std::string problem;
    static uint64_t Hash(const std::vector<uint8_t>& bytes,size_t n){uint64_t h=14695981039346656037ull;for(size_t i=0;i<n;i++){h^=bytes[i];h*=1099511628211ull;}return h;}
    static std::vector<uint8_t> Read(const std::filesystem::path& p,size_t limit){
        std::error_code ec;auto n=std::filesystem::file_size(p,ec);if(ec||n>limit)throw std::runtime_error("Replay file missing or too large");
        std::ifstream in(p,std::ios::binary);std::vector<uint8_t> b(static_cast<size_t>(n));if(n)in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(n));if(!in)throw std::runtime_error("Replay file unreadable");return b;
    }
    static void Atomic(const std::filesystem::path& p,const std::vector<uint8_t>& bytes,bool replace,bool backupExisting=true){
        std::filesystem::create_directories(p.parent_path());auto temp=p;temp+=".tmp";
        if(!replace&&std::filesystem::exists(p)){if(Read(p,std::max(MapCapacity,bytes.size()))!=bytes)throw std::runtime_error("Existing replay preserved; identity conflict");return;}
        {std::ofstream out(temp,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));out.close();if(!out)throw std::runtime_error("Replay could not be saved");}
        if(replace&&backupExisting&&std::filesystem::exists(p)){auto backup=p;backup+=".bak";std::filesystem::copy_file(p,backup,std::filesystem::copy_options::overwrite_existing);}
#ifdef _WIN32
        if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Replay save could not be committed");
#else
        std::filesystem::rename(temp,p);
#endif
    }
    static bool ValidText(const std::string& text,size_t limit){
        if(text.size()>limit||text.find('\0')!=std::string::npos)return false;
        // Strict UTF-8, including overlong encodings, surrogates and > U+10FFFF.
        for(size_t i=0;i<text.size();){uint32_t c=uint8_t(text[i++]);if(c<128)continue;int n=c>=0xc2&&c<=0xdf?1:c>=0xe0&&c<=0xef?2:c>=0xf0&&c<=0xf4?3:-1;if(n<0||i+size_t(n)>text.size())return false;uint32_t value=c&((1u<<(6-n))-1);for(int k=0;k<n;k++){c=uint8_t(text[i++]);if((c&0xc0)!=0x80)return false;value=(value<<6)|(c&63);}if(value<(n==1?128u:n==2?2048u:65536u)||value>0x10ffff||(value>=0xd800&&value<=0xdfff))return false;}return true;
    }
    static std::vector<uint8_t> Meta(const ReplayInfo& row){
        auto single=[](const std::string& s){return std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32||c==127;});};
        if(row.name.empty()||!single(row.name)||!single(row.category)||!ValidText(row.name,160)||!ValidText(row.category,128)||!ValidText(row.notes,8192))throw std::runtime_error("Replay text is invalid or too long");
        std::vector<uint8_t> b;auto put=[&](auto v){auto* p=reinterpret_cast<const uint8_t*>(&v);b.insert(b.end(),p,p+sizeof(v));};put(uint32_t{0x524d4554});put(int32_t{1});put(row.id);
        for(const auto& s:{row.name,row.category,row.notes}){put(int32_t(s.size()));b.insert(b.end(),s.begin(),s.end());}put(Hash(b,b.size()));return b;
    }
    static void ReadMeta(const std::filesystem::path& p,ReplayInfo& row){
        auto b=Read(p,16384);MapReader r(b);if(r.Read<uint32_t>()!=0x524d4554||r.Read<int32_t>()!=1||r.Read<int64_t>()!=row.id)throw std::runtime_error("Replay notes identity mismatch");
        auto name=r.Text(160),category=r.Text(128),notes=r.Text(8192);if(!ValidText(name,160)||!ValidText(category,128)||!ValidText(notes,8192)||b.size()<8||r.Read<uint64_t>()!=Hash(b,b.size()-8)||!r.Done())throw std::runtime_error("Replay notes are damaged");row.name=name;row.category=category;row.notes=notes;
    }
    std::filesystem::path File(int64_t id,const char* extension)const{return root/("run-"+std::to_string(id)+extension);}
    uint64_t savedRevision=0;int64_t savedRun=0;
    void Publish(bool busy=false){
        auto snapshot=std::make_shared<ReplayArchiveState>();snapshot->annotationRun=annotationRun;snapshot->annotationRevision=annotationRevision;snapshot->annotationError=annotationError;snapshot->error=problem;snapshot->busy=busy;snapshot->savedRevision=savedRevision;snapshot->savedRun=savedRun;snapshot->motionError=motionError;snapshot->motionRun=motionRun;snapshot->exportedRun=exportedRun;snapshot->exportPath=exportPath;snapshot->stagedRun=stagedRun;std::map<int64_t,ReplayInfo> rows;
        for(const auto& [path,item]:cache){(void)path;auto old=rows.find(item.info.id);if(old==rows.end()||item.info.saved)rows[item.info.id]=item.info;}
        for(auto it=rows.rbegin();it!=rows.rend();++it){auto row=it->second;
            auto metadata=File(row.id,".meta");std::error_code metaError;if(std::filesystem::exists(metadata,metaError))try{ReadMeta(metadata,row);}catch(const std::exception& e){snapshot->error=e.what();auto backup=metadata;backup+=".bak";try{ReadMeta(backup,row);}catch(...) {}}
            snapshot->runs.push_back(std::move(row));
        }
        std::atomic_store(&state,std::shared_ptr<const ReplayArchiveState>(snapshot));
    }
    void Scan(){
        Publish(true);
        std::map<std::filesystem::path,Cached> next;
        for(const auto& directory:{source,root}){
            if(directory.empty()||!std::filesystem::exists(directory))continue;
            for(const auto& entry:std::filesystem::directory_iterator(directory)){
                if(!entry.is_regular_file()||entry.path().extension()!=".isrun")continue;
                try{auto modified=entry.last_write_time();auto size=entry.file_size();auto old=cache.find(entry.path());
                    if(old!=cache.end()&&old->second.size==size&&old->second.modified==modified){next.emplace(entry.path(),old->second);continue;}
                    std::string error;auto data=DecodeMap(Read(entry.path(),MapCapacity-64),error);if(!data||!data->review)throw std::runtime_error(error.empty()?"File contains no replay":error);
                    const auto& r=*data->review;auto expected="run-"+std::to_string(r.id)+".isrun";if(entry.path().filename()!=expected)throw std::runtime_error("Replay filename and identity differ");
                    ReplayInfo info;info.id=r.id;info.scene=data->scene;info.seed=data->seed;info.outcome=r.outcome;info.seconds=r.seconds;info.valid=r.valid;info.reason=r.validityReason;info.saved=directory==root;info.file=entry.path();info.name=ReplayName(*data);
                    next.emplace(entry.path(),Cached{size,modified,std::move(info)});
                }catch(const std::exception& e){problem=entry.path().filename().string()+": "+std::string(e.what());}
            }
        }
        cache=std::move(next);Publish();
    }
    static bool SameRecording(const MapData& a,const MapData& b){
        if(!a.review||!b.review||a.scene!=b.scene||a.seed!=b.seed||a.hash!=b.hash||a.spawn!=b.spawn||a.keep!=b.keep||a.selected!=b.selected||a.capturedUtc!=b.capturedUtc||a.spawns.size()!=b.spawns.size()||a.points.size()!=b.points.size())return false;
        auto vectors=[](const auto& x,const auto& y){return x.size()==y.size()&&(x.empty()||std::memcmp(x.data(),y.data(),x.size()*sizeof(Vec3))==0);};
        if(!vectors(a.spawns,b.spawns)||!vectors(a.points,b.points))return false;
        const auto& x=*a.review;const auto& y=*b.review;
        if(x.id!=y.id||x.outcome!=y.outcome||x.seconds!=y.seconds||x.valid!=y.valid||x.validationChecked!=y.validationChecked||x.observedFromStart!=y.observedFromStart||x.seedKnown!=y.seedKnown||x.seedKind!=y.seedKind||x.aids!=y.aids||x.warnings!=y.warnings||x.validityReason!=y.validityReason||x.reduced!=y.reduced||x.monstersReduced!=y.monstersReduced||x.lightSources!=y.lightSources||x.lightRejected!=y.lightRejected||x.collected!=y.collected||x.pickupSeconds!=y.pickupSeconds||x.trace.size()!=y.trace.size()||x.monsters.size()!=y.monsters.size()||x.flashes.size()!=y.flashes.size()||bool(x.motion)!=bool(y.motion))return false;
        for(size_t i=0;i<x.trace.size();i++)if(x.trace[i].seconds!=y.trace[i].seconds||Distance(x.trace[i].point,y.trace[i].point)!=0)return false;
        for(size_t i=0;i<x.monsters.size();i++){const auto& m=x.monsters[i];const auto& n=y.monsters[i];if(m.id!=n.id||m.kind!=n.kind||m.trace.size()!=n.trace.size()||m.paths.size()!=n.paths.size())return false;for(size_t j=0;j<m.trace.size();j++)if(m.trace[j].seconds!=n.trace[j].seconds||m.trace[j].continuous!=n.trace[j].continuous||Distance(m.trace[j].point,n.trace[j].point)!=0)return false;for(size_t j=0;j<m.paths.size();j++)if(m.paths[j].seconds!=n.paths[j].seconds||!vectors(m.paths[j].corners,n.paths[j].corners))return false;}
        for(size_t i=0;i<x.flashes.size();i++){const auto& m=x.flashes[i];const auto& n=y.flashes[i];if(m.seconds!=n.seconds||m.kind!=n.kind||m.actor!=n.actor||Distance(m.player,n.player)!=0)return false;}
        if(x.motion){const auto& m=*x.motion;const auto& n=*y.motion;if(m.frames!=n.frames||m.ticks!=n.ticks||m.events!=n.events||m.flags!=n.flags||m.dropped!=n.dropped||m.tool!=n.tool||m.game!=n.game||m.unity!=n.unity)return false;}
        return true;
    }
    std::shared_ptr<const MapData> Canonical(const MapData& d)const{
        auto name="run-"+std::to_string(d.review->id)+".isrun";
        for(const auto& folder:{root,source})if(!folder.empty()&&std::filesystem::exists(folder/name)){std::string error;auto original=DecodeMap(Read(folder/name,MapCapacity-64),error);if(original&&SameRecording(d,*original))return original;}
        return {};
    }
    std::filesystem::path Companion(const MapData& d,const char* extension)const{
        std::string name="run-"+std::to_string(d.review->id)+extension;
        for(const auto& folder:{root,source})if(!folder.empty()){auto map=folder/("run-"+std::to_string(d.review->id)+".isrun");auto p=folder/name;if(!std::filesystem::exists(map)||!std::filesystem::exists(p))continue;std::string error;auto original=DecodeMap(Read(map,MapCapacity-64),error);if(original&&SameRecording(d,*original))return p;}
        return {};
    }
    std::shared_ptr<const ReplayMotion> ReadMotion(const MapData& d){
        if(!d.review||!d.review->motion)return {};
        auto current=std::atomic_load(&motion);if(current&&current->Matches(d)&&(!current->experience.audio.recorded||!current->audioFile.empty()))return current;
        auto file=Companion(d,".isinput");if(file.empty())throw std::runtime_error("Input recording is missing or still being saved");std::string error;auto value=DecodeMotion(Read(file,MotionCapacity),error);if(!value||!value->Matches(d))throw std::runtime_error(value?"Input recording belongs to a different run":error);
        // Bind optional audio only on the archive worker, before publishing.
        if(value->experience.audio.recorded){auto writable=std::const_pointer_cast<ReplayMotion>(value);try{auto audio=Companion(d,".isaudio");if(audio.empty())throw std::runtime_error("Recorded audio is missing");VerifyRecordedAudio(audio,value->id,value->scene,value->seed,value->hash,&value->experience.audio);writable->audioFile=audio;}catch(const std::exception& e){writable->audioError=e.what();}}
        RecordingCheckFor(*value);return value;
    }
    static void CopyAudio(const ReplayMotion& input,const std::filesystem::path& destination){
        if(!input.experience.audio.recorded)return;
        std::filesystem::create_directories(destination.parent_path());
        if(input.audioFile.empty())throw std::runtime_error("Recorded audio companion is missing or damaged");
        if(std::filesystem::exists(destination)){VerifyRecordedAudio(destination,input.id,input.scene,input.seed,input.hash,&input.experience.audio);return;}
        auto temporary=destination;temporary+=".tmp-audio";bool own=false;
        try{std::filesystem::copy_file(input.audioFile,temporary);own=true;VerifyRecordedAudio(temporary,input.id,input.scene,input.seed,input.hash,&input.experience.audio);
#ifdef _WIN32
            if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit replay audio; existing file preserved");
#else
            std::filesystem::create_hard_link(temporary,destination);std::filesystem::remove(temporary);
#endif
        }catch(...){if(own){std::error_code ec;std::filesystem::remove(temporary,ec);}throw;}
    }
    ReplayFiles Package(const MapData& d){
        if(!d.review||d.bytes.empty())throw std::runtime_error("No recorded run to export");
        auto prefix="run-"+std::to_string(d.review->id);ReplayFiles files;auto originalMap=Canonical(d);files[prefix+".isrun"]=originalMap?originalMap->bytes:d.bytes;auto input=ReadMotion(d);if(input)files[prefix+".isinput"]=input->bytes;if(input&&input->experience.audio.recorded){if(input->audioFile.empty())throw std::runtime_error("Recorded audio companion is missing or damaged");files[prefix+".isaudio"]=Read(input->audioFile,AudioCapacity);}
        auto original=Companion(d,".validation.json");bool report=!original.empty();if(report)files[prefix+".validation.json"]=Read(original,65536);
        auto note=File(d.review->id,".meta");if(std::filesystem::exists(note)){ReplayInfo row;row.id=d.review->id;ReadMeta(note,row);files[prefix+".meta"]=Read(note,16384);}
        auto marks=File(d.review->id,".ismarks");bool hasMarks=std::filesystem::exists(marks);if(hasMarks){auto bytes=Read(marks,65536);auto a=DecodeAnnotations(bytes);if(!a->Matches(d))throw std::runtime_error("Annotation identity mismatch");files[prefix+".ismarks"]=std::move(bytes);}
        files["manifest.json"]=ReplayText(ReplayManifest(originalMap?*originalMap:d,bool(input),report,suite_brand::Version,hasMarks));
        files["README.txt"]=ReplayText("PracticeSuite run package / mab\nImport this ZIP in Replay > Import package. Keep the package intact.\nInput/camera data is available only for recordings made with 7.2.0 or later.\nSHA256SUMS.txt verifies consistency, not origin or physical keyboard input.\nABORTED is an outcome; local validity is a separate observation.\nOptional .isaudio contains the recorded game mix and is bound to the input recording. New input formats include observed animation states and real game probes; missing historical channels are labelled.\n");
        std::string sums;for(const auto& [name,bytes]:files)sums+=ReplaySha256(bytes)+"  "+name+"\n";files["SHA256SUMS.txt"]=ReplayText(sums);return files;
    }
    void Import(const std::filesystem::path& path){
        auto files=DecodeReplayZip(Read(path,ReplayPackageCapacity));auto sums=files.find("SHA256SUMS.txt");if(sums==files.end()||sums->second.size()>4096)throw std::runtime_error("Package has no checksum manifest");
        std::istringstream lines(std::string(sums->second.begin(),sums->second.end()));std::string line;std::map<std::string,bool> verified;
        while(std::getline(lines,line)){if(line.size()<67||line.substr(64,2)!="  ")throw std::runtime_error("Invalid package checksum list");auto name=line.substr(66);auto f=files.find(name);if(f==files.end()||name=="SHA256SUMS.txt"||verified[name]||ReplaySha256(f->second)!=line.substr(0,64))throw std::runtime_error("Package SHA256 mismatch");verified[name]=true;}
        if(verified.size()+1!=files.size())throw std::runtime_error("Unverified package entry");
        std::shared_ptr<const MapData> d;std::string error;
        for(const auto& [name,bytes]:files)if(std::filesystem::path(name).extension()==".isrun"){if(d)throw std::runtime_error("Package contains multiple runs");d=DecodeMap(bytes,error);if(!d||!d->review||name!="run-"+std::to_string(d->review->id)+".isrun")throw std::runtime_error("Invalid package run identity: "+error);}
        if(!d)throw std::runtime_error("Package contains no run");
        auto prefix="run-"+std::to_string(d->review->id);bool hasInput=files.count(prefix+".isinput")!=0,hasReport=files.count(prefix+".validation.json")!=0;
        if(bool(d->review->motion)!=hasInput)throw std::runtime_error("Package input recording is missing");
        if(!hasInput&&files.count(prefix+".isaudio"))throw std::runtime_error("Orphaned audio companion");
        if(hasInput){auto input=DecodeMotion(std::move(files[prefix+".isinput"]),error);if(!input||!input->Matches(*d))throw std::runtime_error("Package input identity mismatch: "+error);bool hasAudio=files.count(prefix+".isaudio")!=0;if(hasAudio!=input->experience.audio.recorded)throw std::runtime_error("Package audio companion does not match recording");if(hasAudio)VerifyRecordedAudio(files[prefix+".isaudio"],input->id,input->scene,input->seed,input->hash,&input->experience.audio);files[prefix+".isinput"]=input->bytes;}
        bool hasMarks=files.count(prefix+".ismarks")!=0;
        if(hasMarks){auto a=DecodeAnnotations(files[prefix+".ismarks"]);if(!a->Matches(*d))throw std::runtime_error("Package annotation identity mismatch");}
        bool manifest=false;for(const std::string& version:{std::string(suite_brand::Version),std::string("7.11.3"),std::string("7.11.2"),std::string("7.11.1"),std::string("7.11.0"),std::string("7.10.1"),std::string("7.10.0"),std::string("7.9.0"),std::string("7.8.1"),std::string("7.8.0"),std::string("7.7.0"),std::string("7.6.0"),std::string("7.5.2"),std::string("7.5.1"),std::string("7.5.0"),std::string("7.4.0"),std::string("7.3.5"),std::string("7.3.4"),std::string("7.3.3"),std::string("7.3.2"),std::string("7.3.1"),std::string("7.3.0"),std::string("7.2.1"),std::string("7.2.0")})if((!hasMarks||version==suite_brand::Version||version=="7.11.3"||version=="7.11.2"||version=="7.11.1"||version=="7.11.0"||version=="7.10.1"||version=="7.10.0"||version=="7.9.0"||version=="7.8.1"||version=="7.8.0"||version=="7.7.0"||version=="7.6.0"||version=="7.5.2"||version=="7.5.1"||version=="7.5.0"||version=="7.4.0"||version=="7.3.5"||version=="7.3.4"||version=="7.3.3"||version=="7.3.2"||version=="7.3.1"||version=="7.3.0")&&files.count("manifest.json")&&files["manifest.json"]==ReplayText(ReplayManifest(*d,hasInput,hasReport,version,hasMarks)))manifest=true;
        if(!manifest)throw std::runtime_error("Package manifest identity mismatch");
        for(const auto& [name,bytes]:files){(void)bytes;if(name!=prefix+".isrun"&&name!=prefix+".isinput"&&name!=prefix+".isaudio"&&name!=prefix+".validation.json"&&name!=prefix+".meta"&&name!=prefix+".ismarks"&&name!="manifest.json"&&name!="SHA256SUMS.txt"&&name!="README.txt")throw std::runtime_error("Unexpected package file");}
        // Preflight identity conflicts before committing any file.
        for(const auto& [name,bytes]:files)if(name.rfind(prefix+".",0)==0){auto dest=root/name;if(std::filesystem::exists(dest)&&Read(dest,name.ends_with(".isaudio")?AudioCapacity:MotionCapacity)!=bytes)throw std::runtime_error("Existing recording preserved; package identity conflict");}
        if(files.count(prefix+".meta")){ReplayInfo row;row.id=d->review->id;auto temp=root/"import-notes.tmp";Atomic(temp,files[prefix+".meta"],true,false);try{ReadMeta(temp,row);}catch(...){std::filesystem::remove(temp);throw;}std::filesystem::remove(temp);}
        for(const auto& [name,bytes]:files)if(name.rfind(prefix+".",0)==0&&name!=prefix+".isrun")Atomic(root/name,bytes,false);
        Atomic(File(d->review->id,".isrun"),d->bytes,false);savedRun=d->review->id;savedRevision++;Scan();
    }
    void Work(){
#ifdef _WIN32
        SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
#endif
        while(true){Job job;
            {std::unique_lock lock(gate);wake.wait(lock,[&]{return stopping||!jobs.empty();});if(jobs.empty()&&stopping)return;job=std::move(jobs.front());jobs.pop_front();if(job.kind==1)scanQueued=false;}
            try{
                problem.clear();
                if(job.kind==0){root=job.path;std::filesystem::create_directories(root);auto config=root/"source.txt";if(std::filesystem::exists(config)){auto b=Read(config,32768);source=std::filesystem::path(std::u8string(b.begin(),b.end()));}Scan();}
                else if(job.kind==1)Scan();
                else if(job.kind==2){source=job.path;stagedRun=0;auto text=source.u8string();Atomic(root/"source.txt",std::vector<uint8_t>(text.begin(),text.end()),true);Scan();}
                else if(job.kind==3){auto snapshot=Get();auto row=std::find_if(snapshot->runs.begin(),snapshot->runs.end(),[&](const auto& r){return r.id==job.id;});if(row==snapshot->runs.end())throw std::runtime_error("Replay no longer available");std::string error;auto data=DecodeMap(Read(row->file,MapCapacity-64),error);if(!data||!data->review||data->review->id!=job.id)throw std::runtime_error(error);std::atomic_store(&loaded,data);}
                else if(job.kind==4){if(!job.data||!job.data->review||job.data->bytes.empty())throw std::runtime_error("Replay contains no saved recording");ReplayInfo row;row.id=job.data->review->id;row.name=job.name;row.category=job.category;row.notes=job.notes;auto metadata=Meta(row);if(root.empty())throw std::runtime_error("Replay directory unavailable");auto original=Canonical(*job.data);const auto& bytes=original?original->bytes:job.data->bytes;if(std::filesystem::exists(File(row.id,".isrun"))&&Read(File(row.id,".isrun"),MapCapacity-64)!=bytes)throw std::runtime_error("Existing replay preserved; identity conflict");auto input=ReadMotion(*job.data);if(input){CopyAudio(*input,File(row.id,".isaudio"));Atomic(File(row.id,".isinput"),input->bytes,false);}auto report=Companion(*job.data,".validation.json");if(!report.empty())Atomic(File(row.id,".validation.json"),Read(report,65536),false);Atomic(File(row.id,".isrun"),bytes,false);
                    auto target=File(row.id,".meta");bool preserve=false;if(std::filesystem::exists(target))try{auto previous=row;ReadMeta(target,previous);}catch(...){preserve=true;auto damaged=target;auto damagedBytes=Read(target,16384);damaged+=".corrupt-"+std::to_string(Hash(damagedBytes,damagedBytes.size()));std::filesystem::copy_file(target,damaged,std::filesystem::copy_options::skip_existing);}
                    Atomic(target,metadata,true,!preserve);savedRun=row.id;savedRevision++;Scan();}
                else if(job.kind==5){auto files=Package(*job.data);auto p=root/"Exports"/("run-"+std::to_string(job.data->review->id)+".zip");auto bytes=EncodeReplayZip(files);if(std::filesystem::exists(p)&&Read(p,ReplayPackageCapacity)!=bytes){auto stamp=std::chrono::system_clock::now().time_since_epoch().count();p=root/"Exports"/("run-"+std::to_string(job.data->review->id)+"-"+std::to_string(stamp)+".zip");}Atomic(p,bytes,false);exportedRun=job.data->review->id;exportPath=p.string();Publish();}
                else if(job.kind==6){motionRun=job.data->review->id;motionError.clear();std::atomic_store(&motion,std::shared_ptr<const ReplayMotion>{});Publish(true);auto value=ReadMotion(*job.data);std::atomic_store(&motion,value);Publish();}
                else if(job.kind==7)Import(job.path);
                else if(job.kind==8){stagedRun=0;Publish(true);if(source.empty())throw std::runtime_error("Connect to the game before opening 3D replay");auto input=ReadMotion(*job.data);if(!input||input->frames.empty())throw std::runtime_error("This replay has no camera recording");auto folder=source/"ReplayInbox";auto prefix="run-"+std::to_string(input->id);auto original=Canonical(*job.data);if(!input->audioFile.empty())CopyAudio(*input,folder/(prefix+".isaudio"));Atomic(folder/(prefix+".isinput"),input->bytes,false);Atomic(folder/(prefix+".isrun"),original?original->bytes:job.data->bytes,false);stagedRun=input->id;Publish();}
                else if(job.kind==9){
                    annotationRun=job.data->review->id;annotationError.clear();std::atomic_store(&annotations,std::shared_ptr<const ReplayAnnotations>{});Publish(true);
                    auto p=File(annotationRun,".ismarks");std::shared_ptr<const ReplayAnnotations> a;
                    if(std::filesystem::exists(p)){try{a=DecodeAnnotations(Read(p,65536));}catch(const std::exception& e){annotationError=e.what();auto backup=p;backup+=".bak";a=DecodeAnnotations(Read(backup,65536));}}
                    else{auto fresh=std::make_shared<ReplayAnnotations>();fresh->run=annotationRun;fresh->duration=job.data->review->seconds;a=fresh;}
                    if(!a->Matches(*job.data))throw std::runtime_error("Annotation identity mismatch");
                    std::atomic_store(&annotations,a);annotationRevision++;Publish();
                }
                else if(job.kind==10){
                    if(!job.annotations||!job.annotations->Matches(*job.data))throw std::runtime_error("Annotation identity mismatch");
                    auto original=Canonical(*job.data);if(!original)throw std::runtime_error("Retain the recording before saving markers");auto bytes=EncodeAnnotations(*job.annotations);auto p=File(job.annotations->run,".ismarks");bool backup=true;
                    if(std::filesystem::exists(p))try{DecodeAnnotations(Read(p,65536));}catch(...){backup=false;auto damaged=p;damaged+=".corrupt";std::filesystem::copy_file(p,damaged,std::filesystem::copy_options::overwrite_existing);}
                    Atomic(p,bytes,true,backup);annotationRun=job.annotations->run;annotationError.clear();std::atomic_store(&annotations,job.annotations);annotationRevision++;Publish();
                }
            }catch(const std::exception& e){if(job.kind==9||job.kind==10)annotationError=e.what();else if(job.kind==6)motionError=e.what();else problem=e.what();Publish();}
        }
    }
    void Queue(Job job){std::lock_guard lock(gate);if(stopping)return;if(job.kind==1&&scanQueued)return;if(job.kind==1)scanQueued=true;jobs.push_back(std::move(job));if(!worker.joinable())worker=std::thread([this]{Work();});wake.notify_one();}
public:
    ReplayArchive()=default;ReplayArchive(const ReplayArchive&)=delete;
    ~ReplayArchive(){Shutdown();}
    void Shutdown(){{std::lock_guard lock(gate);stopping=true;wake.notify_one();}if(worker.joinable())worker.join();}
    void SetDirectory(std::filesystem::path path){Job j;j.path=std::move(path);Queue(std::move(j));}
    void SetSourceDirectory(std::filesystem::path path){Job j;j.kind=2;j.path=std::move(path);Queue(std::move(j));}
    void Refresh(){Job j;j.kind=1;Queue(std::move(j));}
    void Load(int64_t id){Job j;j.kind=3;j.id=id;Queue(std::move(j));}
    void LoadMotion(std::shared_ptr<const MapData> data){Job j;j.kind=6;j.data=std::move(data);Queue(std::move(j));}
    void Export(std::shared_ptr<const MapData> data){Job j;j.kind=5;j.data=std::move(data);Queue(std::move(j));}
    void ImportPackage(std::filesystem::path path){Job j;j.kind=7;j.path=std::move(path);Queue(std::move(j));}
    void Stage3D(std::shared_ptr<const MapData> data){Job j;j.kind=8;j.data=std::move(data);Queue(std::move(j));}
    void Save(std::shared_ptr<const MapData> data,std::string name,std::string category,std::string notes){Job j;j.kind=4;j.data=std::move(data);j.name=std::move(name);j.category=std::move(category);j.notes=std::move(notes);Queue(std::move(j));}
    void LoadAnnotations(std::shared_ptr<const MapData> data){Job j;j.kind=9;j.data=std::move(data);Queue(std::move(j));}
    void SaveAnnotations(std::shared_ptr<const MapData> data,std::shared_ptr<const ReplayAnnotations> value){Job j;j.kind=10;j.data=std::move(data);j.annotations=std::move(value);Queue(std::move(j));}
    std::shared_ptr<const ReplayAnnotations> Annotations()const{return std::atomic_load(&annotations);}
    std::shared_ptr<const ReplayArchiveState> Get()const{return std::atomic_load(&state);}
    std::shared_ptr<const MapData> Loaded()const{return std::atomic_load(&loaded);}
    std::shared_ptr<const ReplayMotion> Motion()const{return std::atomic_load(&motion);}
};
inline ReplayArchive g_replayArchive;
}
