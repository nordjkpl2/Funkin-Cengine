/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Chart Editor: caixas de texto/numero, botoes, listas e bloqueio do mouse. */
#include"scenes/AllScenes.h"
#include"data/Song.h"
#include"data/SongList.h"
#include "core/ModPath.h"
#include "logic/Character.h"
#include"logic/Stage.h"
#include"logic/Note.h"
#include "Game.h"
#include "GameData.h"
#include "raymath.h"
#include "EditorWidgets.h"
#include "EditorDataAndIO.h"
#include "EditorTimeline.h"

TxtBox* focusBox = NULL;

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
    Vector2 m = EdMouseWorld();
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height) {
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
    DrawRectangleRec(r, (Color) {20, 20, 20, 255});
    DrawRectangleLinesEx(r, 1, (focusBox == box) ? GREEN : GRAY);
    const char* txt = (box->buf[0] != 0) ? box->buf : hint;
    Color c = (box->buf[0] != 0) ? WHITE : (Color) {120, 120, 120, 255};
    DrawTextEx(mainFont, txt, (Vector2) {r.x + 6, r.y + 6}, 20, 1, c);
}

void NumBox_Update(NumBox* nb, Rectangle r, char blocked) {
    if(blocked)
        return;
    Vector2 m = EdMouseWorld();
    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if(m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height) {
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
    DrawRectangleRec(r, (Color) {20, 20, 20, 255});
    DrawRectangleLinesEx(r, 1, (focusNum == nb) ? GREEN : GRAY);
    const char* txt = (nb->buf[0] != 0) ? nb->buf : "0";
    DrawTextEx(mainFont, txt, (Vector2) {r.x + 6, r.y + 6}, 20, 1, YELLOW);
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
char EdButton(Rectangle r, const char* label, int fs) {
    if(ceDlg != DLG_NONE || activeDrop || menuOpen)
        return 0;
    Vector2 m = EdMouseWorld();
    char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
    DrawRectangleRec(r, hov ? (Color) {60, 60, 60, 255} : (Color) {30, 30, 30, 255});
    DrawRectangleLinesEx(r, 1, hov ? GREEN : GRAY);
    Vector2 s = MeasureTextEx(mainFont, label, fs, 1);
    DrawTextEx(mainFont, label, (Vector2) {r.x + (r.width - s.x) / 2, r.y + (r.height - s.y) / 2}, fs, 1, WHITE);
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

// setinha que abre lista (desenha o triangulo na mao)
TxtBox* dropBox = NULL;
char (*dropNames)[SONG_NAME_MAX] = NULL;

int dropCount = 0;

char EdDropBtn(Rectangle r, char open) {
    if(ceDlg != DLG_NONE || menuOpen)
        return 0;
    Vector2 m = EdMouseWorld();
    char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
    DrawRectangleRec(r, hov ? (Color) {60, 60, 60, 255} : (Color) {30, 30, 30, 255});
    DrawRectangleLinesEx(r, 1, hov ? GREEN : GRAY);
    Vector2 c = {r.x + r.width / 2, r.y + r.height / 2};
    if(open)
        DrawTriangle((Vector2) {c.x - 7, c.y + 4}, (Vector2) {c.x + 7, c.y + 4}, (Vector2) {c.x, c.y - 5}, WHITE);
    else
        DrawTriangle((Vector2) {c.x - 7, c.y - 4}, (Vector2) {c.x + 7, c.y - 4}, (Vector2) {c.x, c.y + 5}, WHITE);
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
    Vector2 m = EdMouseWorld();
    char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
    DrawRectangleRec(r, hov ? (Color) {60, 60, 60, 255} : (Color) {30, 30, 30, 255});
    DrawRectangleLinesEx(r, 2, hov ? WHITE : edge);
    Vector2 sz = MeasureTextEx(mainFont, label, 24, 1);
    DrawTextEx(mainFont, label, (Vector2) {r.x + (r.width - sz.x) / 2, r.y + (r.height - sz.y) / 2}, 24, 1, WHITE);
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

void NumBox_Commit(void) {
    if(focusNum == NULL)
        return;
    if(focusNum == &nbSecBpm) {
        float v = Clamp((float)atof(nbSecBpm.buf), 10.0f, 300.0f);
        if(v != chart.secs[secIdx].bpm) {
            EdTouch();
            chart.secs[secIdx].bpm = v;
            SecTimes_Invalidate(); // inicio dos trechos seguintes muda
        }
    } else if(focusNum == &nbTopBpm) {
        float v = Clamp((float)atof(nbTopBpm.buf), 10.0f, 300.0f);
        if(v != chart.bpm) {
            EdTouch();
            chart.bpm = v;
            SecTimes_Invalidate();
        }
    } else if(focusNum == &nbLen) {
        int v = (int)Clamp((float)atoi(nbLen.buf), 1.0f, 64.0f);
        if(v != chart.secs[secIdx].lenSteps) {
            EdTouch();
            chart.secs[secIdx].lenSteps = v;
            SecTimes_Invalidate(); // duracao do trecho muda
        }
    } else if(focusNum == &nbSpeed) {
        float v = Clamp((float)atof(nbSpeed.buf), 0.2f, 5.0f);
        if(v != chart.speed) {
            EdTouch();
            chart.speed = v;
        }
    }
    focusNum = NULL;
}

// menu File/Edit/View estilo Psych (dropdown, sem alloc)
char EdMenuItem(Rectangle r, const char* label) {
    Vector2 m = EdMouseWorld();
    char hov = (m.x >= r.x && m.x <= r.x + r.width && m.y >= r.y && m.y <= r.y + r.height);
    DrawRectangleRec(r, hov ? (Color){60,60,60,255} : (Color){18,18,18,255});
    DrawTextEx(mainFont, label, (Vector2){r.x + 8, r.y + 5}, 20, 1, WHITE);
    return hov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
