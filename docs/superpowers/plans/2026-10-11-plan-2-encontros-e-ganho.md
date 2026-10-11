# Plano 2 — Encontros fixos e ganho de Pokémon

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Objetivo:** Cada área do caminho usa a lista de encontros do Kaizo. O jogador ganha **um Pokémon por área**, sorteado da lista daquela área, e escolhe **pegar agora ou adiar**.

**Arquitetura:** A lista de cada área vive no JSON do caminho e vira um header C gerado (mesmo pipeline do Plano 1). No jogo, `RandomiseWildEncounters()` deixa de sortear do pool global e passa a ler a lista fixa da área. O "ganho da área" é uma oferta: ao entrar numa rota, o jogo pergunta pegar/adiar; adiar guarda a área numa fila persistente no save, resgatável por uma opção no menu.

**Tech Stack:** C (pokeemerald decomp, `arm-none-eabi`/agbcc), Poryscript (`.pory`), Python 3 (gerador de dados), testes unitários C rodando no `mgba` headless.

**Spec:** `docs/superpowers/specs/2026-10-11-nuzlocke-enxuto-design.md`

## Restrições globais

- Build da ROM: `MODERN=1 make -j2 -O all`. Testes: `MODERN=1 make check`. Filtrar: `TESTS="<prefixo>"`.
- Toolchain: `arm-none-eabi-gcc` (pacote Arch). Não usar devkitARM.
- Dados de caminho: **não** editar o header gerado à mão. Fonte é `src/data/rogue/hack_path.json`.
- `include/rogue.h` e `include/constants/rogue.h` são incluídos por `global.h`: mexer neles dispara rebuild quase total (~30 min em 1 CPU). Planeje um rebuild grande por tarefa que os toca.
- `WILD_ENCOUNTER_GRASS_CAPACITY` = 9 e `WILD_ENCOUNTER_WATER_CAPACITY` = 3 (`include/constants/rogue.h:812-813`).
- `struct RogueAdvPath.rooms[]` **não** é serializado (só `currentRoomParams` e `currentRoomType`, `src/rogue_save.c:253-254`). `struct RogueRunData` (`gRogueRun`) **é** serializado inteiro (`src/rogue_save.c:251`); mudar seu layout invalida saves antigos — aceitável (save novo, decisão do desenho).
- O gerador recusa caminho com mais de `MAX_ROOMS` = 32 salas (`tools/hack/gen_path_header.py`).

## Foco de revisão

O que o desenho implica mas nenhum teste cobre, em ordem de risco:

1. **Lista maior que os slots.** Uma área tem até 63 espécies; o Rogue só guarda 9 de grama + 3 de água. As 9 primeiras vão para a grama, as 3 seguintes para a água; o resto só existe na oferta. Áreas com menos de 12 espécies deixam slots vazios (`SPECIES_NONE`).
2. **Lista vazia.** Uma área sem encontros (ex.: `Starter`, algumas áreas só com água) não pode sortear. A oferta deve ser pulada, não travar.
3. **Time cheio ao ganhar.** O jogador precisa poder soltar um Pokémon para abrir espaço (o Rogue já faz isso nas salas Lab/MiniBoss).
4. **Área já resolvida.** Reentrar numa área não pode oferecer de novo. Guardar o estado por área no save.
5. **Fila de pendentes cheia.** Adiar mais áreas do que a fila aguenta não pode corromper o save nem travar.

---

### Task 1: Listas de encontros nos dados (JSON → header)

**Files:**
- Modify: `src/data/rogue/hack_path.json`
- Modify: `include/rogue_fixedpath.h`
- Modify: `tools/hack/gen_path_header.py`
- Modify: `tools/hack/test_gen_path_header.py`

**Interfaces:**
- Consumes: `struct FixedPathStep` (`include/rogue_fixedpath.h:8`).
- Produces:
  - `struct FixedPathRoomEncounters { const u16 *species; u16 count; };` (em `include/rogue_fixedpath.h`)
  - No header gerado: `static const struct FixedPathRoomEncounters gHackPathEncounters[];` e `#define HACK_PATH_AREA_COUNT ARRAY_COUNT(gHackPathEncounters)`.
  - Índice de `gHackPathEncounters[]` = **índice do passo no JSON (ordem de dados)**, na ordem do arquivo. O gerador NÃO inverte.

- [ ] **Step 1: Escrever o teste que falha (Python)**

