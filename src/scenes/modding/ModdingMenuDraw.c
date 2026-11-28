/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * ModdingMenuDraw.c - desenho do menu de modding (lista de abas, tela de import, tela em breve).
 */
#include "scenes/api/FontLoader.h"
#include "scenes/modding/ModdingMenuInternal.h"
#include "data/modimport/ModImport.h"
#include "rendering/TextMeasure.h"
#include "Game.h"
#include "rlgl.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void Modding_BatchWith(Font font)
{
    /* GenImageFontAtlas (SUPPORT_FONT_ATLAS_WHITE_REC) poe um quadrado branco 3x3 no canto inferior direito;
       o pixel do meio evita borda com filtro bilinear. Sem textura, volta ao padrao do raylib. */
    const Texture2D t = font.texture;
    if (t.id == 0 || t.width < 3 || t.height < 3)
        SetShapesTexture((Texture2D){0}, (Rectangle){0});
    else
        SetShapesTexture(t, (Rectangle){(float)t.width - 2.0f, (float)t.height - 2.0f, 1.0f, 1.0f});
}

void Modding_DrawBackground(void)
{
    Render_SetCamera(&cam);
    Render_DrawGraphicObject(&bg);
}

/* Icones desenhados mais abaixo. */
static void Modding_DrawMusicNote(float x, float y, float size, Color c);
static void Modding_DrawPerson(float x, float y, float size, Color c);
static void Modding_DrawDownload(float x, float y, float size, Color c);
static void Modding_DrawHudIcon(float x, float y, float size, Color c);

static void Imp_FormatSize(long long bytes, char *out, size_t cap)
{
    const double b = (double)bytes;
    if (bytes >= (1LL << 30))      snprintf(out, cap, "%.2f GB", b / 1073741824.0);
    else if (bytes >= (1LL << 20)) snprintf(out, cap, "%.1f MB", b / 1048576.0);
    else if (bytes >= (1LL << 10)) snprintf(out, cap, "%.0f KB", b / 1024.0);
    else                           snprintf(out, cap, "%lld B", bytes);
}

/* Fim do texto que cabe em maxW (caminhos longos mostram o final).
 * Cada corte mede o resto do texto, ou seja O(n^2) por frame. O corte so muda com o
 * texto e a largura, entao o resultado fica guardado enquanto eles nao mudarem. */
static const char *Imp_FitTail(const char *text, float maxW, float size)
{
    static char key[MODDING_PATH_CAP];
    static float keyW = -1.0f, keySize = -1.0f;
    static size_t keyOff;

    if (keyW != maxW || keySize != size || strcmp(key, text) != 0) {
        const char *p = text;
        while (*p && MeasureTextEx(mainFont, p, size, 1).x > maxW) {
            ++p;
            while ((*p & 0xC0) == 0x80) ++p;
        }
        snprintf(key, sizeof(key), "%s", text);
        keyW = maxW;
        keySize = size;
        keyOff = (size_t)(p - text);
    }
    return text + keyOff;
}

/* Quebras de linha de um texto ja medido. As linhas ficam em `packed`, separadas por '\0'. */
enum { IMP_WRAP_INTRO, IMP_WRAP_DEST, IMP_WRAP_PROGRESS, IMP_WRAP_STATUS, IMP_WRAP_DESC, IMP_WRAP_SLOTS };

typedef struct {
    char valid;
    float maxW, size;
    char key[MODDING_MSG_CAP];
    char packed[2 * MODDING_MSG_CAP];
    size_t used;
    int count;
    const char *lines[MODDING_MSG_CAP / 2 + 1];
} ImpWrapCache;

static ImpWrapCache impWrap[IMP_WRAP_SLOTS];

static void Imp_WrapEmit(ImpWrapCache *w, const char *s, size_t n)
{
    if (w->count >= (int)(sizeof(w->lines) / sizeof(w->lines[0])) || w->used + n + 1 > sizeof(w->packed))
        return;
    w->lines[w->count++] = w->packed + w->used;
    memcpy(w->packed + w->used, s, n);
    w->packed[w->used + n] = '\0';
    w->used += n + 1;
}

/* Quebra por palavras e guarda o resultado no slot. Mesma regra de antes, so que sem desenhar. */
static void Imp_WrapBuild(ImpWrapCache *w, const char *text, float maxW, float size)
{
    char line[MODDING_MSG_CAP] = "";
    char test[MODDING_MSG_CAP];
    size_t len = 0;
    const char *p = text;

    w->valid = 1;
    w->maxW = maxW;
    w->size = size;
    snprintf(w->key, sizeof(w->key), "%s", text);
    w->used = 0;
    w->count = 0;

    while (*p) {
        const char *e = p;
        size_t wl, tl;
        while (*e && *e != ' ' && *e != '\n') ++e;
        wl = (size_t)(e - p);
        if (wl + len + 2 >= sizeof(test)) break;
        memcpy(test, line, len);
        tl = len;
        if (len) test[tl++] = ' ';
        memcpy(test + tl, p, wl);
        tl += wl;
        test[tl] = '\0';
        if (len && MeasureTextEx(mainFont, test, size, 1).x > maxW) {
            Imp_WrapEmit(w, line, len);
            memcpy(line, p, wl);
            line[wl] = '\0';
            len = wl;
        } else {
            memcpy(line, test, tl + 1);
            len = tl;
        }
        p = e;
        while (*p == ' ' || *p == '\n') ++p;
    }
    if (len) Imp_WrapEmit(w, line, len);
}

