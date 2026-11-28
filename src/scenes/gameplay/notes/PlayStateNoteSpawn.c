/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * PlayStateNoteSpawn.c - notas da musica: carregar, atualizar e desenhar.
 */

#include "scenes/api/NoteSkinCache.h"
#include "scenes/gameplay/notes/PlayStateNoteAtlas.h"
#include "scenes/gameplay/notes/PlayStateNoteSpawn.h"
#include "data/song/SongStream.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "GameData.h"
#include "scenes/gameplay/notes/PlayStatePlayerInput.h"
#include "logic/notes/NoteTypes.h"
#include "core/ModPath.h"
#include "core/Log.h"
#include "rlgl.h"

static RayAnimatedObject trail;
static RayAnimatedObject end;
static RayAnimatedObject renderNote;
static RayAnimationState laneAnimState[4];   /* animacao atual de cada lane (renderNote e compartilhado) */

/* `notes` guarda todas as notas (modo completo) ou so a janela carregada (modo streaming).
   noteCount, firstNote e os acessos publicos sao sempre relativos a esse array. */
static Note* notes;
static size_t noteCount;
static size_t firstNote;
static size_t noteCapacity;

/* Modo streaming (ver data/SongStream.h): as secoes entram na janela conforme o tempo avanca. */
static SongStream* stream = NULL;
static int streamNext = 0;             /* proxima secao a carregar */
static float streamLookahead = 4.0f;   /* segundos a frente do tempo que ja devem estar carregados */
#define STREAM_MARGIN_S 2.0f           /* folga alem do cutoff das notas (evita carregar tarde) */
#define STREAM_COMPACT_MIN 256         /* so tira notas resolvidas do inicio da janela a partir deste numero */
#define STREAM_MS_TO_S 0.001f

static PlayStateNoteEvents events;

/* Textura propria de um tipo de nota (Psych 'texture'): animacoes achadas na skin dele. */
typedef struct {
    const RayAnimationHandler* set;
    int head[4], trail[4], end[4];
} TypeSkin;
#define TYPE_SKIN_MAX 8
static TypeSkin typeSkins[TYPE_SKIN_MAX];
static int typeSkinCount;
static RayAnimationHandler songSkin;           /* skin da musica (trail/end voltam a ela depois de um tipo com textura) */
static signed char typeSkinOf[NOTETYPE_MAX];   /* tipo -> typeSkins (-1 = skin da musica) */
static unsigned char typeTint[NOTETYPE_MAX];   /* 1 = sem textura: cor de nota de dano */

// sustain colors
static const Color normalColor = {255, 255, 255, 200};
static const Color missedColor = {150, 150, 150, 200};
static const Vector2 trailOffset = {40, 60};

#define NOTE_SCALE 0.7f
#define NOTE_OCC_TOP 300                    /* margem acima da tela (px) */
#define NOTE_OCC_ROWS (300 + 720 + 300)     /* linhas de pixel cobertas pela grade de ocupacao */
static unsigned short occupied[8][NOTE_OCC_ROWS];  /* (lado, casa) x linha de pixel; marcada com a geracao do Draw */
static unsigned short occupiedGen = 0;             /* geracao atual; evita memset por frame */
#define TRAIL_SEGMENT_H 44.0f /* altura do sprite do sustain na escala 1 */
#define END_SEGMENT_H 64.0f   /* altura do sprite da ponta na escala 1 */
/* Teto de objetos (cabeca, corpo e ponta) desenhados por frame. As notas vem em ordem de tempo,
   entao o corte so pega as mais distantes: chart absurdo (spam de sustains) nao explode em draw calls. */
#define NOTE_DRAW_BUDGET 2048

// Ordena por tempo (qsort): garante notas em ordem para o culling.
static int Note_CompareChronological(const void* a, const void* b) {
    float diff = ((const Note*)a)->time - ((const Note*)b)->time;
    if (diff < 0) return -1;
    if (diff > 0) return 1;
    return 0;
}

