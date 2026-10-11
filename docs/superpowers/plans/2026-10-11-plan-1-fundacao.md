# Plano 1 — Fundação e caminho fixo

> **Para agentes:** este plano usa sub-etapas com caixas (`- [ ]`). Use
> `superpowers:subagent-driven-development` ou `superpowers:executing-plans`.

**Meta:** ter um build que funciona e um caminho fixo e linear, lido de um
arquivo de dados, no lugar do sorteio do Rogue.

**Arquitetura:** separar o "montar o caminho" numa função pura (testável). Os
dados do caminho ficam num JSON editável e viram um header C por um gerador
Python. O ponto de integração chama a função pura no lugar da geração
aleatória.

**Stack:** C (pokeemerald-expansion), `make`, `arm-none-eabi-gcc`, `agbcc`,
`poryscript`, Python 3.

**Desenho:** `docs/superpowers/specs/2026-10-11-nuzlocke-enxuto-design.md`
**Roadmap:** `docs/superpowers/plans/2026-10-11-nuzlocke-enxuto-roadmap.md`

## Restrições globais

- Build de ROM com `MODERN=1 make -j2 -O all`.
- Toolchain: `arm-none-eabi-gcc` (Arch: pacote `arm-none-eabi-gcc`). Não usar
  devkitARM.
- `agbcc` precisa estar instalado em `tools/agbcc` (o `scaninc` procura
  `string.h` lá).
- `poryscript` precisa existir em `tools/poryscript/poryscript-linux/poryscript`
  (rodar `sh init_deps.sh`).
- `tools/mapjson/Makefile` precisa de `-include cstdint` no `CXXFLAGS` (GCC
  16).
- Testes de lógica rodam com `MODERN=1 make check` (usa o emulador headless
  `tools/mgba/mgba-rom-test`). Podem ser filtrados com `TESTS="..."`.

## Foco de revisão

Coisas que o desenho implica mas nenhum teste cobre, em ordem de risco:

1. **Limite de 32 salas** (`ROGUE_ADVPATH_ROOM_CAPACITY`). Um caminho maior
   precisa ser partido em andares. Se o caminho passar de 32 salas num andar, o
   jogo corta em silêncio.
2. **Sala final.** O jogo regenera o caminho quando a sala atual é um chefe.
   Se o caminho não terminar num chefe, a progressão trava.
3. **Coordenadas.** `pathMinY`, `pathMaxY` e `connectionMask` controlam o mapa
   do caminho. Erro aqui gera caminho quebrado ou travado.
4. **`rngSeed` não-determinístico.** Para um caminho fixo, o `rngSeed` das
   salas deve ser fixo, senão decorações variam entre execuções.
5. **`#include` do header gerado.** O arquivo gerado precisa ser incluído por
   um caminho que o compilador ache, e o header gerado precisa existir antes da
   compilação.

---

## Task 1: Build reproduzível

**Arquivos:**
- Modificar: `tools/mapjson/Makefile`
- Criar: `docs/BUILD.md`

**Interfaces:**
- Produz: um `pokeemerald.gba` que compila do zero, e o comando de build
  documentado.

- [ ] **Etapa 1: Instalar pacotes do sistema**

```bash
pacman -S --noconfirm --needed base-devel git unzip wget libelf libpng \
    arm-none-eabi-binutils arm-none-eabi-gcc arm-none-eabi-newlib
```

- [ ] **Etapa 2: Baixar dependências e instalar agbcc**

```bash
sh init_deps.sh
git clone --depth 1 https://github.com/pret/agbcc /tmp/agbcc
cd /tmp/agbcc && ./build.sh && ./install.sh /root/streamline-nuzlocke && cd -
```

Esperado: `/root/streamline-nuzlocke/tools/agbcc/include/string.h` existe.

- [ ] **Etapa 3: Consertar o `mapjson` para o GCC 16**

Em `tools/mapjson/Makefile`, trocar:

```make
CXXFLAGS := -Wall -std=c++11 -O2
```

por:

```make
CXXFLAGS := -Wall -std=c++11 -O2 -include cstdint
```

- [ ] **Etapa 4: Compilar as ferramentas e a ROM**

```bash
make tools
MODERN=1 make -j2 -O all
```

Esperado: existe `pokeemerald.gba` na raiz.

- [ ] **Etapa 5: Documentar o build**

Criar `docs/BUILD.md` com os comandos acima, na ordem, e uma nota: usar
`MODERN=1`, e o motivo do agbcc (scaninc) e do `-include cstdint` (GCC 16).

- [ ] **Etapa 6: Commit**

```bash
git add tools/mapjson/Makefile docs/BUILD.md
git commit -m "build: ambiente reproduzivel (agbcc, poryscript, mapjson gcc16)"
```

---

## Task 2: Núcleo puro do caminho fixo

**Arquivos:**
- Criar: `include/rogue_fixedpath.h`
- Criar: `src/rogue_fixedpath.c`
- Criar: `test/rogue_fixedpath.c`
- Modificar: `Makefile` (só se o header não for achado automaticamente)

**Interfaces:**
- Consome: `struct RogueAdvPath`, `struct RogueAdvPathRoom` (`include/rogue.h`);
  `ADVPATH_ROOM_*`, `ROOM_CONNECTION_MASK_*`, `ROGUE_ADVPATH_ROOM_CAPACITY`
  (`include/constants/rogue.h`).
- Produz:
  - `struct FixedPathStep { u8 roomType; u8 param; u16 extra; };`
  - `void RogueFixed_BuildPath(const struct FixedPathStep *steps, u16 count, struct RogueAdvPath *out);`

- [ ] **Etapa 1: Escrever o teste que falha**

Criar `test/rogue_fixedpath.c`:

```c
#include "global.h"
#include "test/test.h"
#include "rogue.h"
#include "rogue_fixedpath.h"

static const struct FixedPathStep sSteps[] =
{
    // param != extra de propósito: pega o bug da união (ver o teste)
    { ADVPATH_ROOM_ROUTE, 5,  7 },
    { ADVPATH_ROOM_ROUTE, 1,  0 },
    { ADVPATH_ROOM_BOSS,  0,  42 },
};

TEST("RogueFixed_BuildPath makes a straight line")
{
    struct RogueAdvPath path;

    RogueFixed_BuildPath(sSteps, ARRAY_COUNT(sSteps), &path);

    EXPECT_EQ(path.roomCount, 3);
    EXPECT_EQ(path.pathLength, 3);
    EXPECT_EQ(path.pathMinY, 0);
    EXPECT_EQ(path.pathMaxY, 0);

    EXPECT_EQ(path.rooms[0].coords.x, 0);
    EXPECT_EQ(path.rooms[0].coords.y, 0);
    EXPECT_EQ(path.rooms[1].coords.x, 1);
    EXPECT_EQ(path.rooms[2].coords.x, 2);

    EXPECT_EQ(path.rooms[0].roomType, ADVPATH_ROOM_ROUTE);
    EXPECT_EQ(path.rooms[2].roomType, ADVPATH_ROOM_BOSS);

    EXPECT(path.rooms[0].connectionMask & ROOM_CONNECTION_MASK_MID);
    EXPECT(path.rooms[1].connectionMask & ROOM_CONNECTION_MASK_MID);
    EXPECT_EQ(path.rooms[2].connectionMask, 0);

    EXPECT_EQ(path.rooms[0].rngSeed, 0);

    // perType é uma união: só o campo do tipo da sala conta.
    EXPECT_EQ(path.rooms[0].roomParams.perType.route.difficulty, 5);
    EXPECT_EQ(path.rooms[1].roomParams.perType.route.difficulty, 1);
    EXPECT_EQ(path.rooms[2].roomParams.perType.boss.trainerNum, 42);
}
```

- [ ] **Etapa 2: Rodar o teste e ver falhar**

