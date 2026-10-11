#include "global.h"
#include "test/test.h"
#include "rogue.h"
#include "rogue_areagift.h"
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
