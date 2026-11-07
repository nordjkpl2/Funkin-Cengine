#pragma once
#ifndef MODLIST_H
#define MODLIST_H

#include <stdbool.h>
#include <stddef.h>

/* Lista dos mods em assets/mods (uma pasta = um mod; o layout direto vale 1 mod "(direto)").
 * Mods desativados ficam em gameDisabledMods (GameData, salvo no save.data).
 * A varredura e preguicosa (1a chamada) e so lista pastas, nao entra nelas. */

#define MOD_LIST_MAX 256

void        ModList_Rescan(void);
int         ModList_Count(void);
const char* ModList_Name(int index);
const char* ModList_Root(int index);          /* ex.: "assets/mods/MeuMod" */
bool        ModList_Enabled(int index);
void        ModList_SetEnabled(int index, bool enabled);   /* salva no save.data */

/* Raiz do 1o mod ATIVO que tem o personagem (animset, xml ou json). false = nenhum. */
bool        ModList_FindCharacterRoot(const char* name, char* out, size_t capacity);

#endif
