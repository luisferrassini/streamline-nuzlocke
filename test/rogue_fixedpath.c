#include "global.h"
#include "test/test.h"
#include "rogue.h"
#include "rogue_fixedpath.h"
#include "rogue_adventurepaths.h"
#include "rogue_controller.h"
#include "rogue_trainers.h"
#include "../src/data/rogue/hack_path.h"

static const struct FixedPathStep sSteps[] =
{
    // Ordem de jogo: 1a area, 2a area, ultima area (o chefe).
    // param != extra de propósito: pega o bug da união (ver o teste)
    { ADVPATH_ROOM_ROUTE, 5,  7 },
    { ADVPATH_ROOM_ROUTE, 1,  0 },
    { ADVPATH_ROOM_BOSS,  0,  42 },
};

TEST("RogueFixed_BuildPath makes a straight line in play order")
{
    struct RogueAdvPath path;

    RogueFixed_BuildPath(sSteps, ARRAY_COUNT(sSteps), &path);

    EXPECT_EQ(path.roomCount, 3);
    EXPECT_EQ(path.pathLength, 3);
    EXPECT_EQ(path.pathMinY, 0);
    EXPECT_EQ(path.pathMaxY, 0);

    // coords.x = indice (mesma convenção da geração do jogo).
    EXPECT_EQ(path.rooms[0].coords.x, 0);
    EXPECT_EQ(path.rooms[1].coords.x, 1);
    EXPECT_EQ(path.rooms[2].coords.x, 2);
    EXPECT_EQ(path.rooms[0].coords.y, 0);

    // O jogo joga de coords.x maior para menor:
    // rooms[2] (coords.x == pathLength-1) é o início = 1a area.
    // rooms[0] (coords.x == 0) é o fim = última area (o chefe).
    EXPECT_EQ(path.rooms[2].roomType, ADVPATH_ROOM_ROUTE);
    EXPECT_EQ(path.rooms[1].roomType, ADVPATH_ROOM_ROUTE);
    EXPECT_EQ(path.rooms[0].roomType, ADVPATH_ROOM_BOSS);

    // A sala inicial (coords.x == pathLength-1) não tem conexão.
    EXPECT_EQ(path.rooms[2].connectionMask, 0);
    EXPECT(path.rooms[0].connectionMask & ROOM_CONNECTION_MASK_MID);
    EXPECT(path.rooms[1].connectionMask & ROOM_CONNECTION_MASK_MID);

    EXPECT_EQ(path.rooms[0].rngSeed, 0);

    // perType é uma união: só o campo do tipo da sala conta.
    EXPECT_EQ(path.rooms[0].roomParams.perType.boss.trainerNum, 42);
    EXPECT_EQ(path.rooms[1].roomParams.perType.route.difficulty, 1);
    EXPECT_EQ(path.rooms[2].roomParams.perType.route.difficulty, 5);

    // Indice da area (lista de encontros), na ordem do dado (nao invertido).
    EXPECT_EQ(path.rooms[2].areaIdx, 0); // rooms[2] guarda steps[0] (1a area)
    EXPECT_EQ(path.rooms[1].areaIdx, 1);
    EXPECT_EQ(path.rooms[0].areaIdx, 2); // rooms[0] guarda steps[2] (chefe)
}

TEST("RogueFixed_FillEncounterSlots fills grass then water")
{
    static const u16 sList[] = { SPECIES_TREECKO, SPECIES_TORCHIC, SPECIES_MUDKIP, SPECIES_PIKACHU };
    struct FixedPathRoomEncounters enc = { sList, 4 };
    struct FixedPathRoomEncounters curto = { sList, 1 };
    u16 grass[2];
    u16 water[2];

    RogueFixed_FillEncounterSlots(&enc, grass, 2, water, 2);
    EXPECT_EQ(grass[0], SPECIES_TREECKO);
    EXPECT_EQ(grass[1], SPECIES_TORCHIC);
    EXPECT_EQ(water[0], SPECIES_MUDKIP);
    EXPECT_EQ(water[1], SPECIES_PIKACHU);

    RogueFixed_FillEncounterSlots(&curto, grass, 2, water, 2);
    EXPECT_EQ(grass[0], SPECIES_TREECKO);
    EXPECT_EQ(grass[1], SPECIES_NONE);
    EXPECT_EQ(water[0], SPECIES_NONE);
    EXPECT_EQ(water[1], SPECIES_NONE);

    RogueFixed_FillEncounterSlots(NULL, grass, 2, water, 2);
    EXPECT_EQ(grass[0], SPECIES_NONE);
    EXPECT_EQ(water[0], SPECIES_NONE);
}

