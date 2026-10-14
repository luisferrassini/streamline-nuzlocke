# Plano 3 — Chefes fixos e teto de nível

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Objetivo:** Cada sala de chefe do caminho luta com o **time fixo da referência (espécie + nível)**; o **teto de nível** do jogador passa a vir do chefe da área e **bloqueia** (não dá para passar do teto, nem com Rare Candy).

**Arquitetura:** O time de cada chefe vive no JSON do caminho (mesmo pipeline do Plano 1/2) e vira um header C. Na batalha, `Rogue_CreateTrainerParty` deixa de sortear espécies para salas de chefe e cria os mons da lista fixa. O teto de nível vem de uma tabela gerada (maior nível do time de cada chefe) e substitui a fórmula por dificuldade em `Rogue_CalculateBossMonLvl()`. O bloqueio acontece no ganho de EXP e no uso de itens de nível (Rare Candy / EXP Candy).

**Tech Stack:** C (pokeemerald decomp, `arm-none-eabi`/agbcc), Python 3 (gerador de dados), testes unitários C rodando no `mgba` headless.

**Spec:** `docs/superpowers/specs/2026-10-11-nuzlocke-enxuto-design.md`. Roadmap: `docs/superpowers/plans/2026-10-11-nuzlocke-enxuto-roadmap.md` (item 3).

## Escopo (decidido com o jogador)

- **Nesta fase, time do chefe = só espécie + nível.** Golpes, habilidade e item de cada Pokémon ficam para um plano futuro de detalhes dos chefes (virão do `league.json` pelo campo `boss` que este plano grava).
- Identidade do treinador (nome/foto/classes): **decisão minha** — reusar o pool de chefes que o Rogue já sorteia (`gRogueRun.bossTrainerNums`), sobrepondo o time fixo do Kaizo. Treinadores dedicados por chefe do Kaizo ficam para o plano de conteúdo.
- Time do chefe é truncado em `PARTY_SIZE` (6). Rivals do `league.json` têm 12 mons (variantes de starter); só os 6 primeiros entram agora.

## Restrições globais

- Build da ROM: `MODERN=1 make -j2 -O all`. Testes: `MODERN=1 make check`. Filtrar: `TESTS="<prefixo>"`.
- Toolchain: `arm-none-eabi-gcc` (pacote Arch). Não usar devkitARM.
- Dados de caminho: **não** editar `src/data/rogue/hack_path.h` à mão. Fonte é `src/data/rogue/hack_path.json`.
- Não mexemos em `include/rogue.h` nem `include/constants/rogue.h` (evita rebuild quase total de ~30 min). Header leve: `include/rogue_fixedpath.h` (não está em `global.h`).
- `ROGUE_ADVPATH_ROOM_CAPACITY` = 32; o gerador recusa caminho acima de `MAX_ROOMS` = 32 (`tools/hack/gen_path_header.py`).
- `ROGUE_MAX_BOSS_COUNT` = 14 (`include/constants/rogue.h:389`); `PARTY_SIZE` = 6 (`include/constants/global.h:40`).
- `gRogueRun.bossTrainerNums[]` é preenchido no início da run (`Rogue_ChooseBossTrainersForNewAdventure`, `src/rogue_trainers.c:1100`).
- `struct RogueRunData` (`gRogueRun`) é serializado inteiro (`src/rogue_save.c:251`). Este plano **não** muda o layout do save.
- Gerador roda via regra `src/data/rogue/custom_json_rules.mk`.

## Foco de revisão

O que o desenho implica mas nenhum teste cobre, em ordem de risco. Cada item vira um teste na tarefa dona do código.

