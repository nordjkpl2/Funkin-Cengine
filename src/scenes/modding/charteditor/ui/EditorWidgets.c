/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: caixas de texto/numero, botoes, listas e bloqueio do mouse. */
#include "scenes/api/FontLoader.h"
#include"data/song/Song.h"
#include "core/ModPath.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "scenes/modding/charteditor/ui/EditorWidgets.h"
#include "rendering/TextMeasure.h"
#include <math.h>
#include "scenes/modding/charteditor/io/EditorDataAndIO.h"
#include "scenes/modding/charteditor/grid/EditorTimeline.h"

TxtBox* focusBox = NULL;

char EdHover(Rectangle r) {
    return CheckCollisionPointRec(EdMouseWorld(), r);
}

static char helpOpen = 0;

char EdHelpOpen(void) {
    return helpOpen;
}

void EdHelpToggle(void) {
    helpOpen = !helpOpen;
}

char EdUiBlocked(void) {
    return ceDlg != DLG_NONE || activeDrop || menuOpen || helpOpen;
}

/* Cantos com raio fixo (~5 px) em qualquer tamanho: o roundness do raylib e relativo ao lado menor. */
void Ed_DrawBox(Rectangle r, Color fill, Color edge) {
    const float side = fminf(r.width, r.height);
    const float round = (side > 10.0f) ? 10.0f / side : 1.0f;
    DrawRectangleRounded(r, round, 6, fill);
    DrawRectangleRoundedLinesEx(r, round, 6, 1.2f, edge);
}

/* Texto de caixa (ou a dica apagada) + cursor piscando quando tem foco. So a caixa com foco mede texto. */
static void Field_DrawText(Rectangle r, const char* buf, const char* hint, char focused) {
    const char empty = (buf[0] == 0);
    const Vector2 p = {r.x + 8, r.y + (r.height - 20) * 0.5f};
    Ed_DrawText(mainFont, empty ? hint : buf, p, 20, 1, empty ? (Color){110, 104, 140, 255} : WHITE);
    if(focused && fmod(GetTime(), 1.0) < 0.55) {
        const float w = empty ? 0.0f : MeasureTextEx(mainFont, buf, 20, 1).x;
        DrawRectangleRec((Rectangle){p.x + w + 1, r.y + 7, 2, r.height - 14}, ED_C_ACCENT);
    }
}

void Ed_DrawText(Font f, const char* t, Vector2 p, float size, float spacing, Color c)
{
    const float off = size >= 30 ? 3.0f : 2.0f;
    DrawTextEx(f, t, (Vector2) {p.x + off, p.y + off}, size, spacing, (Color) {0, 0, 0, 200});
    DrawTextEx(f, t, p, size, spacing, c);
}

/* Titulo de painel: barra de destaque colorida + texto branco. */
void Ed_DrawHeader(float x, float y, const char* t, Color c)
{
    DrawRectangleRounded((Rectangle){x, y + 2, 4, 20}, 0.5f, 4, c);
    Ed_DrawText(mainFont, t, (Vector2){x + 12, y}, 22, 1, WHITE);
}

static float backTimer = 0;

NumBox* focusNum = NULL;

NumBox nbSecBpm;

NumBox nbTopBpm;

NumBox nbSpeed;

NumBox nbLen;

void NumBox_Refresh(NumBox* nb, float v, char isInt) {
    if(isInt)
        snprintf(nb->buf, sizeof(nb->buf), "%d", (int)v);
    else if(v == (int)v)
        snprintf(nb->buf, sizeof(nb->buf), "%.0f", (double)v);
    else
        snprintf(nb->buf, sizeof(nb->buf), "%.1f", (double)v);
}

void TxtBox_Update(TxtBox* box, Rectangle r, char blocked) {
    if(blocked)
        return;
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(EdHover(r)) {
            focusBox = box;
            focusNum = NULL;
        } else if(focusBox == box) {
            focusBox = NULL;
        }
    }
    if(focusBox != box)
        return;
    int ch;
    while((ch = GetCharPressed()) > 0) {
        size_t len = strlen(box->buf);
        if(ch >= 32 && ch < 127 && len + 1 < sizeof(box->buf)) {
            box->buf[len] = (char)ch;
            box->buf[len + 1] = 0;
        }
    }
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if(ctrl && IsKeyPressed(KEY_V)) {
        const char* clip = GetClipboardText();
        if(clip != NULL) {
            for(size_t ci = 0; clip[ci] != 0; ci++) {
                size_t len = strlen(box->buf);
                if(len + 1 >= sizeof(box->buf))
                    break;
                if(clip[ci] >= 32 && clip[ci] < 127) {
                    box->buf[len] = clip[ci];
                    box->buf[len + 1] = 0;
                }
            }
        }
    }
    if(ctrl && IsKeyPressed(KEY_C) && box->buf[0] != 0)
        SetClipboardText(box->buf);
    if(IsKeyDown(KEY_BACKSPACE)) {
        backTimer -= RayGame_DeltaTime();
        if(IsKeyPressed(KEY_BACKSPACE) || backTimer <= 0) {
            size_t len = strlen(box->buf);
            if(len > 0) box->buf[len - 1] = 0;
            backTimer = 0.05f;
        }
    } else {
        backTimer = 0;
    }
}

