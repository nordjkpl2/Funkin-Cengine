#pragma once
#ifndef STAGE_JSON_H
#define STAGE_JSON_H

#include "logic/Stage.h"

/* Le o <stage>.json (Psych). Sempre preenche *out (padroes quando falta campo).
 * Retorna 1 se o arquivo foi lido, 0 se nao existe ou e invalido.
 */
char Stage_LoadJson(const char *path, StageMeta *out);

#endif /* STAGE_JSON_H */
