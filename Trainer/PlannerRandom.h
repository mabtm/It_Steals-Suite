#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <bcrypt.h>
#else
#include <random>
#endif

inline int PlannerSeedFromBits(uint32_t bits) {
    int32_t seed;std::memcpy(&seed,&bits,sizeof(seed));return seed;
}
inline bool GeneratePlannerSeed(int& seed,std::string& error) {
    uint32_t bits=0;
#ifdef _WIN32
    if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&bits),sizeof(bits),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) {
        error="Windows could not generate a random seed.";return false;
    }
#else
    try {std::random_device source;bits=std::uniform_int_distribution<uint32_t>{}(source);}
    catch(const std::exception& e){error=e.what();return false;}
#endif
    seed=PlannerSeedFromBits(bits);error.clear();return true;
}
inline bool GenerateSearchKey(uint64_t& key,std::string& error) {
#ifdef _WIN32
    if(BCryptGenRandom(nullptr,reinterpret_cast<PUCHAR>(&key),sizeof(key),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) {
        error="Windows could not initialize random seed search.";return false;
    }
#else
    try {std::random_device source;key=std::uniform_int_distribution<uint64_t>{}(source);}
    catch(const std::exception& e){error=e.what();return false;}
#endif
    error.clear();return true;
}