void PlayStateNoteSpawn_Init(void) {
    memset(typeSkinOf, -1, sizeof(typeSkinOf));   /* sem LoadTypes: todos com a skin da musica */
    memset(typeTint, 0, sizeof(typeTint));
    Render_DefaultAnimated(&trail);
    trail.animationSet = Cache_GetNoteAnimations();
    trail.scaleX = 0.7f;

    Render_DefaultAnimated(&end);
    end.animationSet = Cache_GetNoteAnimations();
    end.scaleX = 0.7f;
    end.scaleY = 0.7f;

    Render_DefaultAnimated(&renderNote);
    memset(laneAnimState, 0, sizeof(laneAnimState));
    renderNote.animationSet = Cache_GetNoteAnimations();
    renderNote.scaleX = 0.7f;
    renderNote.scaleY = 0.7f;
}

void PlayStateNoteSpawn_SetEvents(const PlayStateNoteEvents* newEvents) {
    events = *newEvents;
}

/* Offset HSB de cada lane fixo durante a musica: lido uma vez aqui, nao por nota/frame. */
static Vector3 laneShift[4];

static bool IsIdentityShift(Vector3 s) {
    return s.x == 0.0f && s.y == 0.0f && s.z == 0.0f;
}

/* Janela de notas por secao (modo streaming). Ver a descricao em PlayStateNoteSpawn.h. */

/* Garante espaco para `extra` notas no fim da janela. */
static void StreamReserve(size_t extra) {
    size_t cap;
    Note* grown;
    if (noteCount + extra <= noteCapacity) return;
    cap = noteCapacity > 0 ? noteCapacity : 1024;
    while (cap < noteCount + extra) cap *= 2;
    grown = realloc(notes, sizeof(Note) * cap);
    if (grown == NULL)
        Log_FatalF("PlayStateNoteSpawn: sem memoria para a janela de notas (%zu)", cap);
    notes = grown;
    noteCapacity = cap;
}

/* Tira do inicio da janela as notas ja resolvidas, quando ja sao muitas e pelo menos metade da janela. */
static void StreamCompact(void) {
    if (firstNote < STREAM_COMPACT_MIN || firstNote * 2 < noteCount) return;
    memmove(notes, notes + firstNote, sizeof(Note) * (noteCount - firstNote));
    noteCount -= firstNote;
    firstNote = 0;
}

/* Converte a secao k e a poe no fim da janela. Notas com tempo < resolveBefore (s) viram resolvidas
   (so o seek usa isso; no andamento normal fica -1: nada resolvido). */
static void StreamLoadSection(int k, float resolveBefore) {
    DataNote* data = NULL;
    char mustHit = 0;
    const int count = SongStream_LoadSection(stream, k, &data, &mustHit);
    if (count <= 0) {
        free(data);
        return;
    }
    StreamReserve((size_t)count);
    for (int j = 0; j < count; j++) {
        Note* note = notes + noteCount;
        Note_Load(note, data + j);
        note->sectionMustHit = mustHit != 0;
        note->pressed = note->time < resolveBefore;
        noteCount++;
    }
    free(data);
}

/* Carrega, em ordem, as secoes que comecam ate untilSec (s). */
static void StreamFeed(float untilSec, float resolveBefore) {
    const int total = SongStream_SectionCount(stream);
    while (streamNext < total &&
           SongStream_SectionLoadMs(stream, streamNext) * STREAM_MS_TO_S <= untilSec) {
        StreamLoadSection(streamNext, resolveBefore);
        streamNext++;
    }
}

static int FindTypeSkin(const RayAnimationHandler* set) {
    for (int i = 0; i < typeSkinCount; i++)
        if (typeSkins[i].set == set) return i;
    if (typeSkinCount >= TYPE_SKIN_MAX) return -1;
    static const char* const heads[4] = {"purple", "blue", "green", "red"};
    static const char* const trails[4] = {"purple hold piece", "blue hold piece", "green hold piece", "red hold piece"};
    static const char* const ends[4] = {"purple hold end", "blue hold end", "green hold end", "red hold end"};
    TypeSkin* s = &typeSkins[typeSkinCount];
    s->set = set;
    for (int l = 0; l < 4; l++) {
        s->head[l] = AnimationSet_FindAnimation(set, heads[l]);
        s->trail[l] = AnimationSet_FindAnimation(set, trails[l]);
        s->end[l] = AnimationSet_FindAnimation(set, ends[l]);
    }
    return typeSkinCount++;
}