Em `tools/hack/test_gen_path_header.py`, adicionar checagens (a função `check(name, ok)` já existe):
- `gHackPathEncounters` tem uma entrada por sala.
- a entrada 0 tem `count` igual ao número de encontros da sala 0 do JSON e `species[0] == SPECIES_<primeiro>`.
- um nome de espécie desconhecido no JSON levanta `ValueError`.
- a ordem é a ordem do JSON (não invertida): a última sala (chefe) tem `count == 0`.

Use como amostra uma área real do Kaizo (Rota 103 começa com `sunkern`, ver `docs/superpowers/specs/...`). Ex.: `check("area0 tem sunkern", "SPECIES_SUNKERN" in header_text)`.

- [ ] **Step 2: Rodar e ver falhar**

Run: `python3 tools/hack/test_gen_path_header.py`
Expected: FAIL (o gerador ainda não emite `gHackPathEncounters`).

- [ ] **Step 3: Estender o JSON**

Em `src/data/rogue/hack_path.json`, cada sala ganha `"name"` e `"encounters"` (lista de nomes de espécie em minúsculas). As duas salas de rota usam listas reais do Kaizo (Rota 103 e Rota 102); o chefe fica com `"encounters": []`.

```json
{
  "version": 1,
  "rooms": [
    { "type": "route", "difficulty": 0, "name": "Route 103",
      "encounters": ["sunkern", "wingull", "ledyba"] },
    { "type": "route", "difficulty": 0, "name": "Route 102",
      "encounters": ["sunkern", "wingull"] },
    { "type": "boss", "trainer": 0, "encounters": [] }
  ]
}
```

- [ ] **Step 4: Estender o gerador**

Em `tools/hack/gen_path_header.py`:
- Ler `include/constants/species.h` (1 vez) e montar `{NOME_UPPER: "SPECIES_<NOME_UPPER>"}` a partir das linhas `#define SPECIES_<NOME>`. Nome desconhecido → `ValueError`.
- Em `parse()`, devolver também, por sala, a lista de espécies (`parse` passa a devolver `(entries, encounter_lists)` ou uma estrutura equivalente).
- Em `render()`, emitir, antes da tabela:
  ```c
  static const u16 sHackPathEncounters0[] = { SPECIES_SUNKERN, SPECIES_WINGULL, ... };
  static const struct FixedPathRoomEncounters gHackPathEncounters[] =
  {
      { sHackPathEncounters0, 3 },
      ...
  };
  #define HACK_PATH_AREA_COUNT ARRAY_COUNT(gHackPathEncounters)
  ```
  Salas sem encontros ficam com ponteiro `NULL` e `count = 0`.

- [ ] **Step 5: Declarar o tipo no header público**

Em `include/rogue_fixedpath.h`, antes de `RogueFixed_BuildPath`:
```c
struct FixedPathRoomEncounters
{
    const u16 *species; // SPECIES_*, ou NULL se vazio
    u16 count;
};
```

- [ ] **Step 6: Rodar e ver passar**

Run: `python3 tools/hack/test_gen_path_header.py`
Expected: `TUDO PASS`.

- [ ] **Step 7: Gerar e conferir o header**

Run: `python3 tools/hack/gen_path_header.py`
Expected: `gerado: .../hack_path.h`; o arquivo contém `gHackPathEncounters`.

- [ ] **Step 8: Commit**

```bash
git add src/data/rogue/hack_path.json include/rogue_fixedpath.h tools/hack/gen_path_header.py tools/hack/test_gen_path_header.py
git commit -m "feat: listas de encontros por area nos dados do caminho"
```

---

### Task 2: Encher os slots de encontro a partir da lista (função pura)

**Files:**
- Modify: `include/rogue_fixedpath.h`
- Modify: `src/rogue_fixedpath.c`
- Test: `test/rogue_fixedpath.c`

**Interfaces:**
- Consumes: `struct FixedPathRoomEncounters` (Task 1).
- Produces:
  ```c
  void RogueFixed_FillEncounterSlots(const struct FixedPathRoomEncounters *enc,
                                     u16 *grass, u8 grassCap,
                                     u16 *water, u8 waterCap);
  ```
  Grama recebe `enc->species[0..min(count,grassCap)-1]`. Água recebe `enc->species[grassCap..min(count,grassCap+waterCap)-1]`. Slots não usados viram `SPECIES_NONE`. `enc` NULL ou `count == 0` deixa tudo `SPECIES_NONE`.