TEST("RogueFixed_FillParty cria os mons do time")
{
    static const struct FixedPathPartyMon sMons[] = {
        { SPECIES_GEODUDE, 15 }, { SPECIES_NOSEPASS, 14 }, { SPECIES_MAKUHITA, 12 },
    };
    struct FixedPathRoomParty team = { sMons, 3 };
    struct Pokemon party[2];
    u16 n = RogueFixed_FillParty(&team, party, 2);

    EXPECT_EQ(n, 2); // trunca na capacidade
    EXPECT_EQ(GetMonData(&party[0], MON_DATA_SPECIES), SPECIES_GEODUDE);
    EXPECT_EQ(GetMonData(&party[0], MON_DATA_LEVEL), 15);
    EXPECT_EQ(GetMonData(&party[1], MON_DATA_SPECIES), SPECIES_NOSEPASS);
    EXPECT_EQ(GetMonData(&party[1], MON_DATA_LEVEL), 14);

    EXPECT_EQ(RogueFixed_PartyMaxLevel(&team), 15);
    EXPECT_EQ(RogueFixed_FillParty(NULL, party, 2), 0);
    EXPECT_EQ(RogueFixed_PartyMaxLevel(NULL), 0);
}

TEST("RogueFixed_CalculateBossMonLvl vem do dado")
{
    u16 prev = gRogueRun.currentDifficulty;

    gRogueRun.currentDifficulty = 0;
    EXPECT_EQ(Rogue_CalculateBossMonLvl(), gHackPathBossCaps[0]);

    gRogueRun.currentDifficulty = 3;
    EXPECT_EQ(Rogue_CalculateBossMonLvl(), gHackPathBossCaps[3]);

    gRogueRun.currentDifficulty = HACK_PATH_BOSS_COUNT + 5; // fora do dado: nao trava
    EXPECT(Rogue_CalculateBossMonLvl() >= 1);
    EXPECT(Rogue_CalculateBossMonLvl() <= MAX_LEVEL);

    gRogueRun.currentDifficulty = prev;
}

TEST("RogueFixed_BuildPath usa bossTrainerNums quando extra e 0")
{
    static const struct FixedPathStep sBossSteps[] =
    {
        // Ordem de jogo: rota, chefe (extra == 0, sem treinador fixo no dado).
        { ADVPATH_ROOM_ROUTE, 4, 0 },
        { ADVPATH_ROOM_BOSS,  0, 0 },
    };
    struct RogueAdvPath path;
    u16 saved[ROGUE_MAX_BOSS_COUNT];
    u16 i;

    for (i = 0; i < ROGUE_MAX_BOSS_COUNT; ++i)
    {
        saved[i] = gRogueRun.bossTrainerNums[i];
        gRogueRun.bossTrainerNums[i] = 0;
    }
    gRogueRun.bossTrainerNums[0] = 7;

    RogueFixed_BuildPath(sBossSteps, ARRAY_COUNT(sBossSteps), &path);

    EXPECT_EQ(path.rooms[0].roomType, ADVPATH_ROOM_BOSS);
    EXPECT_EQ(path.rooms[0].roomParams.perType.boss.trainerNum, 7);

    for (i = 0; i < ROGUE_MAX_BOSS_COUNT; ++i)
        gRogueRun.bossTrainerNums[i] = saved[i];
}

