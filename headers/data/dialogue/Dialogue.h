/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Dialogue.h - arquivos de dialogo da Psych (DialogueBoxPsych).
 *
 * dialogue.json (na pasta da musica):
 *   {"dialogue": [{"portrait","expression","text","boxState","speed","sound"}, ...]}
 * Retrato: images/dialogue/<portrait>.json
 *   {"image","dialogue_pos" (left/center/right),"scale","position":[x,y],"no_antialiasing",
 *    "animations":[{"anim","loop_name","loop_offsets":[x,y],"idle_name","idle_offsets":[x,y]}]}
 * O atlas do retrato fica em images/dialogue/<image>.png/.xml.
 */
#pragma once
#ifndef DATA_DIALOGUE_H
#define DATA_DIALOGUE_H

#include <stdbool.h>
#include <stddef.h>

#define DIALOGUE_MAX_LINES 256
#define DIALOGUE_NAME_MAX  64
#define DIALOGUE_TEXT_MAX  512
#define DIALOGUE_ANIM_MAX  32
#define DIALOGUE_DEFAULT_SPEED 0.05f
#define DIALOGUE_DEFAULT_CHAR "bf"   /* retrato sem json usa este (Psych) */

typedef struct DialogueLine {
    char portrait[DIALOGUE_NAME_MAX];
    char expression[DIALOGUE_NAME_MAX];
    char boxState[16];               /* "normal" ou "angry" */
    char sound[DIALOGUE_NAME_MAX];   /* vazio = "dialogue" */
    char text[DIALOGUE_TEXT_MAX];    /* '\n' quebra a linha */
    float speed;                     /* segundos por letra */
} DialogueLine;

typedef struct DialogueFile {
    DialogueLine lines[DIALOGUE_MAX_LINES];
    int count;
} DialogueFile;

typedef enum { DIALOGUE_POS_LEFT = 0, DIALOGUE_POS_CENTER, DIALOGUE_POS_RIGHT } DialoguePos;

typedef struct DialogueCharAnim {
    char anim[DIALOGUE_NAME_MAX];      /* nome usado em "expression" */
    char loopName[DIALOGUE_NAME_MAX];  /* prefixo no atlas: falando */
    char idleName[DIALOGUE_NAME_MAX];  /* prefixo no atlas: parado */
    float loopOffset[2];
    float idleOffset[2];
} DialogueCharAnim;

typedef struct DialogueCharDef {
    char image[DIALOGUE_NAME_MAX];
    DialoguePos pos;
    float scale;
    float position[2];
    bool noAntialiasing;
    DialogueCharAnim anims[DIALOGUE_ANIM_MAX];
    int animCount;
} DialogueCharDef;

/* Linha nova do editor (mesma da Psych: bf, "talk", normal, 0.05). */
void Dialogue_DefaultLine(DialogueLine *line);

/* Le dialogue.json. Linhas sem campo ficam com o padrao. false = arquivo ausente ou invalido. */
bool Dialogue_Load(const char *path, DialogueFile *out);

/* Grava dialogue.json (cria a pasta). Mantem chaves de topo desconhecidas. */
bool Dialogue_Save(const char *path, const DialogueFile *in, char *msg, size_t msgCap);

/* Le images/dialogue/<nome>.json. false = ausente/invalido (out fica com o padrao). */
bool DialogueChar_Load(const char *path, DialogueCharDef *out);

/* Procura <relative> no mod, depois em assets/mods e por fim em assets. modRoot pode ser NULL. */
bool Dialogue_FindAsset(const char *modRoot, const char *relative, char *out, size_t cap);

/* Caminho do dialogue.json de uma pasta de musica: <songDir>/dialogue.json ou <mod>/data/<musica>/.
   false = nenhum existe. */
bool Dialogue_FindForSong(const char *songDir, char *out, size_t cap);

#endif /* DATA_DIALOGUE_H */