- [ ] **Step 1: Escrever o teste que falha (C)**

Em `test/rogue_fixedpath.c`, novo `TEST("RogueFixed_FillEncounterSlots fills grass then water")`:
```c
static const u16 sList[] = { SPECIES_TREECKO, SPECIES_TORCHIC, SPECIES_MUDKIP, SPECIES_PIKACHU };
struct FixedPathRoomEncounters enc = { sList, 4 };
u16 grass[2]; u16 water[2];

RogueFixed_FillEncounterSlots(&enc, grass, 2, water, 2);
EXPECT_EQ(grass[0], SPECIES_TREECKO);
EXPECT_EQ(grass[1], SPECIES_TORCHIC);
EXPECT_EQ(water[0], SPECIES_MUDKIP);
EXPECT_EQ(water[1], SPECIES_PIKACHU);

struct FixedPathRoomEncounters curto = { sList, 1 };
RogueFixed_FillEncounterSlots(&curto, grass, 2, water, 2);
EXPECT_EQ(grass[0], SPECIES_TREECKO);
EXPECT_EQ(grass[1], SPECIES_NONE);
EXPECT_EQ(water[0], SPECIES_NONE);
EXPECT_EQ(water[1], SPECIES_NONE);

RogueFixed_FillEncounterSlots(NULL, grass, 2, water, 2);
EXPECT_EQ(grass[0], SPECIES_NONE);
EXPECT_EQ(water[0], SPECIES_NONE);
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: FAIL (`RogueFixed_FillEncounterSlots` não existe).

- [ ] **Step 3: Implementar**

Em `src/rogue_fixedpath.c`, implementar `RogueFixed_FillEncounterSlots` conforme a interface. Declarar em `include/rogue_fixedpath.h`.

- [ ] **Step 4: Rodar e ver passar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/rogue_fixedpath.h src/rogue_fixedpath.c test/rogue_fixedpath.c
git commit -m "feat: funcao pura que enche os slots de encontro"
```

---

### Task 3: Usar a lista fixa nos encontros selvagens

**Files:**
- Modify: `include/rogue.h` (adicionar campo em `struct RogueAdvPathRoom`, `:116-123`)
- Modify: `src/rogue_fixedpath.c` (preencher o campo no builder)
- Modify: `src/rogue_controller.c` (substituir o corpo de `RandomiseWildEncounters`, `:9640-9684`)

**Interfaces:**
- Consumes: `RogueFixed_FillEncounterSlots` (Task 2); `gHackPathEncounters`, `HACK_PATH_AREA_COUNT` (Task 1).
- Produces: `u8 areaIdx;` em `struct RogueAdvPathRoom` = índice da área (passo de dados) daquela sala. `RandomiseWildEncounters(void)` mantém a assinatura e passa a preencher `gRogueRun.wildEncounters.species[]` a partir de `gHackPathEncounters[areaIdx]`.

> Nota: o builder guarda `steps[count-1-i]` em `rooms[i]` (ordem de jogo invertida — ver Plano 1). Portanto `rooms[i].areaIdx = count - 1 - i`.

- [ ] **Step 1: Escrever o teste que falha (C)**

Em `test/rogue_fixedpath.c`, estender o teste existente do builder para checar a área:
```c
EXPECT_EQ(path.rooms[2].areaIdx, 0); // rooms[2] guarda steps[0] (1a area)
EXPECT_EQ(path.rooms[1].areaIdx, 1);
EXPECT_EQ(path.rooms[0].areaIdx, 2); // rooms[0] guarda steps[2] (chefe)
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: FAIL (`areaIdx` não existe).

- [ ] **Step 3: Adicionar o campo e preencher**

Em `include/rogue.h`, em `struct RogueAdvPathRoom` (após `u8 connectionMask;`):
```c
u8 areaIdx; // indice da area (lista de encontros) no caminho fixo
```
Em `src/rogue_fixedpath.c`, dentro do laço: `out->rooms[i].areaIdx = count - 1 - i;`

- [ ] **Step 4: Rodar e ver passar o teste do builder**

Run: `MODERN=1 make -j2 check TESTS="RogueFixed_"`
Expected: PASS.

- [ ] **Step 5: Trocar o sorteio pela lista fixa**

Em `src/rogue_controller.c`:
- Incluir `#include "data/rogue/hack_path.h"`.
- Reescrever `RandomiseWildEncounters()` (`:9640-9684`) para:
  ```c
  static void RandomiseWildEncounters(void)
  {
      struct RogueAdvPathRoom *room = &gRogueAdvPath.rooms[gRogueRun.adventureRoomId];
      const struct FixedPathRoomEncounters *enc = NULL;

      if (room->areaIdx < HACK_PATH_AREA_COUNT)
          enc = &gHackPathEncounters[room->areaIdx];

      RogueFixed_FillEncounterSlots(enc,
          &gRogueRun.wildEncounters.species[0], WILD_ENCOUNTER_GRASS_CAPACITY,
          &gRogueRun.wildEncounters.species[WILD_ENCOUNTER_GRASS_CAPACITY], WILD_ENCOUNTER_WATER_CAPACITY);
      memset(&gRogueRun.wildEncounters.catchCounts, 0, sizeof(gRogueRun.wildEncounters.catchCounts));
  }
  ```