1. **Chefe com time vazio ou curto.** Um chefe sem `party` (ou com menos mons que o esperado) não pode estourar a leitura. `RogueFixed_FillParty`/`RogueFixed_PartyMaxLevel` tratam NULL/vazio; `TryCreateBossParty` devolve 0 e a geração normal assume. (Task 1/3)
2. **Time maior que `PARTY_SIZE`.** Rivals têm 12 mons; truncar em 6 sem ler além do array. (Task 1/3)
3. **Cap cai abaixo do nível atual.** A ordem do tracker não é monotônica (ex.: Mt. Chimney/Tabitha lvl 38 antes de Brawly lvl 19). Um mon acima do novo teto não pode baixar de nível nem travar; só para de ganhar. (Task 2/4)
4. **Dificuldade além dos dados.** `Rogue_GetCurrentDifficulty() >= HACK_PATH_BOSS_COUNT` não tem cap no dado → cai na fórmula antiga (`CalculateLvlFor`). (Task 2)
5. **Uso de item de nível fora de run / no teto.** Sem run, Rare Candy continua normal; no teto, vira no-op. (Task 4)
6. **Batalha de chefe sem treinador válido.** Hoje `trainer: 0` = slot `TRAINER_NONE` vazio = batalha de chefe sem classe/pic/flags. (Task 3)

---

### Task 1: Times de chefe nos dados (JSON → header) e funções puras

**Files:**
- Modify: `src/data/rogue/hack_path.json`
- Modify: `tools/hack/gen_path_header.py`
- Modify: `tools/hack/test_gen_path_header.py`
- Modify: `include/rogue_fixedpath.h`
- Modify: `src/rogue_fixedpath.c`
- Test: `test/rogue_fixedpath.c`

**Interfaces:**
- Consumes: `struct FixedPathStep` (`include/rogue_fixedpath.h:8`); `species_const()`/`load_species_constants()` (`tools/hack/gen_path_header.py`).
- Produces:
  - `struct FixedPathPartyMon { u16 species; u8 level; };`
  - `struct FixedPathRoomParty { const struct FixedPathPartyMon *mons; u16 count; };`
  - No header gerado: `gHackPathParties[]` (1 por sala, ordem do JSON), `gHackPathBossCaps[]` (1 por sala de chefe, ordem do JSON; maior nível do time, 0 se vazio), `HACK_PATH_PARTY_COUNT`, `HACK_PATH_BOSS_COUNT`.
  - `u16 RogueFixed_FillParty(const struct FixedPathRoomParty *party, struct Pokemon *out, u8 capacity);` → cria `min(count, capacity)` mons com `CreateMon` e devolve o número criado.
  - `u8 RogueFixed_PartyMaxLevel(const struct FixedPathRoomParty *party);` → maior nível; 0 se NULL/vazio.

- [ ] **Step 1: Escrever os testes Python que falham**

Em `tools/hack/test_gen_path_header.py`, no `JSON_SAMPLE`, dar a cada sala de chefe `"boss"` e `"party"` (ex.: `"boss": "1"`, `"party": [{"species": "geodude", "level": 15}, {"species": "nosepass", "level": 14}]`). Adicionar checagens (a função `check(name, ok)` já existe):
- `gHackPathParties` tem uma entrada por sala;
- a entrada da 1ª sala de chefe referencia `SPECIES_GEODUDE` e tem `count` igual ao tamanho do `party`;
- `gHackPathBossCaps` existe, tem uma entrada por sala de chefe e a 1ª é `15`;
- um `level` não inteiro ou uma espécie desconhecida no `party` levanta `ValueError`.

- [ ] **Step 2: Rodar e ver falhar**

Run: `python3 tools/hack/test_gen_path_header.py`
Expected: FAIL (o gerador ainda não emite `gHackPathParties`/`gHackPathBossCaps`).

- [ ] **Step 3: Estender o JSON**

Cada sala de chefe ganha `"boss"` (valor do Kaizo) e `"party"` (lista `{species, level}`). As 11 salas de chefe atuais são as entradas de chefe 0..10 do `routes.json` emkaizo, nesta ordem:

`["red1", "b0", "r0", "1", "mt1", "mtmax1", "2", "w1", "3", "b1", "r1"]`

O time sai do `league.json` (chave `emkaizo[<valor>].pokemon`), truncado em 6. Rodar uma vez (e conferir o diff):

```python
import json, urllib.request
lg = json.load(urllib.request.urlopen(
    "https://raw.githubusercontent.com/Bassel-T/nuzlocke.app/main/src/lib/data/league.json"))["emkaizo"]
data = json.load(open("src/data/rogue/hack_path.json"))
vals = ["red1","b0","r0","1","mt1","mtmax1","2","w1","3","b1","r1"]
k = 0
for room in data["rooms"]:
    if room["type"] == "boss":
        room.pop("trainer", None)
        room["boss"] = vals[k]
        room["party"] = [{"species": p["name"], "level": int(p["level"])}
                         for p in lg[vals[k]]["pokemon"]][:6]
        k += 1
assert k == len(vals)
json.dump(data, open("src/data/rogue/hack_path.json", "w"), indent=2, ensure_ascii=False)
```

