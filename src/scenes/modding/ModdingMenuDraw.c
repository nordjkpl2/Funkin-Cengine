/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * ModdingMenuDraw.c - desenho do menu de modding (lista de abas, tela de import, tela em breve).
 */
#include "scenes/api/FontLoader.h"
#include "scenes/modding/ModdingMenuInternal.h"
#include "data/modimport/ModImport.h"
#include "rendering/TextMeasure.h"
#include "Game.h"

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

#define IMP_CHIP_SLOTS 8

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
    DrawRectangleLinesEx((Rectangle){x, y, size, h}, 2.0f, c);
    DrawRectangle((int)(x + size * 0.18f), (int)(y + h * 0.66f), (int)(size * 0.64f), (int)(size * 0.08f) + 1, c);
    DrawCircleV((Vector2){x + size * 0.44f, y + h * 0.69f}, size * 0.09f, c);
    DrawCircleV((Vector2){x + size * 0.58f, y + h * 0.69f}, size * 0.09f, c);
    DrawRectangle((int)(x + size * 0.38f), (int)(y + h * 0.18f), (int)(size * 0.24f), (int)(size * 0.06f) + 1, c);
}

/* Palco: moldura + cortinas nos cantos + linha do chao. */
static void Modding_DrawStageIcon(float x, float y, float size, Color c)
{
    const float h = size * 0.8f;
    DrawRectangleLinesEx((Rectangle){x, y, size, h}, 2.0f, c);
    DrawTriangle((Vector2){x, y}, (Vector2){x, y + h * 0.7f}, (Vector2){x + size * 0.28f, y}, c);
    DrawTriangle((Vector2){x + size * 0.72f, y}, (Vector2){x + size, y + h * 0.7f}, (Vector2){x + size, y}, c);
    DrawRectangle((int)x, (int)(y + h * 0.78f), (int)size, (int)(size * 0.06f) + 1, c);
}

/* Semana: calendario (cabecalho + grade 3x2). */
static void Modding_DrawWeekIcon(float x, float y, float size, Color c)
{
    const float h = size * 0.85f;
    const float cell = size * 0.16f;
    DrawRectangleLinesEx((Rectangle){x, y, size, h}, 2.0f, c);
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
    "Move and style the health bar, score text and other HUD parts.",
    "Copy mods from a folder into assets/mods."
};

/* Layout da lista: cards a esquerda (rolam com `offset`), painel de detalhes a direita. */
#define CARD_X   60.0f
#define CARD_W   500.0f
#define CARD_H   62.0f
#define CARD_GAP 76.0f
static const Rectangle panelRect = {620.0f, 150.0f, 600.0f, 440.0f};
static const Rectangle openRect  = {650.0f, 510.0f, 540.0f, 56.0f};

/* 0..1: quanto a aba i esta "em foco" (segue o offset animado). */
static float Modding_TabFocus(int i)
{
    const float d = fabsf((float)i - offset);
    return d < 1.0f ? 1.0f - d : 0.0f;
}

static Rectangle Modding_CardRect(int i)
{
    return (Rectangle){CARD_X + Modding_TabFocus(i) * 18.0f,
                       360.0f - CARD_H * 0.5f + ((float)i - offset) * CARD_GAP,
                       CARD_W, CARD_H};
}

int Modding_TabHit(int i, Vector2 m)
{
    return CheckCollisionPointRec(m, Modding_CardRect(i));
}

int Modding_OpenHit(Vector2 m)
{
    return CheckCollisionPointRec(m, openRect);
}

static void Modding_DrawCard(int i)
{
    const Rectangle r = Modding_CardRect(i);
    const float f = Modding_TabFocus(i);
    float a = 1.0f - fabsf((float)i - offset) * 0.2f;
    char num[8];

    if (r.y < -CARD_H || r.y > 720.0f) return;
    if (a < 0.15f) a = 0.15f;

    DrawRectangleRounded(r, 0.25f, 8, Fade((Color){12, 12, 20, 255}, 0.75f * a));
    if (f > 0.0f) {
        DrawRectangleRounded(r, 0.25f, 8, Fade(selectedColor, 0.22f * f));
        DrawRectangleRoundedLinesEx(r, 0.25f, 8, 2.0f, Fade(selectedColor, f));
        DrawRectangleRounded((Rectangle){r.x - 14.0f, r.y + 12.0f, 6.0f, r.height - 24.0f}, 1.0f, 4,
                             Fade(selectedColor, f));
    } else {
        DrawRectangleRoundedLinesEx(r, 0.25f, 8, 1.0f, Fade(WHITE, 0.12f * a));
    }

    {
        const Rectangle ib = {r.x + 10.0f, r.y + 9.0f, 44.0f, 44.0f};
        const Color ic = f > 0.5f ? selectedColor : Fade(fontColor, a);
        DrawRectangleRounded(ib, 0.3f, 6, f > 0.5f ? Fade(selectedColor, 0.18f) : Fade(WHITE, 0.07f * a));
        Modding_DrawTabIcon(i, ib.x + 9.0f, ib.y + 9.0f, 26.0f, ic);
    }

    DrawTextEx(mainFont, tabs[i], (Vector2){r.x + 70.0f, r.y + 15.0f}, 32.0f, 2.0f,
               f > 0.5f ? fontColor : Fade(fontColor, 0.7f * a));
    snprintf(num, sizeof(num), "%02d", i + 1);
    DrawTextEx(mainFont, num, (Vector2){r.x + r.width - 48.0f, r.y + 19.0f}, 24.0f, 1.0f,
               Fade((Color){150, 150, 160, 255}, a));
}

