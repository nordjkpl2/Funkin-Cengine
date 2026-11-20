/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * ModdingMenuDraw.c - desenho do menu de modding (lista de abas, tela de import, tela em breve).
 */
#include "scenes/api/FontLoader.h"
#include "scenes/menu/modding/ModdingMenuInternal.h"
#include "data/modimport/ModImport.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void Modding_DrawBackground(void)
{
    Render_SetCamera(&cam);
    Render_DrawGraphicObject(&bg);
}

/* Icones desenhados mais abaixo. */
static void Modding_DrawMusicNote(float x, float y, float size, Color c);
static void Modding_DrawPerson(float x, float y, float size, Color c);
static void Modding_DrawDownload(float x, float y, float size, Color c);

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
enum { IMP_WRAP_INTRO, IMP_WRAP_DEST, IMP_WRAP_PROGRESS, IMP_WRAP_STATUS, IMP_WRAP_SLOTS };

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

/* Tecla + rotulo; devolve o x onde termina. */
static float Imp_Chip(float x, float y, const char *key, const char *label,
                      Color accent, char active)
{
    const float kw = MeasureTextEx(mainFont, key, 17, 1).x + 20;
    const Color text = active ? fontColor : (Color){130, 130, 140, 255};

    DrawRectangle((int)x, (int)y, (int)kw, 30, active ? (Color){accent.r, accent.g, accent.b, 70} : (Color){40, 40, 48, 255});
    DrawRectangleLinesEx((Rectangle){x, y, kw, 30}, 1, active ? accent : (Color){70, 70, 80, 255});
    DrawTextEx(mainFont, key, (Vector2){x + 10, y + 6}, 17, 1, text);
    DrawTextEx(mainFont, label, (Vector2){x + kw + 8, y + 5}, 19, 1, text);
    return x + kw + 8 + MeasureTextEx(mainFont, label, 19, 1).x + 22;
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
                tx += MeasureTextEx(mainFont, "...", 20, 1).x + 4;
            }
            DrawTextEx(mainFont, shown, (Vector2){tx, box.y + 12}, 20, 1, WHITE);
            tx += MeasureTextEx(mainFont, shown, 20, 1).x + 3;
        } else {
            DrawTextEx(mainFont, "C:/.../mod-folder", (Vector2){tx, box.y + 12}, 20, 1, (Color){110, 110, 120, 255});
        }
        if (((int)(GetTime() * 2.0)) & 1)
            DrawRectangle((int)tx, (int)box.y + 10, 2, 26, fontColor);
    }
    x = left.x + 20;
    y = left.y + 156;
    x = Imp_Chip(x, y, "ENTER", "Import", selectedColor, impPath[0] != '\0' && !importing);
    x = Imp_Chip(x, y, "CTRL+V", "Paste path", selectedColor, 1);
    (void)Imp_Chip(x, y, "BKSP", "Erase", selectedColor, impPath[0] != '\0');

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
                   (Vector2){1230 - MeasureTextEx(mainFont, msg, 19, 1).x, 586}, 19, 1, muted);
    } else if (impLastValid && !busy) {
        Imp_FormatSize(impLast.bytes, a, sizeof(a));
        snprintf(msg, sizeof(msg), "Last import: %s", a);
        DrawTextEx(mainFont, msg,
                   (Vector2){1230 - MeasureTextEx(mainFont, msg, 19, 1).x, 586}, 19, 1, muted);
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
    x = Imp_Chip(50, 650, "ESC", importing ? "Cancel import" : "Back", selectedColor, 1);
    (void)Imp_Chip(x, 650, "DELETE x2", impConfirmDel ? "Press again to remove all mods" : "Remove all mods",
                   warning, impMods > 0 && impPath[0] == '\0' && !importing);

    Render_StopCamera();
}

void Modding_DrawStub(void)
{
    const char *message = "COMING SOON";
    const Vector2 center = {640.0f, 360.0f};
    const Vector2 size = MeasureTextEx(mainFont, message, 80, 5);

    Modding_DrawBackground();
    DrawTextEx(mainFont, message,
               (Vector2){center.x - size.x * 0.5f,
                         center.y - size.y * 0.5f},
               80, 5, fontColor);

    if (stubTitle) {
        const Vector2 sub = MeasureTextEx(mainFont, stubTitle, 32, 4);
        DrawTextEx(mainFont, stubTitle,
                   (Vector2){center.x - sub.x * 0.5f, center.y + 80.0f},
                   32, 4, fontColor);
    }

    Render_StopCamera();
}

/* Nota musical (colcheia) feita de elipse + retangulo + triangulo; (x,y) = canto superior esquerdo. */
static void Modding_DrawMusicNote(float x, float y, float size, Color c)
{
    const float headW = size * 0.30f;
    const float headH = size * 0.22f;
    const float stemW = size * 0.08f;
    const float stemX = x + headW * 1.8f;
    const float headY = y + size - headH;

    DrawEllipse((int)(x + headW), (int)headY, headW, headH, c);
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

/* Icone de cada aba, na ordem de `tabs`. */
static void Modding_DrawTabIcon(int tab, float x, float y, float size, Color c)
{
    switch (tab) {
        case 0: Modding_DrawMusicNote(x, y, size, c); break;
        case 1: Modding_DrawPerson(x, y, size, c);    break;
        case 2:
        case 3: DrawRectangleLinesEx((Rectangle){x, y, size, size * 0.75f}, 2.0f, c); break;
        default: Modding_DrawDownload(x, y, size, c); break;
    }
}

void Modding_DrawList(void)
{
    Modding_DrawBackground();

    for (int i = 0; i < TAB_COUNT; ++i) {
        const float distance = (float)i - offset;
        const float absDistance = (distance < 0.0f) ? -distance : distance;
        const int selectedItem = (selected == i);
        Color textColor = fontColor;
        Color shadowColor = outlineColor;
        const float fontSize = 80.0f;
        const float shadow = 5.0f;
        Vector2 position = {
            shadow + 150.0f - absDistance * 30.0f,
            shadow + 360.0f + distance * 90.0f
        };

        // Selecionado em verde, igual ao menu principal.
        if (selectedItem)
            textColor = selectedColor;
        else
            textColor.a = 150;
        shadowColor.a = selectedItem ? 255 : 100;

        const float iconX = position.x - shadow + MeasureTextEx(mainFont, tabs[i], fontSize, 5).x + 24.0f;
        DrawTextEx(mainFont, tabs[i], position, fontSize, 5, shadowColor);
        Modding_DrawTabIcon(i, iconX + shadow, position.y + 6.0f, fontSize * 0.8f, shadowColor);
        position.x -= shadow;
        position.y -= shadow;
        DrawTextEx(mainFont, tabs[i], position, fontSize, 5, textColor);
        Modding_DrawTabIcon(i, iconX, position.y + 6.0f, fontSize * 0.8f, textColor);
    }

    Render_StopCamera();
}

