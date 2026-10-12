#include "global.h"
#include "random.h"
#include "rogue.h"
#include "rogue_controller.h"
#include "rogue_adventurepaths.h"
#include "rogue_areagift.h"
#include "data/rogue/hack_path.h"

u8 RogueAreaGift_CurrentAreaIdx(void)
{
    return gRogueAdvPath.rooms[gRogueRun.adventureRoomId].areaIdx;
}

const u8 *RogueAreaGift_AreaName(u8 areaIdx)
{
    if(areaIdx >= HACK_PATH_AREA_COUNT)
        return NULL;
    return gHackPathAreaNames[areaIdx];
}

u16 RogueAreaGift_RollSpecies(u8 areaIdx)
{
    const struct FixedPathRoomEncounters *enc;

    if (areaIdx >= HACK_PATH_AREA_COUNT)
        return SPECIES_NONE;

    enc = &gHackPathEncounters[areaIdx];
    if (enc->count == 0 || enc->species == NULL)
        return SPECIES_NONE;

    return enc->species[Random() % enc->count];
}

bool8 RogueAreaGift_IsResolved(u8 areaIdx)
{
    u8 byte = areaIdx / 8;
    u8 bit = areaIdx % 8;

    if (byte >= GIFT_RESOLVED_BYTES)
        return FALSE;
    return (gRogueRun.giftResolved[byte] >> bit) & 1;
}

static void SetResolved(u8 areaIdx)
{
    u8 byte = areaIdx / 8;
    u8 bit = areaIdx % 8;

    if (byte < GIFT_RESOLVED_BYTES)
        gRogueRun.giftResolved[byte] |= (1 << bit);
}

void RogueAreaGift_MarkResolved(u8 areaIdx)
{
    SetResolved(areaIdx);
}

bool8 RogueAreaGift_AddPending(u8 areaIdx)
{
    u8 i;

    if (gRogueRun.giftPendingCount >= GIFT_PENDING_CAPACITY)
        return FALSE;

    for (i = 0; i < gRogueRun.giftPendingCount; ++i)
        if (gRogueRun.giftPending[i] == areaIdx)
            return FALSE;

    gRogueRun.giftPending[gRogueRun.giftPendingCount] = areaIdx;
    gRogueRun.giftPendingCount++;
    SetResolved(areaIdx);
    return TRUE;
}

u8 RogueAreaGift_PendingCount(void)
{
    return gRogueRun.giftPendingCount;
}

u8 RogueAreaGift_PendingArea(u8 index)
{
    if (index >= gRogueRun.giftPendingCount)
        return 0xFF;
    return gRogueRun.giftPending[index];
}

void RogueAreaGift_RemovePending(u8 index)
{
    u8 i;

    if (index >= gRogueRun.giftPendingCount)
        return;

    for (i = index; i + 1 < gRogueRun.giftPendingCount; ++i)
        gRogueRun.giftPending[i] = gRogueRun.giftPending[i + 1];
    gRogueRun.giftPendingCount--;
}

void RogueAreaGift_Reset(void)
{
    memset(gRogueRun.giftResolved, 0, sizeof(gRogueRun.giftResolved));
    memset(gRogueRun.giftPending, 0, sizeof(gRogueRun.giftPending));
    gRogueRun.giftPendingCount = 0;
}
