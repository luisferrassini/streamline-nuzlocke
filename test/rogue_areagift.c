#include "global.h"
#include "test/test.h"
#include "rogue.h"
#include "rogue_areagift.h"
#include "rogue_adventurepaths.h"
#include "rogue_controller.h"
#include "string_util.h"
#include "../src/data/rogue/hack_path.h"

TEST("RogueAreaGift pending add/remove e resolved")
{
    RogueAreaGift_Reset();
    EXPECT_EQ(RogueAreaGift_PendingCount(), 0);
    EXPECT_EQ(RogueAreaGift_IsResolved(3), FALSE);
    EXPECT(RogueAreaGift_AddPending(3));
    EXPECT_EQ(RogueAreaGift_PendingCount(), 1);
    EXPECT_EQ(RogueAreaGift_PendingArea(0), 3);
    EXPECT_EQ(RogueAreaGift_IsResolved(3), TRUE);
    EXPECT_EQ(RogueAreaGift_AddPending(3), FALSE); // sem duplicata
    EXPECT_EQ(RogueAreaGift_PendingCount(), 1);
    RogueAreaGift_RemovePending(0);
    EXPECT_EQ(RogueAreaGift_PendingCount(), 0);
}

TEST("RogueAreaGift MarkResolved marca a area")
{
    RogueAreaGift_Reset();
    EXPECT_EQ(RogueAreaGift_IsResolved(5), FALSE);
    RogueAreaGift_MarkResolved(5);
    EXPECT_EQ(RogueAreaGift_IsResolved(5), TRUE);
    EXPECT_EQ(RogueAreaGift_PendingCount(), 0); // nao entra na fila
}

TEST("RogueAreaGift CurrentAreaIdx le a sala atual")
{
    u8 savedId = gRogueRun.adventureRoomId;
    u8 savedArea = gRogueAdvPath.rooms[1].areaIdx;

    gRogueRun.adventureRoomId = 1;
    gRogueAdvPath.rooms[1].areaIdx = 7;
    EXPECT_EQ(RogueAreaGift_CurrentAreaIdx(), 7);

    gRogueRun.adventureRoomId = savedId;
    gRogueAdvPath.rooms[1].areaIdx = savedArea;
}

TEST("RogueAreaGift AreaName le o nome da area")
{
    EXPECT_EQ(StringCompare(RogueAreaGift_AreaName(0), sHackPathName0), 0);
    EXPECT(RogueAreaGift_AreaName(HACK_PATH_AREA_COUNT) == NULL);
}

TEST("RogueAreaGift roll fica dentro da lista")
{
    u8 i;
    u16 esp = RogueAreaGift_RollSpecies(0);
    bool8 achou = FALSE;

    for (i = 0; i < gHackPathEncounters[0].count; ++i)
        if (gHackPathEncounters[0].species[i] == esp)
            achou = TRUE;
    EXPECT(achou);

    // Area vazia (o chefe) devolve SPECIES_NONE.
    for (i = 0; i < HACK_PATH_AREA_COUNT; ++i)
    {
        if (gHackPathEncounters[i].count == 0)
        {
            EXPECT_EQ(RogueAreaGift_RollSpecies(i), SPECIES_NONE);
            break;
        }
    }
    EXPECT(i < HACK_PATH_AREA_COUNT); // o dado de teste tem ao menos uma area vazia

    // Indice fora do intervalo devolve SPECIES_NONE.
    EXPECT_EQ(RogueAreaGift_RollSpecies(HACK_PATH_AREA_COUNT), SPECIES_NONE);
}
