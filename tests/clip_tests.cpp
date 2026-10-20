#include "ClipEncoding.h"
#include "HudEditorModel.h"
#include "ReplayAnnotations.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <limits>
using namespace suite_clip;
static void Wait(Encoder& e){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(30);while(e.State()==1&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(e.State()!=1);}
static std::vector<uint8_t> Image(int w,int h,int frame){std::vector<uint8_t> pixels(size_t(w)*h*4);for(int y=0;y<h;y++)for(int x=0;x<w;x++){auto i=(size_t(y)*w+x)*4;pixels[i]=uint8_t(x*3+y*7+frame*13);pixels[i+1]=uint8_t(x+y*2+frame*29);pixels[i+2]=uint8_t(x*5+y+frame*19);pixels[i+3]=255;}return pixels;}
int main(int argc,char** argv){
    assert(argc>1);std::filesystem::path folder=argv[1];std::filesystem::create_directories(folder);
    Options o;o.a=2.125;o.b=5.625;o.fps=60;o.Validate(10);assert(o.Frames()==210);assert(o.At(0)==o.a&&o.At(209)<o.b&&o.Stamp(210)==35000000);o.rate=.5;assert(o.Frames()==420);o.rate=1;
    for(int k=0;k<6;k++){auto bad=o;if(k==0)bad.a=std::numeric_limits<double>::quiet_NaN();if(k==1)bad.b=bad.a;if(k==2)bad.b=11;if(k==3)bad.width=321;if(k==4){bad.format=1;bad.fps=60;}if(k==5)bad.rate=0;bool rejected=false;try{bad.Validate(10);}catch(...){rejected=true;}assert(rejected);}
    for(float x:{-1000.f,-.1f,0.f,100.f,990.f,9999.f}){suite_hud::Rect r{100,80,300,200};auto moved=suite_hud::Move(r,x,x,1280,720,12,true,8);assert(moved.x>=12&&moved.y>=12&&moved.x+moved.w<=1268&&moved.y+moved.h<=708);assert(suite_hud::Fraction(moved.x,1280,300,12)>=0&&suite_hud::Fraction(moved.x,1280,300,12)<=1);}
    assert(suite_hud::Snap(14,300,1280,12,8)==12);assert(suite_hud::Snap(491,300,1280,12,8)==490);
    practice::ReplayAnnotations a;a.run=17;a.duration=10;a.hasA=a.hasB=true;a.a=1;a.b=8;a.loop=true;a.hasClipA=a.hasClipB=true;a.clipA=2;a.clipB=6;a.markers={{1,2,"clip start"},{2,6,"clip end"}};auto encoded=practice::EncodeAnnotations(a);auto read=practice::DecodeAnnotations(encoded);assert(read->clipA==2&&read->clipB==6&&read->a==1&&read->b==8&&read->loop);
    auto v1=encoded;v1.erase(v1.end()-28,v1.end()-8);int32_t v1Format=1;std::memcpy(v1.data()+4,&v1Format,4);auto v1Hash=practice::ReplayHash(v1,v1.size()-8);std::memcpy(v1.data()+v1.size()-8,&v1Hash,8);auto legacyMarks=practice::DecodeAnnotations(v1);assert(!legacyMarks->hasClipA&&!legacyMarks->hasClipB&&legacyMarks->a==1&&legacyMarks->b==8&&legacyMarks->markers.size()==2);
    if(argc>2){std::ifstream f(argv[2],std::ios::binary);std::vector<uint8_t> old{std::istreambuf_iterator<char>(f),{}};auto legacy=practice::DecodeAnnotations(old);assert(!legacy->hasClipA&&!legacy->hasClipB);}
    auto black=std::vector<uint8_t>(320*180*4);auto nv12=NV12(black,320,180);assert(nv12.size()==320*180*3/2);for(size_t i=0;i<320*180;i++)assert(nv12[i]==16);for(size_t i=320*180;i<nv12.size();i++)assert(nv12[i]==128);
    auto file=folder/"palette-animation.gif";{GifWriter writer(file,320,180,20);for(int k=0;k<5;k++){auto bytes=Image(320,180,k);Quantize(bytes.data(),320*180);std::ofstream raw(folder/("expected-"+std::to_string(k)+".bgra"),std::ios::binary);raw.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size()));writer.Write(bytes);}writer.Finish();}
    Encoder encoder;Options job;job.format=1;job.width=320;job.height=180;job.fps=20;job.a=0;job.b=.15;job.Validate(1);auto streamed=folder/"streamed.gif";assert(encoder.Start(job,streamed));for(uint64_t i=0;i<job.Frames();i++){while(!encoder.Ready()&&encoder.State()==1)std::this_thread::sleep_for(std::chrono::milliseconds(1));assert(encoder.Push(Image(320,180,int(i))));}encoder.Finish();Wait(encoder);assert(encoder.State()==2&&encoder.Written()==3&&std::filesystem::exists(streamed));
    auto cancelled=folder/"cancelled.gif";assert(encoder.Start(job,cancelled));encoder.Cancel();Wait(encoder);assert(encoder.State()==4&&!std::filesystem::exists(cancelled)&&!std::filesystem::exists(cancelled.string()+".partial.gif"));
    auto occupied=folder/"existing.gif";{std::ofstream p(occupied);p<<"preserve";}assert(encoder.Start(job,occupied));Wait(encoder);assert(encoder.State()==3);{std::ifstream f(occupied);std::string s;f>>s;assert(s=="preserve");}
    auto partial=folder/"occupied.gif.partial.gif";{std::ofstream p(partial);p<<"also preserve";}assert(encoder.Start(job,folder/"occupied.gif"));Wait(encoder);assert(encoder.State()==3&&std::filesystem::exists(partial));
    std::cout<<"Clip timing, palette/LZW, bounded streaming, cancellation, preserved files, independent clip markers and HUD placement: PASS\n";
}