- As funções `BeginWildEncounterQuery`, `EndWildEncounterQuery`, `RandomiseWildEncounters_CalculateWeight` e `RandomiseWildEncounters_CalculateInitialWeight` ficam sem uso. **Deixar no lugar** (avisos de não-usado não quebram: `UNUSED_ERROR ?= 0`). Limpeza fica para o Plano 5.

- [ ] **Step 6: Build + suíte completa**

Run: `MODERN=1 make -j2 -O all`
Expected: EXIT=0, `pokeemerald.gba` (32 MB).

Run: `MODERN=1 make -j2 check`
Expected: EXIT=0; `FAILED: 0`.

- [ ] **Step 7: Commit**

```bash
git add include/rogue.h src/rogue_fixedpath.c src/rogue_controller.c test/rogue_fixedpath.c
git commit -m "feat: encontros selvagens vem da lista fixa da area"
```

---

### Task 4: Núcleo da oferta da área (sorteio + fila de pendentes)

**Files:**
- Modify: `include/constants/rogue.h` (constantes)
- Modify: `include/rogue.h` (campos em `struct RogueRunData`)
- Create: `include/rogue_areagift.h`
- Create: `src/rogue_areagift.c`
- Test: `test/rogue_areagift.c`

**Interfaces:**
- Consumes: `gHackPathEncounters`, `HACK_PATH_AREA_COUNT` (Task 1).
- Produces (em `include/rogue_areagift.h`):
  ```c
  u16 RogueAreaGift_RollSpecies(u8 areaIdx);     // SPECIES_NONE se a lista for vazia
  bool8 RogueAreaGift_IsResolved(u8 areaIdx);
  bool8 RogueAreaGift_AddPending(u8 areaIdx);    // FALSE se cheio ou duplicado
  u8 RogueAreaGift_PendingCount(void);
  u8 RogueAreaGift_PendingArea(u8 index);
  void RogueAreaGift_RemovePending(u8 index);
  void RogueAreaGift_Reset(void);
  ```
- Campos novos em `struct RogueRunData` (`include/rogue.h`):
  ```c
  u8 giftResolved[GIFT_RESOLVED_BYTES];
  u8 giftPending[GIFT_PENDING_CAPACITY];
  u8 giftPendingCount;
  ```
- Constantes em `include/constants/rogue.h`: `GIFT_PENDING_CAPACITY 16`, `GIFT_RESOLVED_BYTES 8` (cobre 64 áreas).

- [ ] **Step 1: Escrever o teste que falha (C)**

Criar `test/rogue_areagift.c`:
```c
#include "global.h"
#include "test/test.h"
#include "rogue.h"
#include "rogue_areagift.h"
#include "data/rogue/hack_path.h"

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
    EXPECT_EQ(RogueAreaGift_RollSpecies(1), SPECIES_NONE); // chefe: lista vazia
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `MODERN=1 make -j2 check TESTS="RogueAreaGift"`
Expected: FAIL (arquivo/funções não existem).

- [ ] **Step 3: Implementar**

- Constantes em `include/constants/rogue.h` (perto de `WILD_ENCOUNTER_*`, `:812`).
- Campos em `struct RogueRunData` (`include/rogue.h:271+`).
- `src/rogue_areagift.c` inclui `data/rogue/hack_path.h`; `RogueAreaGift_RollSpecies(areaIdx)` usa `Random() % count` sobre `gHackPathEncounters[areaIdx]`, devolvendo `SPECIES_NONE` se `areaIdx >= HACK_PATH_AREA_COUNT` ou `count == 0`.
- `RogueAreaGift_Reset` zera as três estruturas.
- O bit `areaIdx` de `giftResolved` marca resolvida (pega ou adiada).

- [ ] **Step 4: Rodar e ver passar**

Run: `MODERN=1 make -j2 check TESTS="RogueAreaGift"`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add include/constants/rogue.h include/rogue.h include/rogue_areagift.h src/rogue_areagift.c test/rogue_areagift.c
git commit -m "feat: nucleo da oferta de pokemon da area"
```