void TxtBox_Draw(TxtBox* box, Rectangle r, const char* hint) {
    const char focused = (focusBox == box);
    Ed_DrawBox(r, ED_C_FIELD, focused ? ED_C_ACCENT : ED_C_EDGE);
    Field_DrawText(r, box->buf, hint, focused);
}

void NumBox_Update(NumBox* nb, Rectangle r, char blocked) {
    if(blocked)
        return;
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(EdHover(r)) {
            focusNum = nb;
            focusBox = NULL;
        } else if(focusNum == nb) {
            focusNum = NULL;
        }
    }
    if(focusNum != nb)
        return;
    int ch;
    while((ch = GetCharPressed()) > 0) {
        size_t len = strlen(nb->buf);
        if(((ch >= '0' && ch <= '9') || ch == '.') && len + 1 < sizeof(nb->buf)) {
            if(ch == '.' && strchr(nb->buf, '.') != NULL)
                continue;
            nb->buf[len] = (char)ch;
            nb->buf[len + 1] = 0;
        }
    }
    int ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    if(ctrl && IsKeyPressed(KEY_V)) {
        const char* clip = GetClipboardText();
        if(clip != NULL) {
            for(size_t ci = 0; clip[ci] != 0; ci++) {
                size_t len = strlen(nb->buf);
                if(len + 1 >= sizeof(nb->buf))
                    break;
                char c = clip[ci];
                if((c >= '0' && c <= '9') || (c == '.' && strchr(nb->buf, '.') == NULL)) {
                    nb->buf[len] = c;
                    nb->buf[len + 1] = 0;
                }
            }
        }
    }
    if(ctrl && IsKeyPressed(KEY_C) && nb->buf[0] != 0)
        SetClipboardText(nb->buf);
    if(IsKeyDown(KEY_BACKSPACE)) {
        backTimer -= RayGame_DeltaTime();
        if(IsKeyPressed(KEY_BACKSPACE) || backTimer <= 0) {
            size_t len = strlen(nb->buf);
            if(len > 0) nb->buf[len - 1] = 0;
            backTimer = 0.05f;
        }
    } else if(focusNum != nb) {
        backTimer = 0;
    }
}

void NumBox_Draw(NumBox* nb, Rectangle r) {
    const char focused = (focusNum == nb);
    Ed_DrawBox(r, ED_C_FIELD, focused ? ED_C_ACCENT : ED_C_EDGE);
    Field_DrawText(r, nb->buf, "0", focused);
}

char activeDrop = DROP_NONE;

char dropJustOpened = 0;

float dropX = 0, dropY = 0, dropW = 0;

CeDlg ceDlg;

int menuOpen = 0; // 0=fechado 1=File 2=Edit 3=View

Camera2D camEd;

// mouse em coordenada do mundo (stretch: proporcao direta; senao
// camera centralizada COM zoom)
Vector2 EdMouseWorld(void) {
    Vector2 m = GetMousePosition();
    if(gameOptions.fillScreen) {
        m.x = m.x * 1280 / GetScreenWidth();
        m.y = m.y * 720 / GetScreenHeight();
        return m;
    }
    float z = RayGame_ZoomFactor();
    if(z <= 0) z = 1;
    m.x = (m.x - GetScreenWidth() / 2) / z + 1280 / 2;
    m.y = (m.y - GetScreenHeight() / 2) / z + 720 / 2;
    return m;
}

// ---------- botoes e textos ----------
/* Cache de medida dos rotulos dos botoes (indexado pelo texto; colisao so refaz a medida). */
static TextMeasure btnMeasure[32];
static TextMeasure* BtnMeasureSlot(const char* label) {
    unsigned h = 0;
    for (const unsigned char* p = (const unsigned char*)label; *p; ++p) h = h * 31u + *p;
    return &btnMeasure[h & 31u];
}

