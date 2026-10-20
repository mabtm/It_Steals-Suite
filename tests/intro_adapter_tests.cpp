#include "IntroBackdrop.h"
#include <iostream>
int main(){
    ID3D11Device device;FakeGame game;
    for(int fail=0;fail<=9;fail++){
        failure=fail;int previous=captures;
        {IntroBackdrop intro;intro.Capture(&device,&game,2000,2000);assert(!liveDC&&!liveBitmap);assert(bool(intro.View())==(fail==0));if(fail==0)assert(liveTextures==1&&liveViews==1);
            int once=captures;intro.Capture(&device,&game,520,212);assert(captures==once);intro.Reset();intro.Reset();assert(!liveTextures&&!liveViews);intro.Capture(&device,&game,520,212);assert(captures==once);
        }assert(!liveTextures&&!liveViews&&!liveDC&&!liveBitmap);assert(captures-previous<=1);
    }
    failure=0;IntroBackdrop hidden;int before=captures;game.iconic=true;hidden.Capture(&device,&game,520,212);hidden.Capture(&device,nullptr,520,212);hidden.Capture(nullptr,&game,520,212);assert(captures==before);game.iconic=false;hidden.Capture(&device,&game,520,212);assert(hidden.View()&&captures==before+1);hidden.Reset();assert(!liveTextures&&!liveViews);
    std::cout<<"Production IntroBackdrop: single bounded game-client crop, GDI flush, immutable opaque texture, restored GDI objects, no recapture, null/minimized suppression, every GDI/D3D failure, black-capture fallback and repeat cleanup: PASS\n";
}
