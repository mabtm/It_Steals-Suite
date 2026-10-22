#pragma once
#include "SharedProtocol.h"
#include "MovementTiming.h"

inline void ObserveMovementSnapshot(const GameData &d)
{
    if (MapLevelIndex(d.movementScene) < 0)
        return;
    std::string hash(d.gameAssemblyHash, strnlen(d.gameAssemblyHash, sizeof(d.gameAssemblyHash)));
    if (hash.empty() || hash == "UNKNOWN")
        return;
    auto old = std::atomic_load(&practice::learnedMovement[MapLevelIndex(d.movementScene)]);
    if (old && old->revision == d.movementRevision && old->rejected == d.movementRejected &&
        old->hash == hash && old->tick == d.movementTick && old->radius == d.movementRadius &&
        old->height == d.movementHeight)
        return;
    auto m = std::make_shared<practice::MovementData>();
    m->scene = d.movementScene;
    m->revision = d.movementRevision;
    m->rejected = d.movementRejected;
    m->tick = d.movementTick;
    m->radius = d.movementRadius;
    m->height = d.movementHeight;
    m->hash = hash;
    std::copy(std::begin(d.movementCounts), std::end(d.movementCounts), m->counts.begin());
    std::copy(std::begin(d.movementSpeeds), std::end(d.movementSpeeds), m->speeds.begin());
    std::copy(std::begin(d.movementLengths), std::end(d.movementLengths), m->lengths.begin());
    if (!practice::ValidMovement(*m))
        return;
    practice::PublishMovement(m);
}