char EdButton(Rectangle r, const char* label, int fs) {
    if(EdUiBlocked())
        return 0;
    const char hov = EdHover(r);
    Ed_DrawBox(r, hov ? ED_C_BTN_HOV : ED_C_BTN, hov ? ED_C_ACCENT : ED_C_EDGE);
    Vector2 s = TextMeasure_Get(BtnMeasureSlot(label), mainFont, label, fs, 1);
    Ed_DrawText(mainFont, label, (Vector2) {r.x + (r.width - s.x) / 2, r.y + (r.height - s.y) / 2}, fs, 1, WHITE);
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// Igual ao EdButton, mas com "ok = 0" (arquivo ausente) fica com borda e texto vermelhos.

// setinha que abre lista (desenha o triangulo na mao)
TxtBox* dropBox = NULL;
char (*dropNames)[SONG_NAME_MAX] = NULL;

int dropCount = 0;

char EdDropBtn(Rectangle r, char open) {
    if(ceDlg != DLG_NONE || menuOpen)
        return 0;
    const char hov = EdHover(r);
    // so o realce no hover; a seta e linhas (DrawTriangle some quando o sentido dos vertices esta errado)
    if(hov) DrawRectangleRounded(r, 0.3f, 4, (Color) {255, 255, 255, 30});
    const Color col = hov || open ? ED_C_ACCENT : ED_C_DIM;
    const float cx = r.x + r.width * 0.5f, cy = r.y + r.height * 0.5f;
    if(open) {
        DrawLineEx((Vector2) {cx - 6, cy + 3}, (Vector2) {cx, cy - 3}, 2.5f, col);
        DrawLineEx((Vector2) {cx, cy - 3}, (Vector2) {cx + 6, cy + 3}, 2.5f, col);
    } else {
        DrawLineEx((Vector2) {cx - 6, cy - 3}, (Vector2) {cx, cy + 3}, 2.5f, col);
        DrawLineEx((Vector2) {cx, cy + 3}, (Vector2) {cx + 6, cy - 3}, 2.5f, col);
    }
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

int dropScroll = 0;

void EdTimeFmt(char* out, size_t n, double ms) {
    if(ms < 0) ms = 0;
    int m = (int)(ms / 60000);
    int s = (int)(ms / 1000) % 60;
    int d = (int)(ms / 100) % 10;
    snprintf(out, n, "%d:%02d.%d", m, s, d);
}

// botao do dialogo (o EdButton() normal fica bloqueado com dialogo aberto)
char EdDlgButton(Rectangle r, const char* label, Color edge) {
    const char hov = EdHover(r);
    DrawRectangleRounded(r, 0.25f, 6, hov ? ED_C_BTN_HOV : ED_C_BTN);
    DrawRectangleRoundedLinesEx(r, 0.25f, 6, 2.0f, hov ? WHITE : edge);
    Vector2 sz = TextMeasure_Get(BtnMeasureSlot(label), mainFont, label, 24, 1);
    Ed_DrawText(mainFont, label, (Vector2) {r.x + (r.width - sz.x) / 2, r.y + (r.height - sz.y) / 2}, 24, 1, WHITE);
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void NumBox_Commit(void) {
    if(focusNum == NULL)
        return;
    if(focusNum == &nbSecBpm) {
        float v = Clamp((float)atof(nbSecBpm.buf), 10.0f, 300.0f);
        if(v != chart.secs[secIdx].bpm) {
            EdTouchSec(secIdx);
            chart.secs[secIdx].bpm = v;
            SecTimes_Invalidate(); // inicio dos trechos seguintes muda
        }
    } else if(focusNum == &nbTopBpm) {
        float v = Clamp((float)atof(nbTopBpm.buf), 10.0f, 300.0f);
        if(v != chart.bpm) {
            EdTouchMeta();
            chart.bpm = v;
            SecTimes_Invalidate();
        }
    } else if(focusNum == &nbLen) {
        int v = (int)Clamp((float)atoi(nbLen.buf), 1.0f, 64.0f);
        if(v != chart.secs[secIdx].lenSteps) {
            EdTouchSec(secIdx);
            chart.secs[secIdx].lenSteps = v;
            SecTimes_Invalidate(); // duracao do trecho muda
        }
    } else if(focusNum == &nbSpeed) {
        float v = Clamp((float)atof(nbSpeed.buf), 0.2f, 5.0f);
        if(v != chart.speed) {
            EdTouchMeta();
            chart.speed = v;
        }
    }
    focusNum = NULL;
}

// desenha um icone de 20px centrado em (c.x, c.y); cores fixas por tipo
static void Ed_Rect(float x, float y, float w, float h, Color col) {
    DrawRectangleRec((Rectangle){x, y, w, h}, col);
}
static void Ed_DrawIcon(EdIcon icon, Vector2 c) {
    switch(icon) {
    case ED_ICON_PLAYHEAD: { // triangulo + barra vermelha (marcador da grade)
        Color red = (Color){220,40,40,255};
        DrawTriangle((Vector2){c.x - 9, c.y - 6}, (Vector2){c.x - 9, c.y + 6}, (Vector2){c.x + 1, c.y}, red);
        Ed_Rect(c.x + 1, c.y - 1.5f, 9, 3, red);
        break;
    }
    case ED_ICON_RESTART: { // seta circular girando (arco + ponta)
        Color col = (Color){210,210,210,255};
        DrawRing(c, 4.0f, 6.5f, 30.0f, 300.0f, 16, col);
        float a = 300.0f * 3.14159265f / 180.0f;
        Vector2 rad = {cosf(a), sinf(a)}, tan = {-sinf(a), cosf(a)};
        Vector2 p = {c.x + rad.x * 5.25f, c.y + rad.y * 5.25f};
        Vector2 tip = {p.x + tan.x * 4.0f, p.y + tan.y * 4.0f};
        Vector2 b1 = {p.x + rad.x * 3.5f, p.y + rad.y * 3.5f}, b2 = {p.x - rad.x * 3.5f, p.y - rad.y * 3.5f};
        DrawTriangle(tip, b1, b2, col);
        break;
    }
    case ED_ICON_TRASH: { // lixeira
        Color col = (Color){220,80,80,255};
        Ed_Rect(c.x - 5, c.y - 6, 10, 2, col);
        Ed_Rect(c.x - 2, c.y - 8, 4, 2, col);
        Ed_Rect(c.x - 4, c.y - 3, 8, 11, col);
        Ed_Rect(c.x - 2, c.y - 1, 1, 7, (Color){16,16,16,255});
        Ed_Rect(c.x + 1, c.y - 1, 1, 7, (Color){16,16,16,255});
        break;
    }
    case ED_ICON_SAVE: { // disquete
        Color col = (Color){70,130,230,255};
        Ed_Rect(c.x - 8, c.y - 8, 16, 16, col);
        Ed_Rect(c.x - 4.5f, c.y - 8, 9, 5, (Color){200,200,200,255});
        Ed_Rect(c.x - 5, c.y + 1, 10, 7, (Color){235,235,235,255});
        break;
    }
    case ED_ICON_DOOR: { // porta com maçaneta
        Color col = (Color){210,140,60,255};
        Ed_Rect(c.x - 6, c.y - 8, 12, 16, col);
        Ed_Rect(c.x - 4, c.y - 6, 8, 13, (Color){90,50,20,255});
        DrawCircle((int)(c.x + 2), (int)c.y, 1.3f, (Color){250,220,80,255});
        break;
    }
    case ED_ICON_BACK: { // seta para a esquerda
        Color col = (Color){210,210,210,255};
        DrawTriangle((Vector2){c.x - 9, c.y}, (Vector2){c.x - 1, c.y - 7}, (Vector2){c.x - 1, c.y + 7}, col);
        Ed_Rect(c.x - 2, c.y - 2.5f, 10, 5, col);
        break;
    }
    default:
        break;
    }
}

// menu File/Edit/View estilo Psych (dropdown, sem alloc)
char EdMenuItemIcon(Rectangle r, const char* label, EdIcon icon) {
    const char hov = EdHover(r);
    if(hov) {
        DrawRectangleRounded(r, 0.25f, 6, ED_C_BTN_HOV);
        DrawRectangleRec((Rectangle){r.x, r.y + 5, 3, r.height - 10}, ED_C_ACCENT);
    }
    Ed_DrawText(mainFont, label, (Vector2){r.x + (icon != ED_ICON_NONE ? 32 : 10), r.y + 5}, 20, 1, hov ? ED_C_ACCENT : WHITE);
    if(icon != ED_ICON_NONE)
        Ed_DrawIcon(icon, (Vector2){r.x + 16, r.y + r.height * 0.5f});
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

char EdMenuItem(Rectangle r, const char* label) {
    return EdMenuItemIcon(r, label, ED_ICON_NONE);
}