---

### Task 5: Prompt "pegar agora ou adiar" na sala

**Files:**
- Modify: `src/rogue_controller.c` (dispatch em `Rogue_OnProcessPlayerFieldInput`, `:3488`; marcar ao entrar na área)
- Modify: `src/rogue_script.c` (specials C)
- Modify: `data/specials.inc` (declarações)
- Modify: `data/scripts/Rogue/Rogue_Manager.pory` (script)

**Interfaces:**
- Consumes: `RogueAreaGift_*` (Task 4).
- Produces:
  - `void Rogue_AreaGiftTake(void)` — special: sorteia da área atual, cria o Pokémon no nível do cap, entrega; resultado em `gSpecialVar_Result`.
  - `void Rogue_AreaGiftDelay(void)` — special: adiciona a área atual aos pendentes e marca resolvida.
  - Script global `Rogue_EventScript_AreaGiftPrompt`.
  - Flag RAM `gRogueLocal.areaGiftPromptPending` (não serializada).

- [ ] **Step 1: Marcar a oferta ao entrar na rota**

Em `Rogue_OnWarpIntoMap` (`src/rogue_controller.c:5625`), no ramo de rota (perto de `:5834`): se `currentRoomType == ADVPATH_ROOM_ROUTE`, a área tem encontros (`gHackPathEncounters[areaIdx].count > 0`) e não está resolvida → `areaGiftPromptPending = TRUE`. Caso contrário, `FALSE`.

- [ ] **Step 2: Disparar o script no input**

Em `Rogue_OnProcessPlayerFieldInput` (`src/rogue_controller.c:3488`), no topo: se `areaGiftPromptPending` e nenhum script ativo → `areaGiftPromptPending = FALSE; ScriptContext_SetupScript(Rogue_EventScript_AreaGiftPrompt); return TRUE;` (mesmo padrão de `Rogue_EventScript_AttemptSnagBattle`, `:3560`).

- [ ] **Step 3: Escrever os specials**

Em `src/rogue_script.c`: `Rogue_AreaGiftTake` chama uma nova função em `rogue_areagift.c`/`rogue_controller.c` que cria o mon. Nível: `Rogue_CalculatePlayerMonLvl()` (`src/rogue_trainers.c:384`). Criar com `CreateMon(&mon, species, level, USE_RANDOM_IVS, 0, 0, OT_ID_PLAYER_ID, 0)` e `GiveMonToPlayer(&mon)` (`src/pokemon.c:3070`); devolver `MON_GIVEN_TO_PARTY`/etc. em `gSpecialVar_Result`. `Rogue_AreaGiftDelay` chama `RogueAreaGift_AddPending(areaAtual())`.
Declarar `def_special Rogue_AreaGiftTake` e `def_special Rogue_AreaGiftDelay` em `data/specials.inc`.

- [ ] **Step 4: Escrever o script**

Em `data/scripts/Rogue/Rogue_Manager.pory`, adicionar `Rogue_EventScript_AreaGiftPrompt`:
```pory
script Rogue_EventScript_AreaGiftPrompt
{
    lockall
    msgbox("This area has wild Pokemon. Take this area's Pokemon now, or leave it for later?", MSGBOX_YESNO)
    if (var(VAR_RESULT) == YES)
    {
        specialvar(VAR_TEMP_0, CalculatePlayerPartyCount)
        specialvar(VAR_TEMP_1, Rogue_CalcMaxPartySize)
        if (var(VAR_TEMP_0) >= VAR_TEMP_1)
        {
            msgbox("You must release a Pokemon to make room.")
            special(ChoosePartyMon)
            waitstate
            if (var(VAR_0x8004) == PARTY_NOTHING_CHOSEN)
            {
                releaseall
                end
            }
            special(Rogue_ReleaseMonInSlot)
        }
        special(Rogue_AreaGiftTake)
        playfanfare(MUS_OBTAIN_ITEM)
        message("You obtained the area's Pokemon!")
        waitfanfare
        waitmessage
        closemessage
    }
    else
    {
        special(Rogue_AreaGiftDelay)
        msgbox("Left for later. You can take it from the menu.")
    }
    releaseall
    end
}
```
(Espelha o fluxo de time cheio de `data/maps/Rogue_Encounter_Lab/scripts.pory:42-60`.)

