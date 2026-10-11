# Nuzlocke Enxuto (nome provisório) — Desenho

Data: 2026-10-11
Status: rascunho para revisão

## 1. Intenção (o porquê)

Nuzlocke é divertido, mas duas coisas atrapalham:

1. Subir de nível na mão (grind).
2. Ter que explorar o jogo inteiro para saber as opções. Quais áreas existem,
   quais Pokémon dá para capturar em cada área, e quando vale a pena esperar
   para capturar.

A ideia é um jogo que **é** o Nuzlocke. Um só caminho. Itens à mão. Sem
história. Sem grind. Com teto de nível. As áreas já mostram os encontros e os
chefes do Nuzlocke real.

Referência de conteúdo (fase 1): **Emerald Kaizo**.

## 2. O que é

Um romhack em cima do **Emerald Rogue** (branch `expansion`, a versão EX). O
jogo é reduzido a:

- um caminho fixo de salas, uma atrás da outra;
- cada sala = uma área da referência (ex.: Rota 103, Petalburg Woods,
  Rustboro Gym);
- encontros selvagens iguais aos da referência naquela área;
- **um Pokémon por área**, sorteado da lista daquela área. O jogador escolhe
  pegar agora ou deixar para depois;
- chefes na posição certa, com o time da referência;
- um teto de nível por chefe;
- as mudanças do Kaizo nos próprios Pokémon (stats, tipos, golpes) e nos
  chefes;
- um **Mapa de Itens** acessível a qualquer momento.

## 3. O caminho

- Uma sequência fixa. Sem sorteio. Sem ramificação.
- Começa com a cópia fiel do Emerald Kaizo: mesma ordem de áreas, mesmos
  encontros, mesmos chefes.
- O caminho é definido por **um arquivo de dados**, não pelo código. Assim, o
  Tipo 2 (caminho do usuário) vira só edição de dados.

## 4. Referência de dados (fase 1)

Fonte: os dados do app `Bassel-T/nuzlocke.app` (arquivos em
`src/lib/data/`).

- `routes.json` → chave `emkaizo`: 67 entradas de rota (nome + lista de
  encontros) e 37 entradas de chefe (`type: gym`, com `value`, `group`,
  `boss`).
- `league.json` → chave `emkaizo`: 40 chefes com time completo (espécie,
  nível, golpes, habilidade, item).
- `games.json` → `emkaizo`, com `physicalSpecialSplit: true`.
- `patches.json` → chave `emkaizo`, seção `move`: as mudanças de golpes do
  Kaizo.

### 4.1 As mudanças do Kaizo nos Pokémon

O Kaizo não muda só rotas e chefes. Ele muda os Pokémon: stats, tipos e golpes
diferentes do jogo normal. Os chefes também jogam melhor.

O jogador quer essas mudanças (resposta 1 do questionário).

Problema: os dados do tracker têm **só as mudanças de golpes**. As mudanças de
stats e tipos **não** estão lá. Elas só vêm da ROM do Kaizo, ou de um dump
dela.

Então esta parte depende da fonte que o jogador vai conseguir depois. Até lá,
não dá para portar as mudanças de Pokémon.

Lacunas conhecidas dos dados do tracker (nesta fase):

- mudanças de stats e tipos dos Pokémon (só vêm da ROM/dump do Kaizo);
- itens por área (resolvido pelo Mapa de Itens, ver seção 5);
- treinadores comuns (nem quais são obrigatórios);
- recompensa de treinador opcional.

## 5. O Mapa de Itens

- Um mapa separado só de itens no chão.
- Tem **todos** os itens do jogo, separados em seções (ex.: remédios, Poké
  Bolas, TMs, held items, pedras de evolução, berries).
- Os itens são **infinitos**: o jogador pega quantos quiser, quantas vezes
  quiser.
- O jogador pode acessar esse mapa a qualquer momento.

Consequência assumida: a escassez de itens deixa de existir. Isso é
intencional (reforça "sem grind").

## 6. Regras de jogo

- Pokémon que desmaia morre.
- Se o time todo morre, volta para o começo do caminho.
- Sem grind: o teto de nível vem do nível do chefe na referência.
- O teto de nível **bloqueia**. O Pokémon não passa do teto. Rare Candy
  infinito só leva o Pokémon até o teto — nunca além.
- Sem lojinhas.

## 7. O que fica de fora (fase 1)

- Editor de caminho (Tipo 2). O formato de dados precisa permitir, mas nenhuma
  interface entra agora.
- Hub, missões, charms, customização do personagem, safari, multiplayer.
- Treinadores comuns e suas recompensas.
- Múltiplos jogos de referência (só Kaizo agora).

## 8. Metas, não-metas e anti-metas

Metas:

- Caminho fixo e previsível.
- Zero grind.
- Encontros e chefes fiéis à referência.
- Mapa de Itens completo e sempre acessível.
- Caminho definido por dados.

Anti-metas (seria fracasso mesmo "funcionando"):

- Ainda precisa grindar.
- A sequência continua aleatória.
- Os encontros não batem com a referência.
- O jogador precisa explorar para descobrir as opções.
- O teto de nível não impede passar do nível.

## 9. Decisões (confirmadas e minhas)

