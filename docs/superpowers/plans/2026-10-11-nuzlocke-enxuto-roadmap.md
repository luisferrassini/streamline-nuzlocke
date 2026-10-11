# Nuzlocke Enxuto — Roadmap de planos

Data: 2026-10-11
Desenho: `docs/superpowers/specs/2026-10-11-nuzlocke-enxuto-design.md`

O desenho cobre vários subsistemas. Cada plano abaixo produz software que
roda e pode ser verificado sozinho. Ordem sugerida:

1. **Plano 1 — Fundação e caminho fixo.**
   Build que funciona + caminho fixo lido de um arquivo de dados.
   Arquivo: `2026-10-11-plan-1-fundacao.md`.
2. **Plano 2 — Encontros fixos e ganho de Pokémon.**
   Lista de encontros por sala vinda dos dados. Um Pokémon por área, sorteado,
   com opção de adiar.
3. **Plano 3 — Chefes fixos e teto de nível.**
   Times dos chefes vindos dos dados. Teto de nível por chefe. Teto bloqueia.
4. **Plano 4 — Mapa de Itens.**
   Mapa interior com todos os itens, em seções, acesso pelo menu, pegar
   quantas vezes quiser.
5. **Plano 5 — Enxugar o jogo.**
   Remover hub, missões, charms, customização, safari, multiplayer.
6. **Plano 6 — Conteúdo do Kaizo.**
   Stats, tipos e golpes do Kaizo. **Bloqueado** até chegar a ROM/dump.

## Decisões de arquitetura (valem para todos os planos)

Estas decisões saem do código real do Rogue e não estão no desenho. Confira.

- **Andares (floors).** O Rogue gera o caminho em pedaços. Ele gera de novo
  quando a sala atual é um chefe
  (`RogueAdv_GenerateAdventurePathsIfRequired`, `src/rogue_adventurepaths.c`).
  Cada pedaço termina num chefe e sobe a dificuldade.
  - O Kaizo tem ~67 áreas e 37 chefes. Não cabe num só caminho: o limite é
    `ROGUE_ADVPATH_ROOM_CAPACITY` = 32 salas.
  - Decisão: usar o sistema de andares do Rogue. Cada chefe do Kaizo é um
    andar. O jogador não vê a diferença: é um só caminho.
  - Alternativa (depois): subir o limite e fazer um caminho único. Mais
    arriscado.
- **Nível.** O teto hoje vem da dificuldade, por fórmula
  (`CalculateLvlFor`, `src/rogue_trainers.c`). No Kaizo, o teto é o nível do
  chefe. Vamos sobrepor `Rogue_CalculateBossMonLvl()` para devolver o nível do
  chefe atual, vindo dos dados (Plano 3).
- **Encontros.** Hoje são sorteados
  (`RandomiseWildEncounters`, `src/rogue_controller.c`). Vamos trocar por
  leitura da lista fixa da sala (Plano 2).
- **Chefes.** Hoje os times são gerados por
  `src/rogue_trainers.c` a partir de `src/data/rogue/trainers/*.json`.
  Vamos definir times fixos (Plano 3).
- **Dados.** O Rogue já tem um passo de "bake" de JSON para C
  (`tools/Pokabbie/Build/CustomJson`, regras em
  `src/data/rogue/custom_json_rules.mk`). Para o caminho, começamos com um
  gerador Python simples (JSON → header C). Depois avaliamos integrar o
  `customjson`.
