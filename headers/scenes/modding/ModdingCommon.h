/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Utilidades compartilhadas pelos editores de mod (Stage Editor, Week Editor). */
#pragma once
#ifndef MODDINGCOMMON_H
#define MODDINGCOMMON_H

#include <stdbool.h>
#include <stddef.h>
#include "yyjson.h"

#define MODED_ROOT_MAX 256
#define MODED_NAME_MAX 64
#define MODED_ROOTS_MAX 64

/* Raizes de gravacao: mods ativos primeiro (ordem da lista), "assets" por ultimo.
   out[0] e o lugar padrao para criar coisas novas. Devolve a quantidade. */
int ModEd_Roots(char out[][MODED_ROOT_MAX], int capacity);

/* Lista os arquivos de <dir> com a extensao <ext> (ex.: ".json"), sem extensao. Sem duplicatas. */
int ModEd_ListStems(const char *dir, const char *ext, char out[][MODED_NAME_MAX], int capacity);

/* Nome seguro para virar arquivo: letras, digitos, '-', '_' e '.' (sem '..'). */
bool ModEd_IsSafeName(const char *name);

/* Digita no buffer: caracteres imprimiveis e backspace. Devolve true se o texto mudou. */
bool ModEd_TypeText(char *buf, size_t capacity);

/* Copia path para path.bak, uma unica vez (nao sobrescreve um backup existente). */
bool ModEd_BackupOnce(const char *path);

/* Escrita em objeto yyjson: remove a chave antes de adicionar (substitui o valor). */
void ModEd_PutReal(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, double value);
void ModEd_PutBool(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, bool value);
void ModEd_PutStr(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, const char *value);
void ModEd_PutRealArr2(yyjson_mut_doc *doc, yyjson_mut_val *obj, const char *key, float x, float y);

#endif /* MODDINGCOMMON_H */
