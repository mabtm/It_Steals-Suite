#pragma once
#include "SharedProtocol.h"
#include <string>
inline const char* PlayerStateLabel(const GameData& d,double age){if(!d.playerReady||age>=.5)return "--";if(d.health<=0)return "Dead";if(d.freecamActive)return "Free camera";if(d.canMove<=0)return "Locked";if(d.isCrouching)return "Crouching";if(d.isSprinting)return "Sprinting";return d.smoothedSpeed<.1f?"Idle":"Walking";}
inline std::string MonsterStateLabel(const GameData& d,double age){
    if(age>=.5)return "--";
    if(d.hideAndSeekMode){if(d.hideSeek_foundPlayer)return "Found player";if(d.hideSeek_running)return "Running";if(d.hideSeek_hiding)return "Hiding";if(d.hideSeek_seekingMode)return "Seeking";return "No active state";}
    if(d.ghostMode){if(d.ghostChasing)return "Chasing";if(d.ghostHunting)return "Hunting";if(d.ghostRunning)return "Running";return d.monsterActive?"Not chasing":"--";}
    if(d.timerMode){if(!d.timerMonsterCount)return "--";int best=0;float dist=1e30f;for(int i=0;i<d.timerMonsterCount;i++){float x=d.timerMonsterPositions[i*2]-d.playerX,z=d.timerMonsterPositions[i*2+1]-d.playerZ;if(x*x+z*z<dist){dist=x*x+z*z;best=i;}}return d.timerMonsterStates[best]==2?"Angry":d.timerMonsterStates[best]==1?"Spotted":"Not alerted";}
    if(!d.monsterActive)return "--";
    return d.monsterSpotted?"Spotted":d.monsterSneak?"Sneaking":"Not spotted";
}