/* Desenha o texto quebrado; devolve o y logo abaixo dele. Refaz a quebra so se o texto ou a largura mudarem. */
static float Imp_DrawWrapped(int slot, const char *text, float x, float y, float maxW,
                             float size, Color c)
{
    ImpWrapCache *w = &impWrap[slot];

    if (!w->valid || w->maxW != maxW || w->size != size || strcmp(w->key, text) != 0)
        Imp_WrapBuild(w, text, maxW, size);

    for (int i = 0; i < w->count; ++i) {
        DrawTextEx(mainFont, w->lines[i], (Vector2){x, y}, size, 1, c);
        y += size + 6;
    }
    return y;
}

static void Imp_Panel(Rectangle r, const char *title)
{
    DrawRectangleRec(r, (Color){10, 10, 16, 215});
    DrawRectangleLinesEx(r, 2, (Color){42, 209, 86, 150});
    DrawTextEx(mainFont, title, (Vector2){r.x + 20, r.y + 14}, 22, 2, selectedColor);
    DrawRectangle((int)r.x + 20, (int)r.y + 46, (int)r.width - 40, 1, (Color){255, 255, 255, 40});
}

#define IMP_CHIP_SLOTS 5

/* Tecla + rotulo; devolve o x onde termina. `slot` (0..IMP_CHIP_SLOTS-1) identifica o chip: cada um
 * guarda a medida do proprio texto, que e fixo, entao nao mede de novo a cada frame. */
static float Imp_Chip(int slot, float x, float y, const char *key, const char *label,
                      Color accent, char active)
{
    static TextMeasure keyM[IMP_CHIP_SLOTS], labelM[IMP_CHIP_SLOTS];
    const float kw = TextMeasure_Get(&keyM[slot], mainFont, key, 17, 1).x + 20;
    const Color text = active ? fontColor : (Color){130, 130, 140, 255};

    DrawRectangle((int)x, (int)y, (int)kw, 30, active ? (Color){accent.r, accent.g, accent.b, 70} : (Color){40, 40, 48, 255});
    DrawRectangleLinesEx((Rectangle){x, y, kw, 30}, 1, active ? accent : (Color){70, 70, 80, 255});
    DrawTextEx(mainFont, key, (Vector2){x + 10, y + 6}, 17, 1, text);
    DrawTextEx(mainFont, label, (Vector2){x + kw + 8, y + 5}, 19, 1, text);
    return x + kw + 8 + TextMeasure_Get(&labelM[slot], mainFont, label, 19, 1).x + 22;
}

static void Imp_StatCard(float x, float y, int kind, const char *value, const char *label)
{
    const Rectangle card = {x, y, 410, 68};
    const Color icon = selectedColor;

    DrawRectangleRec(card, (Color){255, 255, 255, 14});
    DrawRectangle((int)x + 10, (int)y + 10, 48, 48, (Color){42, 209, 86, 45});
    if (kind == 0)      Modding_DrawPerson(x + 22, y + 18, 24, icon);
    else if (kind == 1) Modding_DrawMusicNote(x + 22, y + 20, 24, icon);
    else                Modding_DrawDownload(x + 22, y + 20, 24, icon);
    DrawTextEx(mainFont, value, (Vector2){x + 72, y + 6}, 30, 2, fontColor);
    DrawTextEx(mainFont, label, (Vector2){x + 72, y + 40}, 19, 1, (Color){150, 150, 160, 255});
}

