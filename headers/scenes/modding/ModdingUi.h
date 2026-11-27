/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * ModdingUi.h - kit de interface dos editores de mod (todos menos o Chart Editor): paleta, texto,
 * botoes, campos de texto/numero, listas, abas, status e janelas (abrir caminho, confirmar).
 *
 * Modelo: a logica roda no Update (MUi_X devolve o evento) e o desenho no Draw (MUi_DrawX).
 * Coordenadas do canvas 1280x720 (mouse via RayGame_MouseWorld).
 * Ordem no Update: MUi_FrameBegin -> janela (MUi_ModalUpdate) -> campos/botoes -> atalhos
 * (so com MUi_KeysFree) -> mouse livre (so com MUi_MouseFree).
 * Um campo de texto por vez tem foco: Ctrl+A/C/X/V, Shift+setas seleciona, Home/End, Enter confirma,
 * Esc cancela (volta o texto), clique fora mantem.
 */
#pragma once
#ifndef MODDINGUI_H
#define MODDINGUI_H

#include <stdbool.h>
#include <stddef.h>
#include "raylib.h"

#define MUI_W        1280.0f
#define MUI_H        720.0f
#define MUI_TOP_H    48.0f   /* barra do titulo */
#define MUI_BOTTOM_H 30.0f   /* barra de ajuda/status */

/* Paleta: mesma familia do Chart Editor (violeta escuro + rosa). */
#define MUI_C_BG_TOP  CLITERAL(Color){24, 16, 44, 255}
#define MUI_C_BG_BOT  CLITERAL(Color){8, 6, 18, 255}
#define MUI_C_PANEL   CLITERAL(Color){14, 11, 30, 235}
#define MUI_C_FIELD   CLITERAL(Color){20, 17, 40, 245}
#define MUI_C_BTN     CLITERAL(Color){34, 29, 62, 245}
#define MUI_C_BTN_HOV CLITERAL(Color){60, 50, 104, 255}
#define MUI_C_BAR     CLITERAL(Color){10, 8, 20, 245}
#define MUI_C_EDGE    CLITERAL(Color){120, 100, 200, 140}
#define MUI_C_ACCENT  CLITERAL(Color){255, 92, 176, 255}
#define MUI_C_SEL     CLITERAL(Color){255, 92, 176, 64}
#define MUI_C_HOVER   CLITERAL(Color){255, 255, 255, 18}
#define MUI_C_TEXT    CLITERAL(Color){240, 238, 250, 255}
#define MUI_C_DIM     CLITERAL(Color){165, 158, 200, 255}
#define MUI_C_FAINT   CLITERAL(Color){105, 98, 140, 255}
#define MUI_C_HINT    CLITERAL(Color){255, 214, 120, 255}
#define MUI_C_OK      CLITERAL(Color){120, 230, 150, 255}
#define MUI_C_BAD     CLITERAL(Color){255, 100, 110, 255}

typedef enum { MUI_INFO = 0, MUI_OK, MUI_WARN, MUI_ERR } MUiTone;

typedef enum {
    MUI_BTN_NORMAL = 0,
    MUI_BTN_ACCENT,   /* acao principal (salvar) */
    MUI_BTN_ACTIVE,   /* ligado / aba atual */
    MUI_BTN_DANGER,   /* remover */
    MUI_BTN_OFF       /* desabilitado */
} MUiBtnStyle;

typedef enum {
    MUI_TXT_IDLE = 0,
    MUI_TXT_EDIT,     /* texto mudou neste frame */
    MUI_TXT_SUBMIT,   /* Enter: confirmou e soltou o foco */
    MUI_TXT_CANCEL,   /* Esc: texto voltou ao original e soltou o foco */
    MUI_TXT_BLUR      /* clique fora / Tab: soltou o foco mantendo o texto */
} MUiTextEvent;

#define MUI_TXT_NUMERIC 1u   /* so digitos, sinal e ponto */

typedef enum {
    MUI_MODAL_NONE = 0,  /* janela aberta (ou nenhuma) */
    MUI_MODAL_OK,        /* prompt confirmado / "Salvar" */
    MUI_MODAL_DISCARD,   /* "Descartar" */
    MUI_MODAL_CANCEL
} MUiModalResult;

/* ---------------- ciclo ---------------- */
void MUi_Reset(void);             /* ao criar a cena: sem foco, sem janela, sem status */
void MUi_FrameBegin(void);        /* inicio do Update: libera mouse/teclas consumidos */
void MUi_Tick(float dt);          /* status e cursor piscando */
bool MUi_Busy(void);              /* algo anima (cursor, status, arraste): cena parada deve redesenhar */
bool MUi_KeysFree(void);          /* atalhos podem rodar (sem foco de texto nem janela) */
bool MUi_MouseFree(void);         /* clique ainda nao foi usado por um widget */
void MUi_TakeMouse(void);         /* o editor usou o clique */
bool MUi_Ctrl(void);
bool MUi_Shift(void);
bool MUi_Rep(int key);            /* apertou ou repetiu */
Vector2 MUi_Mouse(void);
bool MUi_Hover(Rectangle r);

/* ---------------- texto ---------------- */
void  MUi_Text(const char *t, float x, float y, float size, Color c);
void  MUi_TextShadow(const char *t, float x, float y, float size, Color c);
float MUi_TextWidth(const char *t, float size);
void  MUi_TextCenter(const char *t, Rectangle r, float size, Color c);
void  MUi_TextRight(const char *t, float right, float y, float size, Color c);
void  MUi_ClipBegin(Rectangle r);   /* recorte (aninhavel, 4 niveis) */
void  MUi_ClipEnd(void);