```bash
MODERN=1 make check TESTS="RogueFixed_BuildPath"
```

Esperado: falha de compilação (`rogue_fixedpath.h` não existe) ou link.

- [ ] **Etapa 3: Mover as constantes de conexão e criar o header**

As máscaras de conexão (`ROOM_CONNECTION_MASK_*`) hoje são `#define` locais no
fim de `src/rogue_adventurepaths.c` (linhas 47-56). O nosso arquivo também
precisa delas. Mover para `include/constants/rogue.h`.

Em `src/rogue_adventurepaths.c`, remover este bloco:

```c
#define ROOM_CONNECTION_TOP     0
#define ROOM_CONNECTION_MID     1
#define ROOM_CONNECTION_BOT     2
#define ROOM_CONNECTION_COUNT   3

#define ROOM_CONNECTION_MASK_TOP     (1 << ROOM_CONNECTION_TOP)
#define ROOM_CONNECTION_MASK_MID     (1 << ROOM_CONNECTION_MID)
#define ROOM_CONNECTION_MASK_BOT     (1 << ROOM_CONNECTION_BOT)
```

E adicionar o mesmo bloco em `include/constants/rogue.h`, perto dos outros
`#define ADVPATH_*`.

Depois, criar `include/rogue_fixedpath.h`:

```c
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

void RogueFixed_BuildPath(const struct FixedPathStep *steps, u16 count, struct RogueAdvPath *out);

#endif // GUARD_ROGUE_FIXEDPATH_H
```

- [ ] **Etapa 4: Implementar `RogueFixed_BuildPath`**

Criar `src/rogue_fixedpath.c`. Regra: `count` salas em linha reta no eixo X,
`y = 0`. Cada sala se conecta à próxima pelo meio (`ROOM_CONNECTION_MASK_MID`).
A última não conecta. `rngSeed = 0` (fixo). `activeType`/params vêm de
`steps[i]`.

```c
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
```

- [ ] **Etapa 5: Rodar o teste e ver passar**

```bash
MODERN=1 make check TESTS="RogueFixed_BuildPath"
```

Esperado: PASS.

- [ ] **Etapa 6: Commit**

```bash
git add include/rogue_fixedpath.h src/rogue_fixedpath.c test/rogue_fixedpath.c
git commit -m "feat: funcao pura que monta o caminho fixo"
```

---

## Task 3: Dados do caminho (JSON → header)

**Arquivos:**
- Criar: `src/data/rogue/hack_path.json`
- Criar: `tools/hack/gen_path_header.py`
- Criar: `tools/hack/test_gen_path_header.py`
- Criar (gerado): `src/data/rogue/hack_path.h`

**Interfaces:**
- Consome: `struct FixedPathStep` (Task 2), nomes de tipo de sala.
- Produz: `gHackPathSteps[]` e `HACK_PATH_STEP_COUNT` no header gerado.

- [ ] **Etapa 1: Escrever os dados**

Criar `src/data/rogue/hack_path.json`. Os primeiros andares usam o começo do
Kaizo (Rota 103, Rota 102, ...). Nesta tarefa só precisamos de um caminho
válido e pequeno.

```json
{
  "version": 1,
  "rooms": [
    { "type": "route", "difficulty": 0 },
    { "type": "route", "difficulty": 0 },
    { "type": "boss",  "trainer": 0 }
  ]
}
```

- [ ] **Etapa 2: Escrever o teste do gerador**

Criar `tools/hack/test_gen_path_header.py`. Roda com
`python3 tools/hack/test_gen_path_header.py`. Checa: dado o JSON acima, o
gerador produz um header com 3 entradas, a primeira `ADVPATH_ROOM_ROUTE`, a
última `ADVPATH_ROOM_BOSS`.

- [ ] **Etapa 3: Rodar e ver falhar**

```bash
python3 tools/hack/test_gen_path_header.py
```

Esperado: falha (`gen_path_header.py` não existe).

