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

// Lista de encontros de uma area, vinda do arquivo de dados.
struct FixedPathRoomEncounters
{
    const u16 *species; // SPECIES_*, ou NULL se vazio
    u16 count;
};

// Um mon do time fixo de uma sala de chefe, vindo do arquivo de dados.
struct FixedPathPartyMon
{
    u16 species; // SPECIES_*
    u8 level;
};

// Time fixo de uma sala de chefe, vindo do arquivo de dados.
struct FixedPathRoomParty
{
    const struct FixedPathPartyMon *mons; // NULL se vazio
    u16 count;
};

void RogueFixed_BuildPath(const struct FixedPathStep *steps, u16 count, struct RogueAdvPath *out);

// Preenche os slots de encontro a partir da lista da area.
// Grama recebe as primeiras grassCap especies; agua, as seguintes.
// Slots nao usados ficam SPECIES_NONE. enc NULL ou vazio deixa tudo NONE.
void RogueFixed_FillEncounterSlots(const struct FixedPathRoomEncounters *enc,
                                   u16 *grass, u8 grassCap,
                                   u16 *water, u8 waterCap);

// Cria min(count, capacity) mons do time com CreateMon, devolvendo quantos criou.
// party NULL ou vazio cria 0 mons.
u16 RogueFixed_FillParty(const struct FixedPathRoomParty *party, struct Pokemon *out, u8 capacity);

// Maior nivel do time; 0 se party NULL ou vazio.
u8 RogueFixed_PartyMaxLevel(const struct FixedPathRoomParty *party);

#endif // GUARD_ROGUE_FIXEDPATH_H