void Modding_DrawImport(void)
{
    const Color muted = {150, 150, 160, 255};
    const Color warning = impConfirmDel ? RED : YELLOW;
    const Rectangle left = {50, 130, 700, 430};
    const Rectangle right = {780, 130, 450, 430};
    const char busy = ModImport_ScanBusy();
    const char importing = ModImport_Busy();
    char a[96], b[96], msg[MODDING_MSG_CAP];
    float y, x;
    static TextMeasure ellipsisM, pathM, rightM; // textos de medida variavel: so refazem quando mudam

    Modding_DrawBackground();
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 120});

    /* topo: seta de voltar + titulo */
    DrawTriangle((Vector2){76, 52}, (Vector2){76, 88}, (Vector2){50, 70}, fontColor);
    DrawRectangle(76, 64, 24, 12, fontColor);
    DrawTextEx(mainFont, "IMPORT MODS", (Vector2){120, 46}, 44, 4, fontColor);

    /* esquerda: origem */
    Imp_Panel(left, "SOURCE FOLDER");
    DrawTextEx(mainFont, "Directory containing songs/, data/ or weeks/",
               (Vector2){left.x + 20, left.y + 60}, 19, 1, muted);
    {
        const Rectangle box = {left.x + 20, left.y + 92, left.width - 40, 46};
        const float textW = box.width - 24;
        const char *shown = Imp_FitTail(impPath, textW - 24, 20);
        float tx = box.x + 12;

        DrawRectangleRec(box, (Color){20, 20, 20, 255});
        DrawRectangleLinesEx(box, 2, GREEN);
        if (impPath[0]) {
            if (shown != impPath) {
                DrawTextEx(mainFont, "...", (Vector2){tx, box.y + 12}, 20, 1, muted);
                tx += TextMeasure_Get(&ellipsisM, mainFont, "...", 20, 1).x + 4;
            }
            DrawTextEx(mainFont, shown, (Vector2){tx, box.y + 12}, 20, 1, WHITE);
            tx += TextMeasure_Get(&pathM, mainFont, shown, 20, 1).x + 3;
        } else {
            DrawTextEx(mainFont, "C:/.../mod-folder", (Vector2){tx, box.y + 12}, 20, 1, (Color){110, 110, 120, 255});
        }
        if (((int)(GetTime() * 2.0)) & 1)
            DrawRectangle((int)tx, (int)box.y + 10, 2, 26, fontColor);
    }
    x = left.x + 20;
    y = left.y + 156;
    x = Imp_Chip(0, x, y, "ENTER", "Import", selectedColor, impPath[0] != '\0' && !importing);
    x = Imp_Chip(1, x, y, "CTRL+V", "Paste path", selectedColor, 1);
    (void)Imp_Chip(2, x, y, "BKSP", "Erase", selectedColor, impPath[0] != '\0');

    DrawRectangle((int)left.x + 20, (int)left.y + 206, (int)left.width - 40, 1, (Color){255, 255, 255, 30});
    y = Imp_DrawWrapped(IMP_WRAP_INTRO, "Imports charts, music, weeks, stages, images, characters and icons.",
                        left.x + 20, left.y + 222, left.width - 40, 19, fontColor);
    y = Imp_DrawWrapped(IMP_WRAP_DEST, "Destination: assets/mods/<mod-name>/",
                        left.x + 20, y + 2, left.width - 40, 19, muted);

    if (importing) {
        const float top = left.y + 322;
        char c[96];
        Imp_FormatSize(impProg.bytesDone, c, sizeof(c));
        snprintf(msg, sizeof(msg), "%s... %d / %d files, %s copied%s",
                 impProg.cancelling ? "Cancelling" : "Importing",
                 impProg.filesDone, impProg.filesTotal, c,
                 impProg.failed ? " (some files failed)" : "");
        DrawRectangle((int)left.x + 20, (int)top, 4, 90, selectedColor);
        (void)Imp_DrawWrapped(IMP_WRAP_PROGRESS, msg, left.x + 36, top + 2, left.width - 60, 19, selectedColor);
    } else if (impMsg[0]) {
        const float top = left.y + 322;
        snprintf(msg, sizeof(msg), "%s", impMsg);
        DrawRectangle((int)left.x + 20, (int)top, 4, 90, warning);
        (void)Imp_DrawWrapped(IMP_WRAP_STATUS, msg, left.x + 36, top + 2, left.width - 60, 19, warning);
    }

    /* direita: dados que ja existem */
    Imp_Panel(right, "INSTALLED");
    if (busy) {
        snprintf(a, sizeof(a), "...");
        snprintf(b, sizeof(b), "...");
        snprintf(msg, sizeof(msg), "...");
    } else {
        snprintf(a, sizeof(a), "%d", impMods);
        snprintf(b, sizeof(b), "%d", impSongs);
        Imp_FormatSize(impKB * 1024, msg, sizeof(msg));
    }
    Imp_StatCard(right.x + 20, right.y + 60, 0, a, "Mods");
    Imp_StatCard(right.x + 20, right.y + 138, 1, b, "Songs");
    Imp_StatCard(right.x + 20, right.y + 216, 2, msg, "Disk usage");

    DrawTextEx(mainFont, "LAST IMPORT", (Vector2){right.x + 20, right.y + 298}, 19, 2, selectedColor);
    if (impLastValid) {
        char c[96];
        y = right.y + 326;
        snprintf(a, sizeof(a), "Songs %d   Charts %d   Weeks %d   Stages %d",
                 impLast.songs, impLast.charts, impLast.weeks, impLast.stages);
        snprintf(b, sizeof(b), "Images %d   Characters %d   Icons %d",
                 impLast.images, impLast.characters, impLast.icons);
        Imp_FormatSize(impLast.bytes, msg, sizeof(msg));
        snprintf(c, sizeof(c), "Copied %s, %d same files skipped", msg, impLastDup);
        DrawTextEx(mainFont, a, (Vector2){right.x + 20, y}, 19, 1, fontColor);
        DrawTextEx(mainFont, b, (Vector2){right.x + 20, y + 28}, 19, 1, fontColor);
        DrawTextEx(mainFont, c, (Vector2){right.x + 20, y + 62}, 19, 1, muted);
    } else {
        DrawTextEx(mainFont, "Nothing imported in this session.",
                   (Vector2){right.x + 20, right.y + 326}, 19, 1, muted);
    }

    /* rodape: barra de uso de disco (parte do ultimo import) + atalhos */
    Imp_FormatSize(impKB * 1024, a, sizeof(a));
    if (importing) snprintf(msg, sizeof(msg), "Importing: %d / %d files", impProg.filesDone, impProg.filesTotal);
    else if (busy) snprintf(msg, sizeof(msg), "Scanning mods...");
    else           snprintf(msg, sizeof(msg), "Disk usage: %s", a);
    DrawTextEx(mainFont, msg, (Vector2){50, 586}, 19, 1, fontColor);
    if (importing) {
        Imp_FormatSize(impProg.bytesDone, a, sizeof(a));
        snprintf(msg, sizeof(msg), "%s copied", a);
        DrawTextEx(mainFont, msg,
                   (Vector2){1230 - TextMeasure_Get(&rightM, mainFont, msg, 19, 1).x, 586}, 19, 1, muted);
    } else if (impLastValid && !busy) {
        Imp_FormatSize(impLast.bytes, a, sizeof(a));
        snprintf(msg, sizeof(msg), "Last import: %s", a);
        DrawTextEx(mainFont, msg,
                   (Vector2){1230 - TextMeasure_Get(&rightM, mainFont, msg, 19, 1).x, 586}, 19, 1, muted);
    }
    {
        const Rectangle bar = {50, 616, 1180, 14};
        DrawRectangleRec(bar, (Color){255, 255, 255, 30});
        if (importing && impProg.filesTotal > 0) {
            float f = (float)impProg.filesDone / (float)impProg.filesTotal;
            if (f > 0.99f) f = 0.99f;
            DrawRectangle((int)bar.x, (int)bar.y, (int)(bar.width * f) + 3, 14, selectedColor);
        } else if (busy || importing) {
            const float t = (float)fmod(GetTime() * 0.8, 1.0);
            const float segW = 220;
            const float sx = bar.x + t * (bar.width + segW) - segW;
            const float x0 = sx < bar.x ? bar.x : sx;
            const float x1 = sx + segW > bar.x + bar.width ? bar.x + bar.width : sx + segW;
            if (x1 > x0) DrawRectangle((int)x0, (int)bar.y, (int)(x1 - x0), 14, selectedColor);
        } else if (impKB > 0 && impLastValid && impLast.bytes > 0) {
            float f = (float)((double)impLast.bytes / ((double)impKB * 1024.0));
            if (f > 1.0f) f = 1.0f;
            DrawRectangle((int)bar.x, (int)bar.y, (int)(bar.width * f) + 3, 14, selectedColor);
        }
        DrawRectangleLinesEx(bar, 1, (Color){255, 255, 255, 60});
    }
    x = Imp_Chip(3, 50, 650, "ESC", importing ? "Cancel import" : "Back", selectedColor, 1);
    (void)Imp_Chip(4, x, 650, "DELETE x2", impConfirmDel ? "Press again to remove all mods" : "Remove all mods",
                   warning, impMods > 0 && impPath[0] == '\0' && !importing);

    Render_StopCamera();
}

