#ifndef GUARD_ROGUE_FIXEDPATH_H
#define GUARD_ROGUE_FIXEDPATH_H

#include "global.h"
#include "rogue.h"

// Uma entrada do caminho fixo, vinda do arquivo de dados.
struct FixedPathStep
{
    u8 roomType; // ADVPATH_ROOM_*
    u8 param;    // dificuldade da rota, ou 0
    u16 extra;   // trainerNum do chefe, ou 0
};

void RogueFixed_BuildPath(const struct FixedPathStep *steps, u16 count, struct RogueAdvPath *out);

#endif // GUARD_ROGUE_FIXEDPATH_H
