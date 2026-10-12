#ifndef GUARD_ROGUE_AREAGIFT_H
#define GUARD_ROGUE_AREAGIFT_H

#include "global.h"
#include "rogue.h"

// Oferta de Pokemon por area (Nuzlocke Enxuto).

// Sorteia a especie da area. SPECIES_NONE se a lista for vazia ou o indice invalido.
u16 RogueAreaGift_RollSpecies(u8 areaIdx);
// TRUE se a area ja foi resolvida (o jogador pegou ou adiou).
bool8 RogueAreaGift_IsResolved(u8 areaIdx);
// Marca a area como resolvida (usado quando o jogador pega).
void RogueAreaGift_MarkResolved(u8 areaIdx);
// Indice da area da sala atual do caminho.
u8 RogueAreaGift_CurrentAreaIdx(void);
// Poe a area na fila de adiadas. FALSE se a fila estiver cheia ou a area repetida.
bool8 RogueAreaGift_AddPending(u8 areaIdx);
u8 RogueAreaGift_PendingCount(void);
u8 RogueAreaGift_PendingArea(u8 index);
void RogueAreaGift_RemovePending(u8 index);
void RogueAreaGift_Reset(void);

#endif // GUARD_ROGUE_AREAGIFT_H