void Modding_DrawStub(void)
{
    const char *message = "COMING SOON";
    const Vector2 center = {640.0f, 360.0f};
    static TextMeasure messageM, subM; // textos fixos da tela de stub
    const Vector2 size = TextMeasure_Get(&messageM, mainFont, message, 80, 5);

    Modding_DrawBackground();
    DrawTextEx(mainFont, message,
               (Vector2){center.x - size.x * 0.5f,
                         center.y - size.y * 0.5f},
               80, 5, fontColor);

    if (stubTitle) {
        const Vector2 sub = TextMeasure_Get(&subM, mainFont, stubTitle, 32, 4);
        DrawTextEx(mainFont, stubTitle,
                   (Vector2){center.x - sub.x * 0.5f, center.y + 80.0f},
                   32, 4, fontColor);
    }

    Render_StopCamera();
}

/* Nota musical (colcheia) feita de circulo + retangulo + triangulo; (x,y) = canto superior esquerdo.
 * Sem DrawEllipse: ela usa RL_TRIANGLES sem textura e abre outro draw call. */
static void Modding_DrawMusicNote(float x, float y, float size, Color c)
{
    const float headW = size * 0.30f;
    const float headH = size * 0.22f;
    const float stemW = size * 0.08f;
    const float stemX = x + headW * 1.8f;
    const float headY = y + size - headH;

    DrawCircleV((Vector2){x + headW, headY}, headH * 1.2f, c);
    DrawRectangle((int)(stemX - stemW), (int)y, (int)stemW + 1, (int)(headY - y), c);
    DrawTriangle((Vector2){stemX, y},
                 (Vector2){stemX + size * 0.30f, y + size * 0.30f},
                 (Vector2){stemX, y + size * 0.45f}, c);
}