/* A textura existe em images/ (mod da musica ou assets)? Evita o aviso de skin ausente para a Hurt Note. */
static bool TypeTextureExists(const char* texture, const char* songDir) {
    char modRoot[512] = "", rel[160], out[512];
    if (songDir != NULL && !ModPath_Root(songDir, modRoot, sizeof(modRoot))) modRoot[0] = 0;
    if (snprintf(rel, sizeof(rel), "images/%s.png", texture) >= (int)sizeof(rel)) return false;
    return ModPath_Find(modRoot, rel, out, sizeof(out));
}

void PlayStateNoteSpawn_LoadTypes(const char* songDir) {
    NoteTypes_LoadScripts(songDir);
    typeSkinCount = 0;
    const RayAnimationHandler* fallback = Cache_GetDefaultNoteSkinPtr();
    const int count = NoteTypes_Count();
    for (int i = 0; i < NOTETYPE_MAX; i++) {
        typeSkinOf[i] = -1;
        typeTint[i] = 0;
        if (i >= count) continue;
        const NoteTypeInfo* t = NoteTypes_Get(i);
        if (t->texture[0] != 0 && (t->scripted || TypeTextureExists(t->texture, songDir))) {
            char name[80];
            snprintf(name, sizeof(name), "@%s", t->texture);
            const RayAnimationHandler* set = Cache_GetNoteSkinPtr(name, songDir);
            if (set != NULL && set != fallback && set->animationCount > 0) {
                typeSkinOf[i] = (signed char)FindTypeSkin(set);
                if (typeSkinOf[i] >= 0) continue;
            }
        }
        typeTint[i] = t->hurtColor;
    }
}

void PlayStateNoteSpawn_Load(Song* song) {
    firstNote = 0;
    noteCount = 0;
    noteCapacity = 0;
    for (size_t i = 0; i < (size_t)song->sectionCount; i++)
        noteCount += song->sections[i].noteCount;

    /* Skin de mod tem cor propria: sem ajuste HSB. */
    const bool customSkin = Note_IsCustomSkin();
    for (int lane = 0; lane < 4; lane++)
        laneShift[lane] = customSkin ? (Vector3) {0.0f, 0.0f, 0.0f} : GameData_LaneShift(lane);
    {
        /* Mesma skin dos receptores (os indices de Note_Animation sao dela). */
        const RayAnimationHandler skin = *Note_GetCurrentSkin();
        songSkin = skin;
        renderNote.animationSet = trail.animationSet = end.animationSet = skin;
        memset(laneAnimState, 0, sizeof(laneAnimState));
        NoteAtlas_Build(&skin, laneShift);   /* false = sem ajuste de cor ou skin grande: cai no shader */
    }

    /* Streaming: a gameplay fica com o stream; as notas entram pelo Update (StreamFeed). */
    if (song->stream != NULL) {
        if (stream != NULL) SongStream_Close(stream);
        stream = song->stream;
        song->stream = NULL;
        streamNext = 0;
        streamLookahead = 4.0f;
        notes = NULL;
        return;
    }

    notes = NULL;
    if (noteCount == 0) return;

    notes = malloc(sizeof(Note) * noteCount);
    if (notes == NULL)
        Log_FatalF("PlayStateNoteSpawn_Load: sem memoria para %zu notas", noteCount);
    noteCapacity = noteCount;

    size_t noteIndex = 0;
    char sorted = 1;

    for (size_t i = 0; i < song->sectionCount; i++) {
        for (size_t j = 0; j < song->sections[i].noteCount; j++) {
            Note* note = notes + noteIndex;
            Note_Load(note, song->sections[i].notes + j);
            note->sectionMustHit = song->sections[i].mustHit != 0;
            if (noteIndex > 0 && note->time < notes[noteIndex - 1].time)
                sorted = 0;
            noteIndex++;
        }
    }

    // Ordenar e critico (culling e break do Draw assumem ordem cronologica), mas so quando preciso:
    // charts normais ja vem em ordem e pulam o qsort.
    if (!sorted)
        qsort(notes, noteCount, sizeof(Note), Note_CompareChronological);
}

void PlayStateNoteSpawn_Reset(void) {
    firstNote = 0;

    if (stream != NULL) {   /* restart: a janela volta ao inicio; o Update carrega de novo */
        noteCount = 0;
        streamNext = 0;
        return;
    }

    for (size_t i = 0; i < noteCount; i++) {
        Note* note = notes + i;
        note->missed = 0;
        note->pressed = 0;
        note->skipped = 0;
    }
}