/* ---------------- molduras ---------------- */
void MUi_DrawBackdrop(void);
void MUi_DrawTopBar(const char *title, const char *detail, bool dirty);
void MUi_DrawBottomBar(void);   /* so status (quando ativo) ou "TAB shortcuts" a direita */

/* ---------------- overlay de atalhos (TAB) ---------------- */
typedef struct { const char *key; const char *what; } MUiHelpRow;
bool MUi_HelpOpen(void);
bool MUi_HelpUpdate(void);   /* Update: TAB abre; TAB/Esc fecha. Devolve true se o editor deve parar aqui */
void MUi_DrawHelp(const char *title, const MUiHelpRow *rows, int count);   /* Draw: por ultimo */
void MUi_DrawPanel(Rectangle r);
void MUi_DrawSection(float x, float y, const char *title);
void MUi_SetStatus(MUiTone tone, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

/* ---------------- botoes ---------------- */
bool MUi_Button(Rectangle r);
void MUi_DrawButton(Rectangle r, const char *label, MUiBtnStyle style);
bool MUi_Check(Rectangle r, bool *value);
void MUi_DrawCheck(Rectangle r, const char *label, bool value);
int  MUi_Tabs(Rectangle r, int count, int current);  /* devolve a aba (clicada ou a atual) */
void MUi_DrawTabs(Rectangle r, const char *const *names, int count, int current);
void MUi_DrawSwatch(Rectangle r, Color c, bool selected);

/* ---------------- campo de texto ---------------- */
MUiTextEvent MUi_TextField(Rectangle r, char *buf, size_t cap);
MUiTextEvent MUi_TextFieldEx(Rectangle r, char *buf, size_t cap, unsigned flags);
void MUi_DrawTextField(Rectangle r, const char *buf, const char *hint);
void MUi_TextFocus(char *buf, size_t cap);   /* foca por codigo, tudo selecionado */
void MUi_TextBlur(void);
bool MUi_TextActive(void);
bool MUi_TextIsFocused(const char *buf);

/* ---------------- numero ---------------- */
/* Arrastar na horizontal ajusta (Shift x10), roda do mouse em cima ajusta 1 passo, clique sem
   arrastar abre digitacao. Casas decimais pelo passo. true = valor mudou. */
bool MUi_Num(Rectangle r, float *value, float step, float lo, float hi);
bool MUi_NumInt(Rectangle r, int *value, int step, int lo, int hi);
void MUi_DrawNum(Rectangle r, float value, float step);
bool MUi_NumBusy(void);   /* arrastando um numero agora */

/* ---------------- lista ---------------- */
typedef struct {
    int sel;
    int top;
    float wheel;
    double lastClick;
} MUiList;

int  MUi_ListRows(Rectangle r, float rowH);
/* Roda do mouse (cursor em cima), clique e barra de rolagem. Devolve a linha clicada ou -1.
   dbl (opcional) = clique duplo na mesma linha. */
int  MUi_List(MUiList *l, Rectangle r, float rowH, int count, bool *dbl);
/* Up/Down/PgUp/PgDn/Home/End. true = selecao mudou. */
bool MUi_ListKeys(MUiList *l, Rectangle r, float rowH, int count);
void MUi_ListShow(MUiList *l, Rectangle r, float rowH, int count);   /* rola ate a selecao */
void MUi_DrawListBegin(Rectangle r);
Rectangle MUi_ListRow(const MUiList *l, Rectangle r, float rowH, int index);
void MUi_DrawListRow(Rectangle row, bool selected);
void MUi_DrawListEnd(const MUiList *l, Rectangle r, float rowH, int count);

/* ---------------- tela de escolha (busca + lista) ---------------- */
#define MUI_PICK_MAX 1024
enum { MUI_PICK_NONE = -1, MUI_PICK_BACK = -2, MUI_PICK_NEW = -3 };

/* col 0 = nome, 1 = detalhe (opcional, NULL), 2 = etiqueta a direita (opcional, NULL). */
typedef const char *(*MUiLabelFn)(int index, int col);

typedef struct {
    MUiList list;
    char search[64];
    int visible[MUI_PICK_MAX];
    int visibleCount;
} MUiPicker;

void MUi_PickerFilter(MUiPicker *p, int count, MUiLabelFn label);   /* depois de mudar a lista */
/* A busca fica sempre com foco. Devolve o indice a abrir (clique ou Enter), MUI_PICK_BACK (Esc com a
   busca vazia), MUI_PICK_NEW (Enter sem resultado com a busca preenchida) ou MUI_PICK_NONE. */
int  MUi_PickerUpdate(MUiPicker *p, Rectangle r, int count, MUiLabelFn label);
void MUi_DrawPicker(const MUiPicker *p, Rectangle r, MUiLabelFn label, const char *hint, const char *empty);

/* ---------------- janelas ---------------- */
void MUi_PromptOpen(int id, const char *title, const char *hint, const char *initial);
void MUi_PromptReopen(const char *error);   /* depois de OK: reabre com o mesmo texto e um erro */
void MUi_ConfirmOpen(int id, const char *title, const char *message);
bool MUi_ModalActive(void);
MUiModalResult MUi_ModalUpdate(int *id);
const char *MUi_PromptText(void);
void MUi_DrawModal(void);

#endif /* MODDINGUI_H */