Confirmadas pelo jogador:

- **Mudanças do Kaizo:** sim. Portar stats, tipos, golpes e os chefes. Depende
  da fonte do Kaizo (ver 4.1).
- **Como ganhar Pokémon:** um por área, sorteado da lista da área. O jogador
  escolhe pegar ou adiar.
- **Teto de nível:** bloqueia. Rare Candy só até o teto.
- **Nível dos selvagens:** pouco abaixo do teto do próximo chefe.

Decisões minhas (confira):

- **Base:** branch `expansion` (EX) do `Pokabbie/pokeemerald-rogue`.
- **Starter:** os três iniciais de Hoenn.
- **Caminho:** inclui as 67 áreas do Kaizo, na ordem do tracker.
- **Salas:** usam os mapas do próprio Rogue (campo, caverna, floresta,
  montanha, cidade, água), mapeados por uma regra + override por entrada.
- **Itens:** um único Mapa de Itens, todos os itens, acessível por opção no
  menu.
- **Nível:** um teto por chefe.
- **Save:** save novo. Save antigo do Rogue não precisa funcionar.
- **Fim:** o caminho termina no Campeão.
- **Duplas:** as lutas duplas continuam duplas.

## 10. Como fazer (visão técnica)

Levantamento feito no repositório `Pokabbie/pokeemerald-rogue`, branch
`expansion`.

**Build**

- Clonar o repositório e instalar a toolchain do pokeemerald (devkitARM/agbcc).
- O decompila compila a ROM a partir do fonte. A ROM original não é necessária
  para buildar, só para o `make compare` (verificação).

**Caminho fixo**

- `src/rogue_adventurepaths.c` gera o caminho por sorteio (pesos, comprimento,
  coords). Trocar a geração por um carregador que lê a tabela fixa e preenche
  `gRogueAdvPath`.
- Estruturas envolvidas (em `include/rogue.h`):
  `struct RogueAdvPath` (rooms, connectionMask, pathLength, pathMinY/MaxY),
  `struct RogueAdvPathRoom` (coords, roomType, roomParams),
  `struct RogueAdvPathGenerator` (minLength, maxLength, roomWeights).
- Tipos de sala: rota, covil selvagem, boss, miniboss, entre outros.

**Encontros selvagens**

- Em `src/rogue_controller.c`: `RandomiseWildEncounters()`,
  `GetWildGrassEncounter(index)`, `GetWildWaterEncounter`, contagens e
  `encounterPreview`. Trocar o sorteio por leitura da lista fixa da sala.

**Como ganhar o Pokémon da área**

- Em cada área, o jogo dá um Pokémon, sorteado da lista da área.
- O jogador escolhe pegar agora ou adiar.
- O Rogue já tem os pontos de escolha de Pokémon/entrada de time; usar esse
  caminho e trocar a fonte pela lista fixa da área.

**Conteúdo do Kaizo (stats, tipos, golpes)**

- Depende da fonte do Kaizo (ROM ou dump). Sem ela, não começa.
- Quando a fonte chegar: importar stats, tipos e golpes. Os golpes já têm uma
  parte em `patches.json`.
- IA dos chefes: replicar o máximo possível. Meta realista, não garantia.

**Chefes**

- Os times são gerados por `src/rogue_trainers.c` a partir de
  `src/data/rogue/trainers/*.json` (formato com `team_generator`, `subsets` e
  query scripts). Para a fase 1, definir times fixos para os chefes (ou forçar
  as espécies via query script).

**Teto de nível**

- `struct RogueAdventurePhase` (em `include/rogue.h`) tem `levelCap` e
  `levelStep`. Recalcular a partir dos níveis dos chefes da referência.

**Mapa de Itens**

- Criar um mapa interior dedicado (base: um layout de mart do próprio Rogue).
- Itens no chão, separados em seções, com scripts que entregam o item e podem
  ser repetidos.
- Adicionar a opção de acesso ao mapa no menu.

**Remover sistemas**

- Hub, missões (`src/data/rogue/quests/`), charms, customização, safari,
  multiplayer: remover os pontos de entrada.

**Dados e bake**

- Criar o arquivo de caminho (formato a definir) e um passo de bake para
  embutir na ROM. O Rogue já tem um passo de "bake" (`rogue_baked`).

## 11. O que fica para o construtor

- Localizar os pontos exatos de atribuição de encontros, treinadores e teto de
  nível.
- Definir a regra de mapeamento área→layout, e os overrides por entrada.
- Definir o formato do arquivo de caminho.
- Definir o formato do arquivo de conteúdo do Kaizo (stats, tipos, golpes).
- Resolver os casos especiais de presentes/starter.
- Montar o ambiente de build e o ciclo de teste na VPS.

## 12. Em aberto

- **Fonte do Kaizo.** As mudanças de stats e tipos dos Pokémon precisam da ROM
  do Kaizo, ou de um dump dela. O jogador vai conseguir essa fonte depois.
  Esta parte do trabalho espera por ela.
- Confirmar a regra de ganho: um Pokémon por área, espécie sorteada, com opção
  de adiar.
- Cauda longa de casos especiais: presentes, trocas dentro do jogo, e áreas
  sem encontro selvagem.