/* Seek para o tempo `time`. Regra de pontuacao: notas com cabeca antes do alvo ficam
   "resolvidas" (pressed), sem acerto, miss ou score. As do alvo em diante voltam a zero.
   firstNote vai para a primeira nota que ainda pode estar ativa (sustain que cruza o alvo incluso). */
void PlayStateNoteSpawn_SeekTo(float time) {
    if (stream != NULL) {
        /* Mesma regra da versao completa, por secao: a primeira secao cujo maior fim ainda
           alcanca tempo - 0.4 s. As anteriores so tem notas ja resolvidas (nunca sao carregadas). */
        const int total = SongStream_SectionCount(stream);
        int k = 0;
        while (k < total && SongStream_SectionMaxEndMs(stream, k) * STREAM_MS_TO_S < time - 0.4f) k++;
        noteCount = 0;
        firstNote = 0;
        streamNext = k;
        StreamFeed(time + streamLookahead, time);
        return;
    }

    size_t first = noteCount;

    for (size_t i = 0; i < noteCount; i++) {
        Note* note = notes + i;
        note->pressed = note->time < time;
        note->missed = 0;
        note->skipped = 0;

        if (first == noteCount && note->time + note->length >= time - 0.4f)
            first = i;
    }

    firstNote = first;
}

static void UnloadRecolorShader(void);
static void UnloadHurtShader(void);

void PlayStateNoteSpawn_Unload(void) {
    NoteAtlas_Free();
    UnloadRecolorShader();
    UnloadHurtShader();
    free(notes);
    notes = NULL;
    noteCount = 0;
    noteCapacity = 0;
    firstNote = 0;
    if (stream != NULL) {
        SongStream_Close(stream);
        stream = NULL;
    }
    streamNext = 0;
}

Note* PlayStateNoteSpawn_Notes(void) { return notes; }
size_t PlayStateNoteSpawn_Count(void) { return noteCount; }
size_t PlayStateNoteSpawn_First(void) { return firstNote; }

/* Escala da cabeca da nota conforme o modo de scroll. */
static float HeadScale(const Note* note) {
    if (SCROLL_IS_MIDDLE(gameOptions.scroll) && !note->mustHit) return 0.4f;
    return NOTE_SCALE;
}

static void SetAnimIfChanged(RayAnimatedObject* object, int anim) {
    if (!object->currentAnim.isValid || object->currentAnim.animationIndex != anim)
        AnimatedObject_SetAnimation(object, anim);
}

/* Sustain ainda nao pressionado/perdido: desenha inteiro, sem clip. */
/* Recolor da skin (estilo Psych): soma um offset de matiz, saturacao e brilho a cada pixel.
   Offset 0 = skin original. Mesma conta de PlayStateNoteAtlas.c (ShiftRow), para atlas e shader baterem. */
static const char* const recolorFS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform vec3 shiftHSB;\n"   /* x = matiz/360, y = saturacao/100, z = brilho/100 (somados) */
    "out vec4 finalColor;\n"
    "vec3 rgb2hsv(vec3 c) {\n"
    "    vec4 K = vec4(0.0, -1.0/3.0, 2.0/3.0, -1.0);\n"
    "    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));\n"
    "    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));\n"
    "    float d = q.x - min(q.w, q.y);\n"
    "    float e = 1.0e-10;\n"
    "    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);\n"
    "}\n"
    "vec3 hsv2rgb(vec3 c) {\n"
    "    vec4 K = vec4(1.0, 2.0/3.0, 1.0/3.0, 3.0);\n"
    "    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);\n"
    "    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);\n"
    "}\n"
    "void main() {\n"
    "    vec4 texel = texture(texture0, fragTexCoord) * colDiffuse * fragColor;\n"
    "    vec3 hsv = rgb2hsv(texel.rgb);\n"
    "    hsv.x = fract(hsv.x + shiftHSB.x);\n"
    "    hsv.y = clamp(hsv.y + shiftHSB.y, 0.0, 1.0);\n"
    "    hsv.z = clamp(hsv.z + shiftHSB.z, 0.0, 1.0);\n"
    "    finalColor = vec4(hsv2rgb(hsv), texel.a);\n"
    "}\n";

static Shader recolorShader;
static int recolorLoc = -1;
static bool recolorReady;