/* Silhueta de pessoa (avatar sem foto): cabeca + ombros. */
static void Modding_DrawPerson(float x, float y, float size, Color c)
{
    const float cx = x + size * 0.5f;
    DrawCircle((int)cx, (int)(y + size * 0.28f), size * 0.22f, c);
    DrawCircleSector((Vector2){cx, y + size * 1.02f}, size * 0.46f, 180.0f, 360.0f, 24, c);
}

/* Icone de download: seta pra baixo + bandeja. */
static void Modding_DrawDownload(float x, float y, float size, Color c)
{
    const float cx = x + size * 0.5f;
    const float stemW = size * 0.18f;
    const float trayY = y + size * 0.86f;

    DrawRectangle((int)(cx - stemW * 0.5f), (int)y, (int)stemW, (int)(size * 0.40f), c);
    DrawTriangle((Vector2){cx - size * 0.30f, y + size * 0.38f},
                 (Vector2){cx, y + size * 0.72f},
                 (Vector2){cx + size * 0.30f, y + size * 0.38f}, c);
    DrawRectangle((int)x, (int)trayY, (int)size, (int)(size * 0.14f), c);
    DrawRectangle((int)x, (int)(trayY - size * 0.16f), (int)(size * 0.14f), (int)(size * 0.16f), c);
    DrawRectangle((int)(x + size * 0.86f), (int)(trayY - size * 0.16f), (int)(size * 0.14f), (int)(size * 0.16f), c);
}

/* Icone do HUD Editor: moldura de tela com barra de vida, dois icones e o tempo. */
static void Modding_DrawHudIcon(float x, float y, float size, Color c)
{
    const float h = size * 0.75f;
    DrawRectangleLinesEx((Rectangle){x, y, size, h}, fmaxf(2.0f, size * 0.05f), c);
    DrawRectangle((int)(x + size * 0.18f), (int)(y + h * 0.66f), (int)(size * 0.64f), (int)(size * 0.08f) + 1, c);
    DrawCircleV((Vector2){x + size * 0.44f, y + h * 0.69f}, size * 0.09f, c);
    DrawCircleV((Vector2){x + size * 0.58f, y + h * 0.69f}, size * 0.09f, c);
    DrawRectangle((int)(x + size * 0.38f), (int)(y + h * 0.18f), (int)(size * 0.24f), (int)(size * 0.06f) + 1, c);
}

/* Palco: moldura + cortinas nos cantos + linha do chao. */
static void Modding_DrawStageIcon(float x, float y, float size, Color c)
{
    const float h = size * 0.8f;
    DrawRectangleLinesEx((Rectangle){x, y, size, h}, fmaxf(2.0f, size * 0.05f), c);
    DrawTriangle((Vector2){x, y}, (Vector2){x, y + h * 0.7f}, (Vector2){x + size * 0.28f, y}, c);
    DrawTriangle((Vector2){x + size * 0.72f, y}, (Vector2){x + size, y + h * 0.7f}, (Vector2){x + size, y}, c);
    DrawRectangle((int)x, (int)(y + h * 0.78f), (int)size, (int)(size * 0.06f) + 1, c);
}

/* Semana: calendario (cabecalho + grade 3x2). */
static void Modding_DrawWeekIcon(float x, float y, float size, Color c)
{
    const float h = size * 0.85f;
    const float cell = size * 0.16f;
    DrawRectangleLinesEx((Rectangle){x, y, size, h}, fmaxf(2.0f, size * 0.05f), c);
    DrawRectangle((int)x, (int)y, (int)size, (int)(h * 0.22f), c);
    for (int r = 0; r < 2; ++r)
        for (int k = 0; k < 3; ++k)
            DrawRectangle((int)(x + size * 0.14f + (float)k * size * 0.27f),
                          (int)(y + h * 0.36f + (float)r * h * 0.30f), (int)cell, (int)cell, c);
}

/* Brilho de 4 pontas (losangos finos) centrado em (cx,cy). */
static void Modding_DrawSparkle(float cx, float cy, float r, Color c)
{
    const float w = r * 0.28f;
    DrawTriangle((Vector2){cx, cy - r}, (Vector2){cx - w, cy}, (Vector2){cx + w, cy}, c);
    DrawTriangle((Vector2){cx - w, cy}, (Vector2){cx, cy + r}, (Vector2){cx + w, cy}, c);
    DrawTriangle((Vector2){cx - r, cy}, (Vector2){cx, cy + w}, (Vector2){cx, cy - w}, c);
    DrawTriangle((Vector2){cx, cy - w}, (Vector2){cx, cy + w}, (Vector2){cx + r, cy}, c);
}

static void Modding_DrawEffectsIcon(float x, float y, float size, Color c)
{
    Modding_DrawSparkle(x + size * 0.42f, y + size * 0.55f, size * 0.42f, c);
    Modding_DrawSparkle(x + size * 0.84f, y + size * 0.16f, size * 0.16f, c);
}