- [ ] **Step 5: Build + suíte**

Run: `MODERN=1 make -j2 -O all`
Expected: EXIT=0.

Run: `MODERN=1 make -j2 check`
Expected: EXIT=0; `FAILED: 0`.

- [ ] **Step 6: Commit**

```bash
git add src/rogue_controller.c src/rogue_script.c data/specials.inc data/scripts/Rogue/Rogue_Manager.pory
git commit -m "feat: prompt pegar ou adiar a oferta da area"
```

---

### Task 6: Resgatar oferta pendente pelo menu

**Files:**
- Modify: `src/start_menu.c` (ação + callback + entrada no menu)
- Modify: `src/strings.c` e `include/strings.h` (rótulo)
- Modify: `src/rogue_script.c` e `data/specials.inc` (specials de leitura)
- Modify: `data/scripts/Rogue/Rogue_Manager.pory` (script de resgate)

**Interfaces:**
- Consumes: `RogueAreaGift_*` (Task 4).
- Produces:
  - Rótulo `gText_MenuPendingGift`.
  - Script global `Rogue_EventScript_ClaimPendingGift`.
  - Specials: `Rogue_PendingGiftCount` (→ `gSpecialVar_Result`), `Rogue_PendingGiftAreaAt` (índice em `gSpecialVar_0x8001` → área em `gSpecialVar_Result`), `Rogue_PendingGiftClaim` (pega a área indicada).

- [ ] **Step 1: Ampliar a lista do menu**

Em `src/start_menu.c:103`, `sCurrentStartMenuActions[9]` pode estourar com a nova opção no modo run. Aumentar para `[10]`.

- [ ] **Step 2: Adicionar rótulo**

Em `include/strings.h` (perto de `:281`) e `src/strings.c` (perto de `:1779`): `const u8 gText_MenuPendingGift[] = _("Pending");`.

- [ ] **Step 3: Adicionar ação, callback e entrada**

Em `src/start_menu.c`:
- `MENU_ACTION_PENDING_GIFT` no enum (`:63-83`).
- `{gText_MenuPendingGift, {.u8_void = StartMenuPendingGiftCallback}}` em `sStartMenuItems[]` (`:218-238`).
- `StartMenuPendingGiftCallback` espelha `StartMenuDecorateCallback` (`:430`): se `RogueAreaGift_PendingCount() > 0`, prepara o script `Rogue_EventScript_ClaimPendingGift`, senão mostra "nada pendente".
- `AddStartMenuAction(MENU_ACTION_PENDING_GIFT);` em `BuildRogueRunStartMenu` (`:407-453`).

- [ ] **Step 4: Escrever os specials de leitura**

Em `src/rogue_script.c`: `Rogue_PendingGiftCount`, `Rogue_PendingGiftAreaAt`, `Rogue_PendingGiftClaim`. Declarar em `data/specials.inc`.

- [ ] **Step 5: Escrever o script de resgate**

Em `data/scripts/Rogue/Rogue_Manager.pory`, `Rogue_EventScript_ClaimPendingGift` monta um `dynamicmultichoice` listando as áreas pendentes, e ao escolher uma faz o mesmo fluxo de time cheio + `special(Rogue_PendingGiftClaim)`. Ver o padrão de `data/maps/Rogue_Area_SafariZone/scripts.pory:9-33`.

- [ ] **Step 6: Build + suíte**

Run: `MODERN=1 make -j2 -O all`
Expected: EXIT=0.

Run: `MODERN=1 make -j2 check`
Expected: EXIT=0; `FAILED: 0`.

- [ ] **Step 7: Commit**

```bash
git add src/start_menu.c src/strings.c include/strings.h src/rogue_script.c data/specials.inc data/scripts/Rogue/Rogue_Manager.pory
git commit -m "feat: resgatar oferta pendente pelo menu"
```

---

## Fim do plano

Resultado: as áreas usam as listas de encontros dos dados; o jogador ganha um Pokémon por área, sorteado da lista, com opção de pegar agora ou adiar; ofertas adiadas podem ser resgatadas depois pelo menu.