static void EnsureRecolorShader(void) {
    if (recolorReady) return;
    recolorShader = LoadShaderFromMemory(NULL, recolorFS);
    recolorLoc = GetShaderLocation(recolorShader, "shiftHSB");
    recolorReady = true;
}

static void UnloadRecolorShader(void) {
    if (!recolorReady) return;
    UnloadShader(recolorShader);
    recolorReady = false;
}

/* Hurt Note sem textura HURTNOTE_assets: mesma ideia da paleta RGB da Psych 0.7
   (miolo preto 0x101010, borda branca -> vermelho 0xFF0000, contorno escuro -> 0x990022). */
static const char* const hurtFS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 t = texture(texture0, fragTexCoord) * colDiffuse * fragColor;\n"
    "    float mx = max(t.r, max(t.g, t.b));\n"
    "    float sat = mx > 0.0 ? (mx - min(t.r, min(t.g, t.b))) / mx : 0.0;\n"
    "    vec3 c = mix(vec3(1.0, 0.0, 0.0), vec3(0.063), smoothstep(0.15, 0.55, sat));\n"
    "    c = mix(vec3(0.6, 0.0, 0.133), c, smoothstep(0.15, 0.5, mx));\n"
    "    finalColor = vec4(c, t.a);\n"
    "}\n";

static Shader hurtShader;
static bool hurtReady;

static void UnloadHurtShader(void) {
    if (!hurtReady) return;
    UnloadShader(hurtShader);
    hurtReady = false;
}

/* Desenha `obj` com o conjunto de animacao `set` (a textura do atlas), sem mexer no resto do objeto. */
static void DrawInSet(RayAnimatedObject* obj, const RayAnimationHandler* set) {
    const RayAnimationHandler saved = obj->animationSet;
    obj->animationSet = *set;
    Render_DrawAnimatedObject(obj);
    obj->animationSet = saved;
}

/* Desenha a(s) parte(s) de uma nota da lane `lane` (b pode ser NULL; a e b juntos = sustain).
   Com atlas: cada objeto usa a variante da lane na mesma textura, entao o lote segue unico.
   Sem atlas: lane branca desenha direto; lane colorida usa o shader de matiz, com um
   begin/end so para as partes (dedup: cabeca/sustain nao repetem a troca de shader). */
static void DrawLaneObjs(int lane, RayAnimatedObject* a, RayAnimatedObject* b) {
    if (a == NULL && b == NULL) return;
    if (lane < 0 || lane > 3) return;

    const RayAnimationHandler* variant = NoteAtlas_Lane(lane);
    if (variant != NULL) {
        if (a != NULL) DrawInSet(a, variant);
        if (b != NULL) DrawInSet(b, variant);
        return;
    }

    if (IsIdentityShift(laneShift[lane])) {
        if (a != NULL) Render_DrawAnimatedObject(a);
        if (b != NULL) Render_DrawAnimatedObject(b);
        return;
    }

    EnsureRecolorShader();
    rlDrawRenderBatchActive();   /* o lote pendente usa o uniform antigo */
    SetShaderValue(recolorShader, recolorLoc, &laneShift[lane], SHADER_UNIFORM_VEC3);
    BeginShaderMode(recolorShader);
    if (a != NULL) Render_DrawAnimatedObject(a);
    if (b != NULL) Render_DrawAnimatedObject(b);
    EndShaderMode();
}

static bool shiftActive;

void PlayStateNoteSpawn_BeginShift(Vector3 shift) {
    shiftActive = !IsIdentityShift(shift);
    if (!shiftActive) return;
    EnsureRecolorShader();
    rlDrawRenderBatchActive();   /* o lote pendente usa o uniform antigo */
    SetShaderValue(recolorShader, recolorLoc, &shift, SHADER_UNIFORM_VEC3);
    BeginShaderMode(recolorShader);
}

void PlayStateNoteSpawn_EndShift(void) {
    if (shiftActive) EndShaderMode();
    shiftActive = false;
}

/* Menu de opcoes (preview): mesmo desenho de nota com um offset HSB qualquer (sempre shader, sem atlas). */
void PlayStateNoteSpawn_DrawShifted(RayAnimatedObject* obj, Vector3 shift) {
    PlayStateNoteSpawn_BeginShift(shift);
    Render_DrawAnimatedObject(obj);
    PlayStateNoteSpawn_EndShift();
}