- [ ] **Step 4: Estender o gerador**

Em `tools/hack/gen_path_header.py`:
- `parse()` passa a devolver 4 valores: `(entries, encounter_lists, names, party_lists)`. Em `party_lists`, cada sala é uma lista de `(SPECIES_*, level)`; sala sem `party` → lista vazia. Validar cada espécie com `species_const()` (desconhecida → `ValueError`) e cada nível com `int()` (`ValueError` se não inteiro).
- `render(entries, encounter_lists, names, party_lists)` emite, depois da tabela de encontros:
  ```c
  static const struct FixedPathPartyMon sHackPathParty<i>[] = { { SPECIES_GEODUDE, 15 }, ... };
  static const struct FixedPathRoomParty gHackPathParties[] =
  {
      { NULL, 0 },
      ...
      { sHackPathParty7, 6 },
  };
  #define HACK_PATH_PARTY_COUNT ARRAY_COUNT(gHackPathParties)
  ```
  e, na ordem do JSON, só para salas `ADVPATH_ROOM_BOSS`, o maior nível de cada `party` (0 se vazia):
  ```c
  static const u8 gHackPathBossCaps[] = { 9, 16, 16, 15, 38, 39, 19, 70, 29, 26, 26 };
  #define HACK_PATH_BOSS_COUNT ARRAY_COUNT(gHackPathBossCaps)
  ```

- [ ] **Step 5: Declarar os tipos no header público**

Em `include/rogue_fixedpath.h`, antes de `RogueFixed_BuildPath`:
```c
struct FixedPathPartyMon
{
    u16 species; // SPECIES_*
    u8 level;
};

struct FixedPathRoomParty
{
    const struct FixedPathPartyMon *mons; // NULL se vazio
    u16 count;
};
```
E declarar `RogueFixed_FillParty` e `RogueFixed_PartyMaxLevel` ao final.

- [ ] **Step 6: Gerar o header e ver os testes Python passarem**

Run: `python3 tools/hack/gen_path_header.py`
Run: `python3 tools/hack/test_gen_path_header.py`
Expected: `gerado: .../hack_path.h`; `TUDO PASS`.

- [ ] **Step 7: Escrever o teste C que falha**

Em `test/rogue_fixedpath.c`, novo `TEST("RogueFixed_FillParty cria os mons do time")`:
```c
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
```

- [ ] **Step 8: Rodar e ver falhar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: FAIL (`RogueFixed_FillParty` não existe).

- [ ] **Step 9: Implementar em `src/rogue_fixedpath.c`**

Incluir `"data/rogue/hack_path.h"`. Implementar as duas funções conforme a interface: `FillParty` limita `n = min(count, capacity)`, cria cada mon com `CreateMon(&out[i], species, level, USE_RANDOM_IVS, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0)` e devolve `n`. `PartyMaxLevel` percorre `count` (NULL/vazio → 0).

- [ ] **Step 10: Rodar e ver passar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: PASS.

- [ ] **Step 11: Commit**

```bash
git add src/data/rogue/hack_path.json include/rogue_fixedpath.h src/rogue_fixedpath.c tools/hack/gen_path_header.py tools/hack/test_gen_path_header.py test/rogue_fixedpath.c
git commit -m "feat: times de chefe (especie e nivel) nos dados do caminho"
```

---

### Task 2: Teto de nível por chefe vindo do dado

**Files:**
- Modify: `src/rogue_trainers.c` (`Rogue_CalculateBossMonLvl`, `:451`; incluir o header gerado)
- Test: `test/rogue_fixedpath.c`

**Interfaces:**
- Consumes: `gHackPathBossCaps`, `HACK_PATH_BOSS_COUNT` (Task 1).
- Produces: `Rogue_CalculateBossMonLvl(void)` mantém assinatura; passa a devolver `gHackPathBossCaps[difficulty]` quando `difficulty < HACK_PATH_BOSS_COUNT` e o cap do dado é `!= 0`; caso contrário, a fórmula antiga `CalculateLvlFor(difficulty)`.

