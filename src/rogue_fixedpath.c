#include "global.h"
#include "rogue.h"
#include "rogue_fixedpath.h"

void RogueFixed_BuildPath(const struct FixedPathStep *steps, u16 count, struct RogueAdvPath *out)
{
    u16 i;

    if (count > ROGUE_ADVPATH_ROOM_CAPACITY)
        count = ROGUE_ADVPATH_ROOM_CAPACITY;

    out->roomCount = count;
    out->pathLength = count;
    out->pathMinY = 0;
    out->pathMaxY = 0;

    for (i = 0; i < count; ++i)
    {
        out->rooms[i].coords.x = i;
        out->rooms[i].coords.y = 0;
        out->rooms[i].roomType = steps[i].roomType;
        out->rooms[i].connectionMask = (i + 1 < count) ? ROOM_CONNECTION_MASK_MID : 0;
        out->rooms[i].rngSeed = 0;
        out->rooms[i].roomParams.roomIdx = i;

        // perType é uma união: escrever só o campo do tipo da sala.
        if (steps[i].roomType == ADVPATH_ROOM_BOSS)
            out->rooms[i].roomParams.perType.boss.trainerNum = steps[i].extra;
        else
            out->rooms[i].roomParams.perType.route.difficulty = steps[i].param;
    }
}