/* Splash: circulo central + gotas em volta. */
static void Modding_DrawSplashIcon(float x, float y, float size, Color c)
{
    const float cx = x + size * 0.5f, cy = y + size * 0.5f;
    DrawCircleV((Vector2){cx, cy}, size * 0.18f, c);
    for (int i = 0; i < 6; ++i) {
        const float a = (float)i * (PI / 3.0f) + 0.3f;
        DrawCircleV((Vector2){cx + cosf(a) * size * 0.38f, cy + sinf(a) * size * 0.38f}, size * 0.08f, c);
    }
}

/* Dialogo: balao com rabicho e tres linhas de texto. */
static void Modding_DrawDialogueIcon(float x, float y, float size, Color c)
{
    const Rectangle body = {x, y + size * 0.08f, size, size * 0.62f};
    DrawRectangleRounded(body, 0.35f, 8, c);
    DrawTriangle((Vector2){x + size * 0.22f, body.y + body.height - 1.0f},
                 (Vector2){x + size * 0.12f, y + size * 0.92f},
                 (Vector2){x + size * 0.46f, body.y + body.height - 1.0f}, c);
    const Color ink = Fade(BLACK, 0.55f);
    for (int i = 0; i < 3; ++i)
        DrawRectangle((int)(x + size * 0.16f), (int)(body.y + size * (0.13f + 0.15f * (float)i)),
                      (int)(size * (i == 2 ? 0.42f : 0.68f)), (int)(size * 0.07f) + 1, ink);
}

/* Icone de cada aba, na ordem de `tabs`. */
static void Modding_DrawTabIcon(int tab, float x, float y, float size, Color c)
{
    switch (tab) {
        case 0: Modding_DrawMusicNote(x + size * 0.15f, y, size, c); break;
        case 1: Modding_DrawPerson(x, y, size, c);      break;
        case 2: Modding_DrawStageIcon(x, y, size, c);   break;
        case 3: Modding_DrawWeekIcon(x, y, size, c);    break;
        case 4: Modding_DrawEffectsIcon(x, y, size, c); break;
        case 5: Modding_DrawSplashIcon(x, y, size, c);  break;
        case TAB_DIALOGUE: Modding_DrawDialogueIcon(x, y, size, c); break;
        case TAB_HUD: Modding_DrawHudIcon(x, y + size * 0.1f, size, c); break;
        default: Modding_DrawDownload(x, y, size, c);   break;
    }
}

static const char *const tabDesc[TAB_COUNT] = {
    "Place notes, events and BPM changes for any song.",
    "Edit animations, offsets, health icons and colors of characters.",
    "Arrange stage layers, props and camera positions.",
    "Build story mode weeks: songs, characters and title art.",
    "Create and tune visual effects used during songs.",
    "Customize the splash shown when hitting a Sick note.",
    "Write Psych dialogues (dialogue.json) with portraits, box style and typing preview.",
    "Move and style the health bar, score text and other HUD parts.",
    "Copy mods from a folder into assets/mods."
};

/* Cor de cada ferramenta (puxada das setas do FNF); a faixa, o icone e o fundo seguem ela. */
static const Color tabAccent[TAB_COUNT] = {
    {18, 220, 90, 255},   /* chart: verde */
    {194, 75, 153, 255},  /* personagem: roxo */
    {255, 140, 40, 255},  /* palco: laranja */
    {250, 200, 40, 255},  /* semana: amarelo */
    {0, 200, 230, 255},   /* efeitos: ciano */
    {70, 120, 255, 255},  /* splash: azul */
    {255, 120, 200, 255}, /* dialogo: rosa */
    {249, 57, 63, 255},   /* hud: vermelho */
    {170, 120, 255, 255}  /* import: lilas */
};

/* Layout da lista: texto em diagonal (estilo freeplay), item em foco no centro sobre a faixa. */
#define LIST_X     96.0f
#define LIST_CY    380.0f
#define LIST_GAP   66.0f
#define LIST_SLANT 24.0f  /* deslocamento x por item: mesma inclinacao "\" do painel */
#define LIST_BIG   52.0f
#define LIST_SMALL 34.0f
#define PANEL_X    760.0f /* borda esquerda do painel no topo; desce para a direita */
#define PANEL_LEAN 100.0f
static const Rectangle openRect = {830.0f, 120.0f, 420.0f, 480.0f}; /* lado direito todo e clicavel */

/* Fundo em duotone: luminancia da imagem vai de quase preto ao tom da ferramenta, com faixas
 * lentas, sombra na esquerda (lista), vinheta e brilho atras do icone. Um passe, uma leitura. */
