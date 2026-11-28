/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * DialogueBox.h - caixa de dialogo da Psych (DialogueBoxPsych): fundo branco, bolha
 * (images/speech_bubble.png/.xml; sem ela desenha uma bolha simples), retratos que deslizam,
 * texto digitado letra a letra com som (sounds/<sound>.ogg, padrao "dialogue") e "dialogueClose".
 *
 * Uma caixa por vez (estado do modulo). Ciclo: Begin -> Start (jogo) ou ShowLine (editor)
 * -> Update/Draw por frame -> End. Coordenadas da tela 1280x720.
 */
#pragma once
#ifndef LOGIC_DIALOGUEBOX_H
#define LOGIC_DIALOGUEBOX_H

#include <stdbool.h>
#include "data/dialogue/Dialogue.h"

/* Carrega a bolha e prepara o estado. modRoot (pode ser NULL): onde procurar imagens e sons primeiro. */
void DialogueBox_Begin(const char *modRoot);

/* Jogo: carrega todos os retratos do arquivo e abre a primeira fala. O arquivo deve viver ate End. */
void DialogueBox_Start(const DialogueFile *file);

/* Editor: mostra esta fala (copia). instant = texto inteiro; senao digita de novo.
   Retratos carregam sob demanda. */
void DialogueBox_ShowLine(const DialogueLine *line, bool instant);

/* accept: termina a digitacao ou passa a fala. skip: encerra tudo (so no jogo).
   true = a caixa terminou de fechar (chamar End). */
bool DialogueBox_Update(float dt, bool accept, bool skip);

void DialogueBox_Draw(void);
void DialogueBox_End(void);

/* Editor: definicao do retrato (carrega se preciso). found = existe o json com esse nome
   (false = usando o "bf" padrao). NULL se nem o padrao existe. */
const DialogueCharDef *DialogueBox_Portrait(const char *name, bool *found);

/* Editor: true = achou images/speech_bubble; false = bolha desenhada. */
bool DialogueBox_HasBubble(void);

/* Editor: a fala atual ja terminou de digitar. */
bool DialogueBox_TextDone(void);

#endif /* LOGIC_DIALOGUEBOX_H */