void PlayStateNoteSpawn_ReleaseTint(void) {
    UnloadRecolorShader();
    UnloadHurtShader();
}

/* Partes de uma nota pelo tipo dela: cor de dano (shader), textura do tipo (direto) ou cor da lane. */
static void DrawNoteObjs(const Note* note, RayAnimatedObject* a, RayAnimatedObject* b) {
    if (typeTint[note->type]) {
        if (!hurtReady) {
            hurtShader = LoadShaderFromMemory(NULL, hurtFS);
            hurtReady = true;
        }
        BeginShaderMode(hurtShader);
        if (a != NULL) Render_DrawAnimatedObject(a);
        if (b != NULL) Render_DrawAnimatedObject(b);
        EndShaderMode();
        return;
    }
    if (typeSkinOf[note->type] >= 0) {
        if (a != NULL) Render_DrawAnimatedObject(a);
        if (b != NULL) Render_DrawAnimatedObject(b);
        return;
    }
    DrawLaneObjs(note->id, a, b);
}

static void DrawSustainFull(const Note* note, char fullyAbove) {
    if (fullyAbove) return;
    DrawNoteObjs(note, &trail, &end);
}

/* Sustain em andamento: corta a parte que ja passou do receptor. */
static void DrawSustainClipped(const Note* note, const StrumNote* strum, float k) {
    const float receptorY = strum->object.position.y + trailOffset.y * k;

    if (SCROLL_IS_DOWN(gameOptions.scroll)) {
        // Downscroll: a cauda fica acima da cabeca; o que ja passou do receptor (abaixo dele) some.
        const float trailBottom = trail.position.y + trail.scaleY * TRAIL_SEGMENT_H;
        if (trailBottom > receptorY)
            trail.scaleY = fmaxf(0.0f, (receptorY - trail.position.y) / TRAIL_SEGMENT_H);
        DrawNoteObjs(note,
                     trail.scaleY > 0.0f ? &trail : NULL,
                     end.position.y < receptorY ? &end : NULL);
        return;
    }

    const float trailBottom = trail.position.y + trail.scaleY * TRAIL_SEGMENT_H;
    if (trail.position.y < receptorY) {
        trail.scaleY = fmaxf(0.0f, (trailBottom - receptorY) / TRAIL_SEGMENT_H);
        trail.position.y = receptorY;
    }

    DrawNoteObjs(note,
                 trail.scaleY > 0.0f ? &trail : NULL,
                 trailBottom >= receptorY ? &end : NULL);
}

/* Tecla do dono da nota segurada neste frame (automatico conta como sempre segurada). So e consultada com sustain ativo. */
static char LaneHeld(const Note* note) {
    const int slot = PlayStatePlayerInput_Owner(note);
    return slot < 0 || IsKeyDown(PlayStatePlayerInput_Key(slot, note->id));
}

/* Logica do sustain (hold/miss). Sem desenho: roda no Update. */
static void UpdateSustain(Note* note, const PlayStateNoteView* view) {
    if (note->missed || note->skipped) return;

    const char shouldHold = Note_ShouldHold(note, view->time);

    if (PlayStatePlayerInput_Owner(note) < 0) {   // automatico: oponente, BF da IA ou botplay
        if (shouldHold && note->pressed) {
            if (note->mustHit) events.botHold(note);
            else events.opponentHold(note);
        }
    }
    else if (!note->pressed) {
        return;
    }
    else if (!view->paused && shouldHold) {
        const float tailLeft = (note->time + note->length) - view->time;
        if (tailLeft > 0.12f && !LaneHeld(note))
            events.miss(note);
        else
            events.playerHold(note);
    }
}

