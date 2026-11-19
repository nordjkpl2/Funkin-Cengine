#pragma once
#ifndef ANIMATIONSETS_XML_H
#define ANIMATIONSETS_XML_H

/* Fachada publica dos animsets. A implementacao foi dividida em:
 *   AnimationSets.c         despacho (AnimationSet_LoadAnimations / FreeAll)
 *   AnimSetSparrow.c        atlas Sparrow XML
 *   AnimSetBinary.c         formato binario .animset
 *   AnimSetCharacterXML.c   XML de personagem Codename
 *   AnimSetParseUtil.c      helpers de parse compartilhados
 */
#include "logic/AnimSetCharacterXML.h"

#endif /* ANIMATIONSETS_XML_H */