TEST("RogueFixed_BuildPath mapeia chefe pela ordem de jogo")
{
    static const struct FixedPathStep sBossSteps[] =
    {
        // Ordem de jogo == ordem do dado: o 1o chefe tem ordinal 0.
        { ADVPATH_ROOM_BOSS,  0, 0 }, // 1o chefe (ordinal 0)
        { ADVPATH_ROOM_ROUTE, 4, 0 },
        { ADVPATH_ROOM_BOSS,  0, 0 }, // 2o chefe (ordinal 1)
    };
    struct RogueAdvPath path;
    u16 saved[ROGUE_MAX_BOSS_COUNT];
    u16 i;

    for (i = 0; i < ROGUE_MAX_BOSS_COUNT; ++i)
    {
        saved[i] = gRogueRun.bossTrainerNums[i];
        gRogueRun.bossTrainerNums[i] = 0;
    }
    gRogueRun.bossTrainerNums[0] = 100;
    gRogueRun.bossTrainerNums[1] = 200;

    RogueFixed_BuildPath(sBossSteps, ARRAY_COUNT(sBossSteps), &path);

    // O 1o chefe do dado cai na ultima sala (rooms[2]); o 2o, na primeira.
    EXPECT_EQ(path.rooms[2].roomParams.perType.boss.trainerNum, 100);
    EXPECT_EQ(path.rooms[0].roomParams.perType.boss.trainerNum, 200);

    for (i = 0; i < ROGUE_MAX_BOSS_COUNT; ++i)
        gRogueRun.bossTrainerNums[i] = saved[i];
}

TEST("RogueFixed_TryCreateBossParty usa o time da area")
{
    u8 areaIdx = 0;
    u8 savedRoomId = gRogueRun.adventureRoomId;
    struct RogueAdvPathRoom savedRoom = gRogueAdvPath.rooms[0];
    u16 monCount;

    // Primeira area do dado que tem time fixo.
    while (areaIdx < HACK_PATH_PARTY_COUNT && gHackPathParties[areaIdx].count == 0)
        ++areaIdx;
    EXPECT(areaIdx < HACK_PATH_PARTY_COUNT); // o dado tem ao menos um time

    // Funcao direta.
    monCount = RogueFixed_TryCreateBossParty(areaIdx, gEnemyParty, PARTY_SIZE);
    EXPECT_EQ(monCount, gHackPathParties[areaIdx].count);
    EXPECT_EQ(GetMonData(&gEnemyParty[0], MON_DATA_SPECIES), gHackPathParties[areaIdx].mons[0].species);
    EXPECT_EQ(GetMonData(&gEnemyParty[0], MON_DATA_LEVEL), gHackPathParties[areaIdx].mons[0].level);

    // Area sem chefe / fora do intervalo: 0.
    EXPECT_EQ(RogueFixed_TryCreateBossParty(HACK_PATH_PARTY_COUNT, gEnemyParty, PARTY_SIZE), 0);

    // Integra com Rogue_CreateTrainerParty: a sala de chefe usa o time do dado.
    ZeroEnemyPartyMons();
    gRogueRun.adventureRoomId = 0;
    gRogueAdvPath.rooms[0].roomType = ADVPATH_ROOM_BOSS;
    gRogueAdvPath.rooms[0].areaIdx = areaIdx;

    monCount = Rogue_CreateTrainerParty(0, gEnemyParty, PARTY_SIZE, TRUE);
    EXPECT_EQ(monCount, gHackPathParties[areaIdx].count);
    EXPECT_EQ(GetMonData(&gEnemyParty[0], MON_DATA_SPECIES), gHackPathParties[areaIdx].mons[0].species);
    EXPECT_EQ(GetMonData(&gEnemyParty[0], MON_DATA_LEVEL), gHackPathParties[areaIdx].mons[0].level);

    // Restaura o estado global.
    gRogueRun.adventureRoomId = savedRoomId;
    gRogueAdvPath.rooms[0] = savedRoom;
}