- [ ] **Step 1: Escrever o teste que falha**

Em `test/rogue_fixedpath.c`, novo `TEST("Rogue_CalculateBossMonLvl vem do dado")` (incluir `rogue_trainers.h` e `data/rogue/hack_path.h`; salvar/restaurar `gRogueRun.currentDifficulty`):
```c
u8 prev = gRogueRun.currentDifficulty;

gRogueRun.currentDifficulty = 0;
EXPECT_EQ(Rogue_CalculateBossMonLvl(), gHackPathBossCaps[0]);

gRogueRun.currentDifficulty = 3;
EXPECT_EQ(Rogue_CalculateBossMonLvl(), gHackPathBossCaps[3]);

gRogueRun.currentDifficulty = HACK_PATH_BOSS_COUNT + 5; // fora do dado: não trava
EXPECT(Rogue_CalculateBossMonLvl() >= 1);
EXPECT(Rogue_CalculateBossMonLvl() <= MAX_LEVEL);

gRogueRun.currentDifficulty = prev;
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: FAIL (o valor 0/3 ainda é a fórmula antiga).

- [ ] **Step 3: Implementar**

Em `src/rogue_trainers.c`: incluir `"data/rogue/hack_path.h"`. Reescrever:
```c
u8 Rogue_CalculateBossMonLvl()
{
    u8 difficulty = Rogue_GetCurrentDifficulty();

    if (difficulty < HACK_PATH_BOSS_COUNT && gHackPathBossCaps[difficulty] != 0)
        return gHackPathBossCaps[difficulty];

    return CalculateLvlFor(difficulty);
}
```

- [ ] **Step 4: Rodar e ver passar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: PASS.

- [ ] **Step 5: Build + suíte completa**

Run: `MODERN=1 make -j2 -O all`
Expected: EXIT=0, `pokeemerald.gba` (32 MB).

Run: `MODERN=1 make -j2 check`
Expected: EXIT=0; `FAILED: 0`.

- [ ] **Step 6: Commit**

```bash
git add src/rogue_trainers.c test/rogue_fixedpath.c
git commit -m "feat: teto de nivel por chefe vindo da lista do caminho"
```

---

### Task 3: Time fixo do chefe na batalha (e treinador válido)

**Files:**
- Modify: `include/rogue_fixedpath.h`
- Modify: `src/rogue_fixedpath.c` (builder + `RogueFixed_TryCreateBossParty`)
- Modify: `src/rogue_trainers.c` (`Rogue_CreateTrainerParty`, `:2127`)
- Test: `test/rogue_fixedpath.c`

**Interfaces:**
- Consumes: `gHackPathParties`, `HACK_PATH_PARTY_COUNT` (Task 1); `gRogueRun.bossTrainerNums` (existente); `RogueFixed_FillParty` (Task 1).
- Produces:
  - `u16 RogueFixed_TryCreateBossParty(u8 areaIdx, struct Pokemon *party, u8 capacity);` → se `areaIdx >= HACK_PATH_PARTY_COUNT` ou `gHackPathParties[areaIdx].count == 0`, devolve 0; senão preenche via `RogueFixed_FillParty` e devolve o número criado.
  - Builder: em sala `ADVPATH_ROOM_BOSS`, se `step->extra == 0`, usar `gRogueRun.bossTrainerNums[<ordinal do chefe na ordem de jogo>]` (limitado a `ROGUE_MAX_BOSS_COUNT`); `extra != 0` continua vencendo (mantém o teste atual).

- [ ] **Step 1: Escrever o testes que falham**

Em `test/rogue_fixedpath.c`:
- Estender o teste do builder: adicionar um `TEST` que zera `gRogueRun.bossTrainerNums`, seta `gRogueRun.bossTrainerNums[0] = 7`, chama o builder com um step de chefe de `extra == 0` e espera `rooms[..].perType.boss.trainerNum == 7`.
- Novo `TEST("RogueFixed_TryCreateBossParty usa o time da area")`: varrer `gHackPathParties` pela primeira entrada com `count > 0` (guardar `areaIdx`); setar `gRogueRun.adventureRoomId` e `gRogueAdvPath.rooms[..]` (roomType `ADVPATH_ROOM_BOSS`, `areaIdx`); chamar `Rogue_CreateTrainerParty(0, party, PARTY_SIZE, TRUE)`; esperar que `party[0]` tenha espécie e nível iguais a `gHackPathParties[areaIdx].mons[0]`. Restaurar o estado ao final.

- [ ] **Step 2: Rodar e ver falhar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: FAIL (`RogueFixed_TryCreateBossParty` não existe; treinador do chefe não vem de `bossTrainerNums`).

- [ ] **Step 3: Implementar o builder e a função**

Em `src/rogue_fixedpath.c`:
- No laço do builder, manter um contador `bossOrdinal` (ordem de jogo). Para `ADVPATH_ROOM_BOSS`: `u16 tn = step->extra; if (tn == 0 && bossOrdinal < ROGUE_MAX_BOSS_COUNT) tn = gRogueRun.bossTrainerNums[bossOrdinal];` gravar `tn`; `++bossOrdinal`.
- Implementar `RogueFixed_TryCreateBossParty` conforme a interface. Declarar em `include/rogue_fixedpath.h`.

- [ ] **Step 4: Ligar na criação do time**

Em `src/rogue_trainers.c`, `Rogue_CreateTrainerParty` (`:2127`): antes do `if(Rogue_IsRivalTrainer...)`, se `gRogueRun.adventureRoomId < ROGUE_ADVPATH_ROOM_CAPACITY` e `gRogueAdvPath.rooms[gRogueRun.adventureRoomId].roomType == ADVPATH_ROOM_BOSS`, chamar `RogueFixed_TryCreateBossParty(room->areaIdx, party, monCapacity)`; se `> 0`, esse é o `monCount` (pula rival/normal). O resto da função (pokeball) continua igual. Incluir `"rogue_fixedpath.h"`.

- [ ] **Step 5: Rodar e ver passar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: PASS.

- [ ] **Step 6: Build + suíte completa**

Run: `MODERN=1 make -j2 -O all`
Expected: EXIT=0.

Run: `MODERN=1 make -j2 check`
Expected: EXIT=0; `FAILED: 0`.

- [ ] **Step 7: Commit**

```bash
git add include/rogue_fixedpath.h src/rogue_fixedpath.c src/rogue_trainers.c test/rogue_fixedpath.c
git commit -m "feat: sala de chefe luta com o time fixo e treinador valido"
```

---

### Task 4: O teto bloqueia (EXP e itens de nível)

**Files:**
- Modify: `include/rogue_controller.h` (declarar `Rogue_CanLevelUpMon`)
- Modify: `src/rogue_controller.c` (`Rogue_ModifyExpGained`, `:968`; nova `Rogue_CanLevelUpMon`)
- Modify: `src/pokemon.c` (gate do item de nível, `:3584-3613`)
- Test: `test/rogue_levelcap.c` (novo)

**Interfaces:**
- Consumes: `Rogue_CalculateBossMonLvl` (Task 2), `Rogue_IsRunActive` (`src/rogue_controller.c:326`), `Rogue_ModifyExpGained` (existente).
- Produces:
  - `bool8 Rogue_CanLevelUpMon(struct Pokemon *mon);` → `TRUE` se `!Rogue_IsRunActive()`; senão `FALSE` quando o nível atual `>= Rogue_CalculateBossMonLvl()` (ou `>= MAX_LEVEL`), `TRUE` caso contrário.
  - `Rogue_ModifyExpGained`: quando `currentLevel >= Rogue_CalculateBossMonLvl()`, `*expGain = 0` (remove o ramo de 34% do `CONFIG_TOGGLE_OVER_LVL`).

- [ ] **Step 1: Escrever os testes que falham**

Criar `test/rogue_levelcap.c` (inclui `global.h`, `test/test.h`, `rogue.h`, `rogue_controller.h`, `rogue_trainers.h`, `data/rogue/hack_path.h`):
```c
TEST("Rogue_CanLevelUpMon para no teto do chefe atual")
{
    struct Pokemon mon;
    u8 cap;
    bool8 prevRun;

    prevRun = FlagGet(FLAG_ROGUE_RUN_ACTIVE);
    FlagSet(FLAG_ROGUE_RUN_ACTIVE);
    gRogueRun.currentDifficulty = 0;
    gRogueRun.gameRules.disablePerBadgeLvlCaps = FALSE;
    cap = Rogue_CalculateBossMonLvl();

    CreateMon(&mon, SPECIES_GEODUDE, cap, USE_RANDOM_IVS, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
    EXPECT_EQ(Rogue_CanLevelUpMon(&mon), FALSE);
    CreateMon(&mon, SPECIES_GEODUDE, cap - 1, USE_RANDOM_IVS, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
    EXPECT_EQ(Rogue_CanLevelUpMon(&mon), TRUE);

    if (!prevRun)
        FlagClear(FLAG_ROGUE_RUN_ACTIVE);
}

TEST("Rogue_ModifyExpGained zera no teto")
{
    struct Pokemon mon;
    s32 expGain = 12345;
    u8 cap;

    FlagSet(FLAG_ROGUE_RUN_ACTIVE);
    gRogueRun.currentDifficulty = 0;
    cap = Rogue_CalculateBossMonLvl();

    CreateMon(&mon, SPECIES_GEODUDE, cap, USE_RANDOM_IVS, FALSE, 0, OT_ID_RANDOM_NO_SHINY, 0);
    Rogue_ModifyExpGained(&mon, &expGain);
    EXPECT_EQ(expGain, 0);
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `MODERN=1 make -j2 check TESTS="Rogue_LevelCap"`
Expected: FAIL (`Rogue_CanLevelUpMon` não existe; EXP ainda dá 34% no teto).

- [ ] **Step 3: Implementar o bloqueio**

- Em `src/rogue_controller.c`, implementar `Rogue_CanLevelUpMon` conforme a interface e declarar em `include/rogue_controller.h`.
- Em `Rogue_ModifyExpGained` (`:1003-1014`), trocar o ramo `else` por `desiredExpPerc = 0;` (sem o toggle de 34%). Manter o resto.
- Em `src/pokemon.c` (`:3587-3588`), no ramo `ITEM3_LEVEL_UP`, acrescentar `&& Rogue_CanLevelUpMon(mon)` à condição, para que Rare Candy / EXP Candy parem no teto. Incluir `"rogue_controller.h"`.

- [ ] **Step 4: Rodar e ver passar**

Run: `MODERN=1 make -j2 check TESTS="Rogue_LevelCap"`
Expected: PASS.

- [ ] **Step 5: Build + suíte completa**

Run: `MODERN=1 make -j2 -O all`
Expected: EXIT=0.

Run: `MODERN=1 make -j2 check`
Expected: EXIT=0; `FAILED: 0`.

- [ ] **Step 6: Commit**

```bash
git add include/rogue_controller.h src/rogue_controller.c src/pokemon.c test/rogue_levelcap.c
git commit -m "feat: teto de nivel bloqueia o ganho de nivel e o Rare Candy"
```

---

### Task 5: Robustez do teto quando o nível cai (não estoura `u8`)

> **Adicionada durante a execução (ruling do controlador, achado do review da Task 2).** A ordem do caminho não é monotônica (o teto cai de 39→19, de 70→29, etc.). `gRogueRun.currentLevelOffset` é `u8` e hoje é calculado como `nextLevel - prevLevel` (`src/rogue_controller.c:7130`); quando o próximo teto é menor, isso estoura (ex.: `19 - 39 = 236`) e faz `Rogue_CalculatePlayerMonLvl()` devolver um teto alto (ex.: 39) em vez do novo teto (19) por dezenas de batalhas. Correção mínima: o offset só cresce quando o teto sobe; as subtrações saturam em 1.

**Files:**
- Modify: `include/rogue_controller.h` (declarar `Rogue_LevelOffsetForNextBoss`)
- Modify: `src/rogue_controller.c` (`:7130`)
- Modify: `src/rogue_trainers.c` (`Rogue_CalculatePlayerMonLvl` `:386`, `Rogue_CalculateTrainerLvlCap` ramo MP `:377`)
- Test: `test/rogue_levelcap.c`

**Interfaces:**
- Consumes: `Rogue_CalculateBossMonLvl` (Task 2); `gRogueRun.currentLevelOffset`.
- Produces:
  - `u8 Rogue_LevelOffsetForNextBoss(u8 prevLevel, u8 nextLevel);` → `nextLevel > prevLevel ? nextLevel - prevLevel : 0`.
  - `Rogue_CalculatePlayerMonLvl()` nunca devolve 0 nem estoura: satura em `1` quando `offset >= teto`.
  - `Rogue_CalculateTrainerLvlCap` (ramo MP `RogueMP_IsActive()`) idem.

- [ ] **Step 1: Escrever os testes que falham**

Em `test/rogue_levelcap.c` (anexar):
```c
TEST("Rogue_LevelOffsetForNextBoss nao estoura quando o teto cai")
{
    EXPECT_EQ(Rogue_LevelOffsetForNextBoss(9, 16), 7);
    EXPECT_EQ(Rogue_LevelOffsetForNextBoss(16, 16), 0);
    EXPECT_EQ(Rogue_LevelOffsetForNextBoss(39, 19), 0);
}

TEST("Rogue_CalculatePlayerMonLvl satura quando o offset passa do teto")
{
    u16 prevDiff = gRogueRun.currentDifficulty;
    u8 prevOffset = gRogueRun.currentLevelOffset;

    gRogueRun.gameRules.disablePerBadgeLvlCaps = FALSE;
    gRogueRun.currentDifficulty = 0; // teto do dado = 9
    gRogueRun.currentLevelOffset = 20;
    EXPECT(Rogue_CalculatePlayerMonLvl() >= 1);
    EXPECT(Rogue_CalculatePlayerMonLvl() <= Rogue_CalculateBossMonLvl());

    gRogueRun.currentDifficulty = prevDiff;
    gRogueRun.currentLevelOffset = prevOffset;
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `MODERN=1 make -j2 check TESTS="Rogue_LevelCap"`
Expected: FAIL (`Rogue_LevelOffsetForNextBoss` não existe; `Rogue_CalculatePlayerMonLvl` devolve 245).

- [ ] **Step 3: Implementar**

- Em `src/rogue_controller.c`, adicionar `Rogue_LevelOffsetForNextBoss` e declarar em `include/rogue_controller.h`. Em `:7130`, trocar `nextLevel - prevLevel` por `Rogue_LevelOffsetForNextBoss(prevLevel, nextLevel)`.
- Em `src/rogue_trainers.c`, saturar as duas subtrações: devolver `1` quando o offset alcança o teto (em `Rogue_CalculatePlayerMonLvl` e no ramo MP de `Rogue_CalculateTrainerLvlCap`).

- [ ] **Step 4: Rodar e ver passar**

Run: `MODERN=1 make -j2 check TESTS="Rogue_LevelCap"`
Expected: PASS.

- [ ] **Step 5: Build + suíte completa**

Run: `MODERN=1 make -j2 -O all`
Expected: EXIT=0.

Run: `MODERN=1 make -j2 check`
Expected: EXIT=0; `FAILED: 0`.

- [ ] **Step 6: Commit**

```bash
git add include/rogue_controller.h src/rogue_controller.c src/rogue_trainers.c test/rogue_levelcap.c
git commit -m "fix: teto nao estoura o offset quando o proximo chefe e mais fraco"
```

---

## Fim do plano

Resultado: cada chefe do caminho luta com o time fixo da referência (espécie + nível); o teto de nível do jogador vem do chefe da área e bloqueia (EXP e itens de nível não passam do teto). Golpes, habilidade e item por chefe ficam para o plano de detalhes dos chefes.

## Trabalho futuro (registrado)

- **Detalhes dos chefes:** golpes, habilidade e item de cada mon, do `league.json` pelo campo `boss` gravado na Task 1.
- **Identidade fiel do chefe:** treinadores dedicados por chefe do Kaizo (nome/foto/classes), em vez do pool atual.
- **Ordem do caminho:** o spec fixou "na ordem do tracker", que não é a ordem de jogo (ex.: Mt. Chimney/Tabitha lvl 38 logo após Roxanne lvl 15; Wally/Mauville lvl 70). Consequência: o teto sobe/desce de forma não monotônica. Revisar os dados depois.