- [ ] **Etapa 4: Implementar o gerador**

Criar `tools/hack/gen_path_header.py`. Lê o JSON, mapeia `type` para
`ADVPATH_ROOM_ROUTE` / `ADVPATH_ROOM_BOSS`, escreve
`src/data/rogue/hack_path.h`:

```c
// AUTO-GERADO por tools/hack/gen_path_header.py. NAO edite a mao.
#ifndef GUARD_DATA_ROGUE_HACK_PATH_H
#define GUARD_DATA_ROGUE_HACK_PATH_H

#include "rogue_fixedpath.h"

static const struct FixedPathStep gHackPathSteps[] =
{
    { ADVPATH_ROOM_ROUTE, 0, 0 },
    /* ... */
};

#define HACK_PATH_STEP_COUNT ARRAY_COUNT(gHackPathSteps)

#endif // GUARD_DATA_ROGUE_HACK_PATH_H
```

- [ ] **Etapa 5: Rodar o teste e gerar o header**

```bash
python3 tools/hack/test_gen_path_header.py
python3 tools/hack/gen_path_header.py
```

Esperado: PASS, e `src/data/rogue/hack_path.h` criado.

- [ ] **Etapa 6: Commit**

```bash
git add src/data/rogue/hack_path.json tools/hack/gen_path_header.py \
        tools/hack/test_gen_path_header.py src/data/rogue/hack_path.h
git commit -m "feat: dados do caminho e gerador de header"
```

---

## Task 4: Integrar no jogo

**Arquivos:**
- Modificar: `src/rogue_adventurepaths.c` (função
  `RogueAdv_GenerateAdventurePathsIfRequired`, perto da linha 1339)

**Interfaces:**
- Consome: `RogueFixed_BuildPath` (Task 2), `gHackPathSteps` /
  `HACK_PATH_STEP_COUNT` (Task 3).
- Produz: um jogo que usa o caminho fixo.

- [ ] **Etapa 1: Incluir o header gerado**

No topo de `src/rogue_adventurepaths.c`, adicionar:

```c
#include "rogue_fixedpath.h"
#include "data/rogue/hack_path.h"
```

O caminho `"data/rogue/hack_path.h"` é o padrão do repo (ver
`src/rogue_gifts.c`, `src/rogue_hub.c`). Deixar no topo: o header gerado só usa
constantes e `struct FixedPathStep`, então funciona antes da função.

- [ ] **Etapa 2: Trocar a geração aleatória**

Na função `RogueAdv_GenerateAdventurePathsIfRequired`, trocar a chamada
`GeneratePath(&pathSettings);` por uma que monta o caminho fixo:

```c
Rogue_ResetAdventurePathBuffers();
RogueFixed_BuildPath(gHackPathSteps, HACK_PATH_STEP_COUNT, &gRogueAdvPath);
```

Manter o resto da função como está. As funções de geração aleatória
(`GeneratePath` e cia.) ficam sem uso. Isso gera avisos `-Wunused-function`,
mas o build moderno tem `-Wno-error=unused-function`, então não quebra. A
limpeza dessas funções fica para o Plano 5 (enxugar).

- [ ] **Etapa 3: Compilar**

```bash
MODERN=1 make -j2 -O all
```

Esperado: build sem erros, `pokeemerald.gba` gerado.

- [ ] **Etapa 4: Verificar os testes**

```bash
MODERN=1 make check
```

Esperado: PASS (os testes existentes continuam passando).

- [ ] **Etapa 5: Commit**

```bash
git add src/rogue_adventurepaths.c
git commit -m "feat: jogo usa o caminho fixo no lugar do sorteio"
```

**Verificação funcional (manual):** abrir a ROM num emulador, começar uma
aventura e conferir que o caminho é curto, em linha reta, na ordem dos dados.
Isso roda em emulador com tela; fica para o humano ou para um passo futuro de
automação (ver `tools/Pokabbie/AutoSuite`).
