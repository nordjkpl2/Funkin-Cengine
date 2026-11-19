/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * CharacterXML.h - Leitura de personagem Codename em XML.
 */
#pragma once
#ifndef CHARACTER_XML_H
#define CHARACTER_XML_H

#include <stdbool.h>
#include "characters/CharacterInternal.h"

bool Char_FindCharacterXML(const CharCtx *ctx, const char *name,
                           char xmlPath[CHAR_PATH_MAX], char spriteName[128]);
bool Char_FindAtlasXML(const CharCtx *ctx, const char *sprite, const char *name,
                       const char *characterXML, char atlasPath[CHAR_PATH_MAX]);
bool Char_FindCharacterPNG(const CharCtx *ctx, const char *sprite, const char *name,
                           char pngPath[CHAR_PATH_MAX]);
bool Char_TryLoadXMLCharacter(const CharCtx *ctx, Character *character,
                              const char *name, CharRole role);

#endif /* CHARACTER_XML_H */