static const char *const bgFS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec3 tint;\n"
    "uniform float time;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec2 uv = fragTexCoord;\n"
    "    float l = dot(texture(texture0, uv).rgb, vec3(0.299, 0.587, 0.114));\n"
    "    vec3 c = mix(vec3(0.03, 0.02, 0.05), tint * 0.6, smoothstep(0.08, 0.95, l));\n"
    "    c *= 0.95 + 0.05 * sin((uv.x * 1.78 - uv.y) * 60.0 + time * 0.6);\n"
    "    c *= mix(0.5, 1.0, smoothstep(0.05, 0.6, uv.x));\n"
    "    vec2 v = uv - 0.5;\n"
    "    c *= 1.0 - dot(v, v) * 1.2;\n"
    "    vec2 g = (uv - vec2(0.77, 0.29)) * vec2(1.78, 1.0);\n"
    "    c += tint * 0.3 * (1.0 - smoothstep(0.0, 0.32, length(g)));\n"
    "    finalColor = vec4(c, 1.0);\n"
    "}\n";

static Shader bgFx;
static int bgTintLoc = -1, bgTimeLoc = -1;
static char bgFxOk;

void Modding_LoadFx(void)
{
    if (bgFxOk) return;
    bgFx = LoadShaderFromMemory(NULL, bgFS);
    bgFxOk = IsShaderValid(bgFx) && bgFx.id != rlGetShaderIdDefault();
    bgTintLoc = GetShaderLocation(bgFx, "tint");
    bgTimeLoc = GetShaderLocation(bgFx, "time");
}

void Modding_UnloadFx(void)
{
    if (bgFxOk) UnloadShader(bgFx);
    bgFxOk = 0;
}

/* Cor atual: segue o offset animado, entao troca suave junto com a lista. */
static Color Modding_Accent(void)
{
    int a = (int)floorf(offset);
    if (a < 0) a = 0;
    if (a > TAB_COUNT - 1) a = TAB_COUNT - 1;
    const int b = a + 1 < TAB_COUNT ? a + 1 : a;
    return ColorLerp(tabAccent[a], tabAccent[b], offset - (float)a);
}

static void Modding_DrawTintedBackground(Color accent)
{
    if (!bgFxOk) {
        Modding_DrawBackground();
        DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 140});
        return;
    }
    const Vector3 tint = {accent.r / 255.0f, accent.g / 255.0f, accent.b / 255.0f};
    const float t = (float)GetTime();
    SetShaderValue(bgFx, bgTintLoc, &tint, SHADER_UNIFORM_VEC3);
    SetShaderValue(bgFx, bgTimeLoc, &t, SHADER_UNIFORM_FLOAT);
    BeginShaderMode(bgFx);
    Modding_DrawBackground();
    EndShaderMode();
}

/* Quadrilatero: a=sup.esq, b=sup.dir, c=inf.dir, d=inf.esq. DrawTriangle quer anti-horario na tela. */
static void Modding_Quad(Vector2 a, Vector2 b, Vector2 c, Vector2 d, Color col)
{
    DrawTriangle(a, d, c, col);
    DrawTriangle(a, c, b, col);
}

/* Texto com sombra; outline != 0 contorna em preto (estilo letreiro do FNF). */
static void Modding_Text(const char *s, float x, float y, float size, float spacing, Color c, char outline)
{
    const Color k = {0, 0, 0, (unsigned char)(c.a * 0.75f)};
    if (outline) {
        const float o = size * 0.055f;
        DrawTextEx(mainFont, s, (Vector2){x - o, y - o}, size, spacing, k);
        DrawTextEx(mainFont, s, (Vector2){x + o, y - o}, size, spacing, k);
        DrawTextEx(mainFont, s, (Vector2){x - o, y + o}, size, spacing, k);
    }
    DrawTextEx(mainFont, s, (Vector2){x + 3.0f, y + 4.0f}, size, spacing, k);
    DrawTextEx(mainFont, s, (Vector2){x, y}, size, spacing, c);
}

/* 0..1: quanto a aba i esta "em foco" (segue o offset animado). */
static float Modding_TabFocus(int i)
{
    const float d = fabsf((float)i - offset);
    return d < 1.0f ? 1.0f - d : 0.0f;
}

static float Modding_ItemX(int i) { return LIST_X + ((float)i - offset) * LIST_SLANT; }
static float Modding_ItemY(int i) { return LIST_CY + ((float)i - offset) * LIST_GAP; }

int Modding_TabHit(int i, Vector2 m)
{
    if (fabsf((float)i - offset) > 3.0f) return 0; /* fora da tela/apagado */
    return CheckCollisionPointRec(m, (Rectangle){Modding_ItemX(i) - 20.0f, Modding_ItemY(i) - LIST_GAP * 0.5f,
                                                 560.0f, LIST_GAP});
}

int Modding_OpenHit(Vector2 m)
{
    return CheckCollisionPointRec(m, openRect);
}

/* Largura de cada nome no tamanho grande (texto fixo: mede uma vez). */
static float Modding_TabWidth(int i)
{
    static TextMeasure m[TAB_COUNT];
    return TextMeasure_Get(&m[i], mainFont, tabs[i], LIST_BIG, LIST_BIG * 0.06f).x;
}

