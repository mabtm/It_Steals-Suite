#pragma once
#include "PlannerRandom.h"
#include <array>

namespace practice {
// An OS-random key chooses a pseudorandom permutation of all 32-bit seeds.
// Feistel rounds are bijective for every key: no visited-seed collection, no
// collisions, and no slowdown as the full seed space approaches exhaustion.
// This is a sampling order, not a replacement for the game's Unity RNG.
class RandomSeedOrder {
    std::array<uint32_t,8> keys{};
    static uint32_t Mix(uint32_t value) {
        value=(value^(value>>16))*0x7feb352du;
        value=(value^(value>>15))*0x846ca68bu;
        return value^(value>>16);
    }
public:
    explicit RandomSeedOrder(uint64_t key) {
        uint32_t low=static_cast<uint32_t>(key),high=static_cast<uint32_t>(key>>32);
        for(unsigned i=0;i<keys.size();i++) {
            unsigned shift=7+i;
            keys[i]=Mix(low^((high<<shift)|(high>>(32-shift)))^(0x9e3779b9u*(i+1)));
        }
    }
    uint32_t BitsAt(uint32_t index) const {
        uint32_t left=index>>16,right=index&0xffffu;
        for(uint32_t key:keys){uint32_t next=left^(Mix(right^key)&0xffffu);left=right;right=next;}
        return (left<<16)|right;
    }
    int SeedAt(uint32_t index) const {return PlannerSeedFromBits(BitsAt(index));}
};
}
