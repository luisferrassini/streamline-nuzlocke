#include "global.h"
#include "gba/isagbprint.h"
#include "rogue.h"
#include "rogue_controller.h"
#include "rogue_fixedpath.h"
#include "data/rogue/hack_path.h"

void RogueFixed_BuildPath(const struct FixedPathStep *steps, u16 count, struct RogueAdvPath *out)
{
    u16 i;
    u16 bossCount = 0;

    // O gerador (tools/hack/gen_path_header.py) já recusa um caminho maior que
    // isto. O assert torna o limite visível em vez de cortar em silêncio.
    AGB_ASSERT(count <= ROGUE_ADVPATH_ROOM_CAPACITY);
    if (count > ROGUE_ADVPATH_ROOM_CAPACITY)
        count = ROGUE_ADVPATH_ROOM_CAPACITY;

    out->roomCount = count;
    out->pathLength = count;
    out->pathMinY = 0;
    out->pathMaxY = 0;

    // Quantos chefes o caminho tem. O laço abaixo percorre as areas em ordem
    // inversa (a ultima area do dado cai na sala 0), então este contador
    // decresce e o valor usado é o ordinal do chefe na ordem de jogo.
    for (i = 0; i < count; ++i)
        if (steps[i].roomType == ADVPATH_ROOM_BOSS)
            ++bossCount;

    for (i = 0; i < count; ++i)
    {
        // O jogo joga de coords.x maior para menor: a última área do dado
        // (o chefe) cai em coords.x == 0, que é a sala final.
        const struct FixedPathStep *step = &steps[count - 1 - i];

        out->rooms[i].coords.x = i;
        out->rooms[i].coords.y = 0;
        out->rooms[i].roomType = step->roomType;
        out->rooms[i].connectionMask = (i + 1 < count) ? ROOM_CONNECTION_MASK_MID : 0;
        out->rooms[i].rngSeed = 0;
        out->rooms[i].roomParams.roomIdx = i;

        // Indice da area na ordem do dado (o passo que caiu nesta sala).
        out->rooms[i].areaIdx = count - 1 - i;

        // perType é uma união: escrever só o campo do tipo da sala.
        if (step->roomType == ADVPATH_ROOM_BOSS)
        {
            // Sem treinador fixo no dado (extra == 0), usa o treinador
            // escolhido para este chefe. O ordinal é o da ordem de jogo.
            u16 tn = step->extra;

            --bossCount;
            if (tn == 0 && bossCount < ROGUE_MAX_BOSS_COUNT)
                tn = gRogueRun.bossTrainerNums[bossCount];

            out->rooms[i].roomParams.perType.boss.trainerNum = tn;
        }
        else
        {
            out->rooms[i].roomParams.perType.route.difficulty = step->param;
        }
    }
}

void RogueFixed_FillEncounterSlots(const struct FixedPathRoomEncounters *enc,
                                   u16 *grass, u8 grassCap,
                                   u16 *water, u8 waterCap)
{
    u16 count = (enc != NULL) ? enc->count : 0;
    u16 i;

    // Grama: as primeiras grassCap especies da lista.
    for (i = 0; i < grassCap; ++i)
        grass[i] = (i < count) ? enc->species[i] : SPECIES_NONE;

    // Agua: as seguintes, comecando depois da grama.
    for (i = 0; i < waterCap; ++i)
    {
        u16 idx = grassCap + i;
        water[i] = (idx < count) ? enc->species[idx] : SPECIES_NONE;
    }
}

u16 RogueFixed_FillParty(const struct FixedPathRoomParty *party, struct Pokemon *out, u8 capacity)
{
    u16 count = (party != NULL) ? party->count : 0;
    u16 n = count;
    u16 i;

    // Nao criar mais mons do que cabe no time de destino.
    if (n > capacity)
        n = capacity;

    for (i = 0; i < n; ++i)
        CreateMon(&out[i], party->mons[i].species, party->mons[i].level,
                  USE_RANDOM_IVS, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);

    return n;
}

u8 RogueFixed_PartyMaxLevel(const struct FixedPathRoomParty *party)
{
    u16 count = (party != NULL) ? party->count : 0;
    u8 max = 0;
    u16 i;

    for (i = 0; i < count; ++i)
        if (party->mons[i].level > max)
            max = party->mons[i].level;

    return max;
}

u16 RogueFixed_TryCreateBossParty(u8 areaIdx, struct Pokemon *party, u8 capacity)
{
    if (areaIdx >= HACK_PATH_PARTY_COUNT || gHackPathParties[areaIdx].count == 0)
        return 0;

    return RogueFixed_FillParty(&gHackPathParties[areaIdx], party, capacity);
}
