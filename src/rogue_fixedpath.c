#include "global.h"
#include "gba/isagbprint.h"
#include "rogue.h"
#include "rogue_fixedpath.h"

void RogueFixed_BuildPath(const struct FixedPathStep *steps, u16 count, struct RogueAdvPath *out)
{
    u16 i;

    // O gerador (tools/hack/gen_path_header.py) já recusa um caminho maior que
    // isto. O assert torna o limite visível em vez de cortar em silêncio.
    AGB_ASSERT(count <= ROGUE_ADVPATH_ROOM_CAPACITY);
    if (count > ROGUE_ADVPATH_ROOM_CAPACITY)
        count = ROGUE_ADVPATH_ROOM_CAPACITY;

    out->roomCount = count;
    out->pathLength = count;
    out->pathMinY = 0;
    out->pathMaxY = 0;

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

        // perType é uma união: escrever só o campo do tipo da sala.
        if (step->roomType == ADVPATH_ROOM_BOSS)
            out->rooms[i].roomParams.perType.boss.trainerNum = step->extra;
        else
            out->rooms[i].roomParams.perType.route.difficulty = step->param;
    }
}
