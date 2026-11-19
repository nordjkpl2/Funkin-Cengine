/*
 * Friday Night Funkin' C Engine
 * (c) 2026 nordjkpl
 *
 * AnimSetParseUtil.h - Declaracoes dos helpers de parse de animset.
 */

#pragma once
#ifndef ANIMSET_PARSE_UTIL_H
#define ANIMSET_PARSE_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "Render.h"
#include "yxml.h"

#define ANIMSET_MAX_ANIMATIONS 8192
#define XML_MAX_FRAMES         262144u
#define XML_MAX_ANIM_NAME      31u
#define XML_MAX_PREFIX         31u
#define XML_MAX_FRAME_NAME     127u
#define XML_MAX_XML_DEPTH      64u
#define XML_STACK_SIZE         4096u
#define XML_READ_CHUNK         16384u
#define XML_ATTR_VALUE_SIZE    512u

typedef bool (*YxmlTokenHandler)(yxml_t *xml, yxml_ret_t token, void *user);

bool AnimParse_YxmlFile(const char *path, YxmlTokenHandler handler, void *user);
bool AnimParse_Int32(const char *text, int32_t *out);
bool AnimParse_Float(const char *text, float *out);
bool AnimParse_Bool(const char *text, bool *out);
bool AnimParse_Color(const char *text, Color *out);
bool AnimParse_AppendAttr(char *buffer, size_t capacity, size_t *length, const char *data);
bool AnimParse_EndsWithXML(const char *path);
bool AnimParse_IsLikelyXML(const char *path);
void AnimationSet_FreeArrays(RayAnimationHandler *animations);

#endif /* ANIMSET_PARSE_UTIL_H */