/* Faixa inclinada atras do item em foco; a largura acompanha o nome. */
static void Modding_DrawBand(Color accent)
{
    int a = (int)floorf(offset);
    if (a < 0) a = 0;
    if (a > TAB_COUNT - 1) a = TAB_COUNT - 1;
    const int b = a + 1 < TAB_COUNT ? a + 1 : a;
    const float t = offset - (float)a;
    const float w = Modding_TabWidth(a) + (Modding_TabWidth(b) - Modding_TabWidth(a)) * t;
    const float end = LIST_X + w + 34.0f;
    const float top = LIST_CY - 38.0f, bot = LIST_CY + 38.0f;

    Modding_Quad((Vector2){-10.0f, top + 6.0f}, (Vector2){end - 8.0f, top + 6.0f},
                 (Vector2){end + 20.0f, bot + 6.0f}, (Vector2){-10.0f, bot + 6.0f}, (Color){0, 0, 0, 120});
    Modding_Quad((Vector2){-10.0f, top}, (Vector2){end - 14.0f, top},
                 (Vector2){end + 14.0f, bot}, (Vector2){-10.0f, bot}, accent);
}

static void Modding_DrawItem(int i)
{
    const float d = fabsf((float)i - offset);
    const float alpha = 1.0f - d * 0.3f;
    if (alpha <= 0.05f) return;

    const float f = Modding_TabFocus(i);
    const float size = LIST_SMALL + (LIST_BIG - LIST_SMALL) * f;
    const Color c = Fade(WHITE, f > 0.5f ? 1.0f : 0.75f * alpha);
    Modding_Text(tabs[i], Modding_ItemX(i), Modding_ItemY(i) - size * 0.52f, size, size * 0.06f, c, f > 0.5f);
}

/* Lado direito: icone grande, nome e descricao. Entram deslizando ao trocar de aba. */
static void Modding_DrawDetails(Color accent)
{
    const float k = Modding_TabFocus(selected); /* 0 logo apos trocar, 1 quando assentou */
    const float slide = (1.0f - k) * 30.0f;
    const float ca = 0.25f + 0.75f * k;
    const float bob = sinf((float)GetTime() * 2.0f) * 5.0f;
    const float x = 852.0f + slide;

    Modding_Quad((Vector2){PANEL_X, 0.0f}, (Vector2){1280.0f, 0.0f}, (Vector2){1280.0f, 720.0f},
                 (Vector2){PANEL_X + PANEL_LEAN, 720.0f}, (Color){0, 0, 0, 115});
    Modding_Quad((Vector2){PANEL_X - 10.0f, 0.0f}, (Vector2){PANEL_X, 0.0f},
                 (Vector2){PANEL_X + PANEL_LEAN, 720.0f}, (Vector2){PANEL_X + PANEL_LEAN - 10.0f, 720.0f}, accent);

    Modding_DrawTabIcon(selected, x + 48.0f, 138.0f + bob, 150.0f, Fade(BLACK, 0.45f * ca));
    Modding_DrawTabIcon(selected, x + 40.0f, 130.0f + bob, 150.0f, Fade(accent, ca));

    Modding_Text(tabs[selected], x, 340.0f, 42.0f, 2.0f, Fade(WHITE, ca), 0);
    (void)Imp_DrawWrapped(IMP_WRAP_DESC, tabDesc[selected], x + 2.0f, 405.0f, 390.0f, 24.0f,
                          Fade(BLACK, 0.6f * ca));
    (void)Imp_DrawWrapped(IMP_WRAP_DESC, tabDesc[selected], x, 403.0f, 390.0f, 24.0f,
                          Fade((Color){228, 228, 236, 255}, ca));
}

/* Atalho em texto puro: tecla na cor da ferramenta + acao. Devolve o x onde termina. */
static float Modding_Hint(int slot, float x, float y, const char *key, const char *label, Color accent)
{
    static TextMeasure keyM[3], labelM[3];
    const float kw = TextMeasure_Get(&keyM[slot], mainFont, key, 20.0f, 2.0f).x;
    Modding_Text(key, x, y, 20.0f, 2.0f, accent, 0);
    Modding_Text(label, x + kw + 10.0f, y, 20.0f, 2.0f, Fade(WHITE, 0.8f), 0);
    return x + kw + 10.0f + TextMeasure_Get(&labelM[slot], mainFont, label, 20.0f, 2.0f).x + 30.0f;
}

/* Fundo com shader (2 draw calls); formas e texto dividem a textura da fonte: mais 1. */
void Modding_DrawList(void)
{
    const Color accent = Modding_Accent();
    float x;

    Modding_DrawTintedBackground(accent);

    Modding_DrawBand(accent);
    for (int i = 0; i < TAB_COUNT; ++i)
        Modding_DrawItem(i);

    Modding_Text("MODDING", 56.0f, 40.0f, 46.0f, 4.0f, WHITE, 1);

    Modding_DrawDetails(accent);

    x = Modding_Hint(0, 900.0f, 652.0f, "ENTER", "open", accent);
    (void)Modding_Hint(1, x, 652.0f, "ESC", "back", accent);

    Render_StopCamera();
}