static void Modding_DrawDetails(void)
{
    const Rectangle p = panelRect;
    const float k = Modding_TabFocus(selected); // 0 logo apos trocar de aba, 1 quando assentou
    const float slide = (1.0f - k) * 24.0f;
    const float ca = 0.3f + 0.7f * k;
    char line[32];

    /* cabecalho da tela */
    DrawTextEx(mainFont, "MODDING", (Vector2){p.x, 52.0f}, 56.0f, 4.0f, fontColor);
    DrawTextEx(mainFont, "Choose a tool to create or edit content", (Vector2){p.x + 2.0f, 112.0f}, 20.0f, 1.0f,
               (Color){150, 150, 160, 255});

    DrawRectangleRounded(p, 0.06f, 8, Fade((Color){10, 10, 16, 255}, 0.82f));
    DrawRectangleRoundedLinesEx(p, 0.06f, 8, 2.0f, Fade(selectedColor, 0.55f));

    {
        const Rectangle ib = {p.x + 30.0f + slide, p.y + 30.0f, 130.0f, 130.0f};
        DrawRectangleRounded(ib, 0.2f, 8, Fade(selectedColor, 0.16f * ca));
        DrawRectangleRoundedLinesEx(ib, 0.2f, 8, 2.0f, Fade(selectedColor, ca));
        Modding_DrawTabIcon(selected, ib.x + 27.0f, ib.y + 27.0f, 76.0f, Fade(selectedColor, ca));
    }
    DrawTextEx(mainFont, tabs[selected], (Vector2){p.x + 190.0f + slide, p.y + 52.0f}, 44.0f, 3.0f,
               Fade(fontColor, ca));
    snprintf(line, sizeof(line), "TOOL %02d / %02d", selected + 1, TAB_COUNT);
    DrawTextEx(mainFont, line, (Vector2){p.x + 192.0f + slide, p.y + 108.0f}, 20.0f, 2.0f,
               Fade(selectedColor, ca));

    DrawRectangle((int)p.x + 30, (int)p.y + 190, (int)p.width - 60, 1, (Color){255, 255, 255, 40});
    (void)Imp_DrawWrapped(IMP_WRAP_DESC, tabDesc[selected], p.x + 30.0f + slide, p.y + 214.0f,
                          p.width - 60.0f, 26.0f, Fade(fontColor, 0.9f * ca));

    /* botao abrir (clicavel, ver Modding_OpenHit) */
    {
        static TextMeasure openM[2]; // "OPEN" e "IMPORT"
        const int isImport = selected == TAB_IMPORT;
        const char *label = isImport ? "IMPORT" : "OPEN";
        const float tw = TextMeasure_Get(&openM[isImport], mainFont, label, 30.0f, 3.0f).x;
        const char hover = Modding_OpenHit(RayGame_MouseWorld());
        const float pulse = 0.85f + 0.15f * sinf((float)GetTime() * 4.0f);

        DrawRectangleRounded(openRect, 0.35f, 8, Fade(selectedColor, hover ? 1.0f : pulse));
        DrawTextEx(mainFont, label, (Vector2){openRect.x + (openRect.width - tw) * 0.5f, openRect.y + 13.0f},
                   30.0f, 3.0f, (Color){8, 24, 12, 255});
    }
}

/* Formas e texto dividem a textura (Modding_BatchWith): tudo sai num draw call. */
void Modding_DrawList(void)
{
    float x;

    Modding_DrawBackground();
    DrawRectangle(0, 0, 1280, 720, (Color){0, 0, 0, 110});
    DrawRectangleGradientH(0, 0, 640, 720, (Color){0, 0, 0, 170}, (Color){0, 0, 0, 0});

    for (int i = 0; i < TAB_COUNT; ++i)
        Modding_DrawCard(i);

    Modding_DrawDetails();

    x = Imp_Chip(5, panelRect.x, 630.0f, "UP/DOWN", "Select", selectedColor, 1);
    x = Imp_Chip(6, x, 630.0f, "ENTER", "Open", selectedColor, 1);
    (void)Imp_Chip(7, x, 630.0f, "ESC", "Back", selectedColor, 1);

    Render_StopCamera();
}
