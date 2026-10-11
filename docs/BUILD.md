# Como compilar (build)

Este projeto é um fork do Emerald Rogue (base: `Pokabbie/pokeemerald-rogue`,
branch `expansion`). A ROM se chama `pokeemerald.gba`.

O comando final é:

```bash
MODERN=1 make -j2 -O all
```

## Requisitos

Sistema: Arch Linux. Como root (ou com `sudo`), instale os pacotes:

```bash
pacman -S --noconfirm --needed base-devel git unzip wget libelf libpng \
    arm-none-eabi-binutils arm-none-eabi-gcc arm-none-eabi-newlib
```

Notas:

- Usamos `arm-none-eabi-gcc` (toolchain moderno). Não precisamos do devkitARM.
- `-j2` é o número de tarefas em paralelo. Ajuste para o número de núcleos da
  sua máquina. Com 1 núcleo, use `-j1` e espere bastante tempo.

## Preparar as ferramentas

O repositório precisa de duas coisas baixadas e uma consertada.

### 1. Dependências baixadas

```bash
sh init_deps.sh
```

Isso baixa o `poryscript` (compila os scripts `.pory`) e o `ups` (patcher).

### 2. agbcc

O `scaninc` procura headers no `tools/agbcc/include`. Sem o agbcc, ele não acha
o `string.h` e o build quebra com "No rule to make target 'src/string.h'".

```bash
git clone --depth 1 https://github.com/pret/agbcc /tmp/agbcc
cd /tmp/agbcc && ./build.sh && ./install.sh /root/streamline-nuzlocke && cd -
```

Esperado: existe `tools/agbcc/include/string.h`.

### 3. Conserto do `mapjson` para o GCC 16

O GCC 16 é mais estrito. O `json11` embutido não inclui `<cstdint>` e falha.
Em `tools/mapjson/Makefile`, o `CXXFLAGS` já tem `-include cstdint`. Se você
atualizar o repositório e perder esse ajuste, adicione de volta:

```make
CXXFLAGS := -Wall -std=c++11 -O2 -include cstdint
```

## Compilar

```bash
make tools
MODERN=1 make -j2 -O all
```

Esperado: `pokeemerald.gba` na raiz do repositório (~32 MB).

## Testes

Os testes rodam num emulador sem tela (headless), com o `mgba-rom-test`.

```bash
make check-tools
MODERN=1 make -j2 check
```

Para filtrar por nome de teste:

```bash
MODERN=1 make check TESTS="parte do nome"
```

Esperado hoje: 1315 PASS, 34 falhas conhecidas, 100 a fazer, 1452 no total.