/* Posiciona, anima e desenha o sustain da nota (so le o estado). sk = textura do tipo (ou NULL). */
static void DrawSustainBody(const Note* note, const StrumNote* strum, float endHeight,
                            float scrollSpeed, char fullyAbove, const TypeSkin* sk) {
    // Middlescroll: sustain do oponente acompanha a escala menor do receptor.
    const float k = (SCROLL_IS_MIDDLE(gameOptions.scroll) && !note->mustHit) ? (0.4f / NOTE_SCALE) : 1.0f;

    const float lengthPx = scrollSpeed * note->length;
    const float headY = renderNote.position.y + trailOffset.y * k;

    if (sk != NULL) AnimatedObject_SetAnimation(&trail, sk->trail[note->id]);
    else SetAnimIfChanged(&trail, Note_TrailAnimation(note->id));
    trail.scaleX = end.scaleX = NOTE_SCALE * k;
    trail.position.x = strum->object.position.x + trailOffset.x * k;
    trail.scaleY = fmaxf(0.01f, (lengthPx - endHeight) / TRAIL_SEGMENT_H);
    trail.color = normalColor;

    if (sk != NULL) AnimatedObject_SetAnimation(&end, sk->end[note->id]);
    else SetAnimIfChanged(&end, Note_EndAnimation(note->id));
    end.position.x = trail.position.x;
    end.scaleY = endHeight / END_SEGMENT_H;
    end.color = normalColor;

    // Cauda vai no sentido do futuro: para baixo no normal, para cima no downscroll.
    if (SCROLL_IS_DOWN(gameOptions.scroll)) {
        end.position.y = headY - lengthPx;
        trail.position.y = end.position.y + endHeight;
    } else {
        trail.position.y = headY;
        end.position.y = trail.position.y + trail.scaleY * TRAIL_SEGMENT_H;
    }

    if (note->missed) {
        trail.color = end.color = missedColor;
        DrawSustainFull(note, fullyAbove);
        return;
    }

    // Cabeca ainda nao acertada (ou ignorada pelo tipo): sustain inteiro, sem corte.
    if (!note->pressed) {
        DrawSustainFull(note, fullyAbove);
        return;
    }

    DrawSustainClipped(note, strum, k);
}

static void DrawSustain(const Note* note, const StrumNote* strum, float endHeight,
                        float scrollSpeed, char fullyAbove) {
    const int ts = typeSkinOf[note->type];
    if (ts < 0) {
        DrawSustainBody(note, strum, endHeight, scrollSpeed, fullyAbove, NULL);
        return;
    }
    trail.animationSet = end.animationSet = *typeSkins[ts].set;
    DrawSustainBody(note, strum, endHeight, scrollSpeed, fullyAbove, &typeSkins[ts]);
    trail.animationSet = end.animationSet = songSkin;
    trail.currentAnim.isValid = end.currentAnim.isValid = 0;   /* a proxima nota normal reinicia a animacao */
}

/* Avanca firstNote sobre notas ja resolvidas e bem atras do receptor. */
static void CullPassedNotes(float time, float cullTimeThreshold) {
    while (firstNote < noteCount) {
        const Note* n = notes + firstNote;
        const char resolved = n->id < 0 || n->id > 3 || n->pressed || n->missed || n->skipped;
        if (!resolved || time - (n->time + n->length) <= cullTimeThreshold)
            break;
        firstNote++;
    }
}

/* Culling + hits automaticos + hold/miss. Sem desenho; roda no Update, depois do input. */
void PlayStateNoteSpawn_Update(const PlayStateNoteView* view) {
    const float time = view->time;
    const float scrollSpeed = 450.0f * view->songSpeed;
    const float cutoffTime = time + (900.0f / scrollSpeed);

    /* Streaming: carrega as secoes que o cutoff (e a folga) ainda alcanca, antes do culling. */
    if (stream != NULL) {
        streamLookahead = (900.0f / scrollSpeed) + STREAM_MARGIN_S;
        StreamCompact();
        StreamFeed(time + streamLookahead, -1.0f);
    }

    CullPassedNotes(time, 400.0f / scrollSpeed);

    for (size_t i = firstNote; i < noteCount; i++) {
        Note* note = notes + i;

        if (note->id < 0 || note->id > 3)
            continue;

        if (note->time > cutoffTime)
            break;

        // Automaticos (oponente, BF da IA no Play as, botplay) acertam exatamente no tempo da nota.
        // Tipo que eles nao apertam (Hurt Note no bot, ignoreNote) segue reto e vira "skipped".
        if (!note->pressed && !note->skipped && time >= note->time && PlayStatePlayerInput_Owner(note) < 0) {
            if (PlayStatePlayerInput_AutoSkips(note)) {
                note->skipped = 1;
            } else {
                note->pressed = 1;
                if (note->mustHit) events.botHit(note);
                else events.opponentHit(note);
            }
        }

        if (note->length > 0)
            UpdateSustain(note, view);

        if (!note->pressed && !note->missed && !note->skipped && Note_TooLate(note, time)) {
            if (PlayStatePlayerInput_Ignorable(note)) note->skipped = 1;   // ignoreNote: passar nao e miss
            else events.miss(note);
        }
    }
}

/* So desenha o estado atual das notas (cabeca, sustain e ponta). Dentro da camera do HUD. */
void PlayStateNoteSpawn_Draw(const PlayStateNoteView* view) {
    rlSetBlendFactorsSeparate(0x0302, 0x0303, 1, 0x0303, 0x8006, 0x8006);
    BeginBlendMode(BLEND_CUSTOM_SEPARATE);

    const float time = view->time;
    const float speedMult = view->scrollVelocity * view->songSpeed;
    const float scrollSpeed = 450.0f * view->songSpeed;
    const float cutoffTime = time + (900.0f / scrollSpeed);
    const float endHeight = fmaxf(END_SEGMENT_H, 18.0f * view->songSpeed);

    int drawBudget = NOTE_DRAW_BUDGET;

    if (++occupiedGen == 0) {   /* wraparound: limpa uma vez a cada 65535 frames */
        memset(occupied, 0, sizeof(occupied));
        occupiedGen = 1;
    }

    for (size_t i = firstNote; i < noteCount; i++) {
        const Note* note = notes + i;

        if (note->id < 0 || note->id > 3)
            continue;

        if (note->time > cutoffTime)
            break;

        // Hidden Notes: so desenha as notas de quem esta cantando na secao. So muda o desenho; o hit segue igual.
        if (!GameData_HiddenShowsSide(note->mustHit, note->sectionMustHit))
            continue;

        const StrumNote* strum = (note->mustHit ? view->bfStrums : view->dadStrums) + note->id;

        renderNote.position.x = strum->object.position.x;
        renderNote.position.y = strum->object.position.y - speedMult * (note->time - time);

        const char fullyAbove = SCROLL_IS_DOWN(gameOptions.scroll) ? (renderNote.position.y > 900.0f)
                                                           : (renderNote.position.y < -200.0f);

        if ((note->pressed || note->skipped) && fullyAbove && time >= note->time + note->length)
            continue;

        /* Cabeca curta fora da tela, ou empilhada no mesmo pixel de outra ja desenhada (spam gigante):
           nao gera draw call. O custo do desenho fica limitado a ~8 casas x altura da tela. */
        if (note->length <= 0.0f) {
            const float yf = renderNote.position.y;
            if (!(yf > -(float)NOTE_OCC_TOP && yf < (float)(NOTE_OCC_ROWS - NOTE_OCC_TOP)))
                continue;   /* fora da tela (ou NaN) */
            const int row = (int)yf + NOTE_OCC_TOP;
            if (note->pressed)
                continue;
            unsigned short* cell = &occupied[(note->mustHit ? 4 : 0) + note->id][row];
            if (*cell == occupiedGen)
                continue;
            *cell = occupiedGen;
        }

        if (drawBudget <= 0)
            break;

        /* Estado de animacao por lane: notas de lanes alternadas no mesmo trecho
           nao reiniciam a animacao umas das outras (antes voltava ao frame 0 a cada troca). */
        renderNote.currentAnim = laneAnimState[note->id];
        SetAnimIfChanged(&renderNote, Note_Animation(note->id));

        if (note->length > 0) {
            DrawSustain(note, strum, endHeight, scrollSpeed, fullyAbove);
            drawBudget -= 2;
        }

        if (!note->pressed) {
            drawBudget--;
            renderNote.scaleX = renderNote.scaleY = HeadScale(note);
            renderNote.color = WHITE;
            const int ts = typeSkinOf[note->type];
            if (ts >= 0) {   /* textura do tipo: copia com a animacao da skin dele (lane state intacto) */
                RayAnimatedObject head = renderNote;
                head.animationSet = *typeSkins[ts].set;
                AnimatedObject_SetAnimation(&head, typeSkins[ts].head[note->id]);
                DrawNoteObjs(note, &head, NULL);
            } else {
                DrawNoteObjs(note, &renderNote, NULL);
            }
        }
        laneAnimState[note->id] = renderNote.currentAnim;
    }

    EndBlendMode();
}
