/*
 * Friday Night Funkin' C Engine (c) 2026 nordjkpl
 * ShaderFx.c - efeitos de tela (TV velha, glitch) em GLSL 330 via raylib.
 * Estatico: velocidade 0 = quadro congelado (mesma semente sempre). Animado: velocidade > 0.
 * Carregamento preguicoso (so quando um efeito e ligado). Custo: 1 passe de textura por frame.
 */

#include "rendering/shaders/ShaderFx.h"
#include "GameData.h"
#include "raylib.h"
#include <stddef.h>

static const char kFsTv[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 uRes;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }\n"
    "void main() {\n"
    "    vec2 c = fragTexCoord * 2.0 - 1.0;\n"
    "    float r2 = dot(c, c);\n"
    "    c *= 1.0 + uAmt * 0.05 * r2;\n"                     /* curvatura do tubo */
    "    vec2 uv = c * 0.5 + 0.5;\n"
    "    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { finalColor = vec4(0.0, 0.0, 0.0, 1.0); return; }\n"
    "    float ca = uAmt * 0.004 * (0.5 + r2);\n"            /* separacao RGB nas bordas */
    "    vec3 col;\n"
    "    col.r = texture(texture0, uv + vec2(ca, 0.0)).r;\n"
    "    col.g = texture(texture0, uv).g;\n"
    "    col.b = texture(texture0, uv - vec2(ca, 0.0)).b;\n"
    "    float scan = 0.5 + 0.5 * sin(fragTexCoord.y * uRes.y * 2.0943);\n"  /* linhas a cada 3 px */
    "    col *= mix(1.0, 0.72 + 0.28 * scan, uAmt);\n"
    "    col *= mix(1.0, 1.0 - smoothstep(0.35, 1.25, length(c)), uAmt);\n"  /* vinheta */
    "    float tt = floor(uTime * uAnim * 24.0);\n"
    "    float n = hash(floor(fragTexCoord * uRes) + tt * 17.0);\n"        /* granulado */
    "    col += (n - 0.5) * 0.10 * uAmt;\n"
    "    float on = step(0.001, uAnim);\n"
    "    float roll = smoothstep(0.93, 1.0, fract(fragTexCoord.y - uTime * uAnim * 0.25));\n"  /* faixa de rolagem */
    "    col += roll * 0.10 * uAmt * on;\n"
    "    col *= 1.0 + (hash(vec2(tt, 3.0)) - 0.5) * 0.06 * uAmt * on;\n"    /* tremor de brilho */
    "    col *= vec3(1.0, 1.02, 0.97);\n"                    /* tom de tubo */
    "    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
    "}\n";

static const char kFsGlitch[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 uRes;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }\n"
    "void main() {\n"
    "    float tt = floor(uTime * uAnim * 12.0);\n"          /* estatico: tt = 0 (quadro congelado) */
    "    vec2 uv = fragTexCoord;\n"
    "    float row = floor(uv.y * 22.0);\n"
    "    float h = hash(vec2(row, tt));\n"
    "    float on = step(1.0 - 0.5 * uAmt, h);\n"            /* mais intensidade = mais fatias */
    "    uv.x += (hash(vec2(row, tt + 7.0)) - 0.5) * 0.12 * uAmt * on;\n"
    "    float sp = uAmt * 0.012 * (0.5 + hash(vec2(tt, 2.0)));\n"
    "    vec3 col;\n"
    "    col.r = texture(texture0, uv + vec2(sp, 0.0)).r;\n"
    "    col.g = texture(texture0, uv).g;\n"
    "    col.b = texture(texture0, uv - vec2(sp, 0.0)).b;\n"
    "    float bh = hash(floor(fragTexCoord * vec2(48.0, 27.0)) + tt);\n"
    "    float blk = step(1.0 - 0.04 * uAmt, bh);\n"          /* blocos invertidos */
    "    col = mix(col, 1.0 - col, blk);\n"
    "    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
    "}\n";

/* Buraco negro: puxa a imagem pro centro (lente), escurece, horizonte preto e anel de acrecao animado. */
static const char kFsBlackness[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 uRes;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    float asp = uRes.x / max(uRes.y, 1.0);\n"
    "    vec2 c = fragTexCoord * 2.0 - 1.0;\n"
    "    c.x *= asp;\n"                                          /* circulo sem elipse */
    "    float r = length(c);\n"
    "    float k = uAmt * 0.45 * exp(-r * r * 2.5);\n"           /* puxao gravitacional: forte no centro */
    "    float ang = uAmt * 0.9 * exp(-r * r * 3.5);\n"          /* redemoinho */
    "    float ca = cos(ang), sa = sin(ang);\n"
    "    vec2 s = mat2(ca, -sa, sa, ca) * (c * (1.0 + k));\n"
    "    s.x /= asp;\n"
    "    vec2 uv = s * 0.5 + 0.5;\n"
    "    vec3 col = texture(texture0, uv).rgb;\n"
    "    col *= step(0.0, uv.x) * step(uv.x, 1.0) * step(0.0, uv.y) * step(uv.y, 1.0);\n"
    "    float lum = dot(col, vec3(0.299, 0.587, 0.114));\n"
    "    col = mix(col, vec3(lum), uAmt * 0.6);\n"               /* perde cor */
    "    col *= 1.0 - uAmt * 0.6 * smoothstep(0.3, 1.2, r);\n"   /* borda escura */
    "    float rh = 0.08 + uAmt * 0.32;\n"                       /* raio do horizonte de eventos */
    "    col *= mix(1.0, smoothstep(rh, rh + 0.2, r), 0.3);\n"    /* buraco preto no centro (30% de escuridao, deixa a gameplay ver) */
    "    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
    "}\n";

/* Preto e branco: luminancia pura. intensidade 0..1 = quanto da cor some (1 = tudo cinza). */
static const char kFsGrayscale[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform float uAmt;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 t = texture(texture0, fragTexCoord);\n"
    "    float l = dot(t.rgb, vec3(0.299, 0.587, 0.114));\n"
    "    finalColor = vec4(mix(t.rgb, vec3(l), uAmt), t.a);\n"
    "}\n";

/* Bloom (brilho): 1 passe. Pega os pixels claros em dois aneis de 12 amostras ao redor
 * e soma por cima da imagem. intensidade = mais raio, mais limiar baixo e mais forca. */
static const char kFsBloom[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 uRes;\n"
    "uniform float uAmt;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec4 base = texture(texture0, fragTexCoord);\n"
    "    vec2 px = 1.0 / max(uRes, vec2(1.0));\n"
    "    float radius = mix(2.0, 12.0, uAmt);\n"                    /* raio em pixels */
    "    float thr = mix(0.9, 0.5, uAmt);\n"                        /* limiar de brilho */
    "    vec3 glow = vec3(0.0);\n"
    "    for (int ring = 1; ring <= 2; ring++) {\n"
    "        float r = radius * float(ring) * 0.5;\n"
    "        float off = (ring == 2) ? 0.5 : 0.0;\n"               /* aneis alternados */
    "        for (int i = 0; i < 12; i++) {\n"
    "            float a = (float(i) + off) * 0.5235988;\n"        /* 2*pi/12 */
    "            vec3 s = texture(texture0, fragTexCoord + vec2(cos(a), sin(a)) * r * px).rgb;\n"
    "            glow += max(s - thr, vec3(0.0)) / (1.0 - thr);\n"
    "        }\n"
    "    }\n"
    "    glow /= 24.0;\n"
    "    vec3 col = base.rgb + glow * uAmt * 2.0;\n"
    "    finalColor = vec4(clamp(col, 0.0, 1.0), base.a);\n"
    "}\n";

/* Espaco: nebulosa em fbm (roxo/azul/rosa) entra em tela sobre a imagem, mais forte nas areas escuras,
 * e uma camada de estrelas (2 profundidades) que descem e piscam. animacao 0 = quadro parado. */
static const char kFsSpace[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 uRes;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
    "float noise(vec2 p) {\n"
    "    vec2 i = floor(p), f = fract(p);\n"
    "    f = f * f * (3.0 - 2.0 * f);\n"
    "    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);\n"
    "}\n"
    "float fbm(vec2 p) {\n"
    "    float v = 0.0, a = 0.5;\n"
    "    for (int i = 0; i < 4; i++) { v += a * noise(p); p = p * 2.02 + vec2(17.0, 9.0); a *= 0.5; }\n"
    "    return v;\n"
    "}\n"
    /* estrelas de uma camada: grade de celulas, uma estrela por celula (algumas), desce com 'speed' px/s */
    "float stars(vec2 px, float cell, float speed, float t) {\n"
    "    px.y += t * speed;\n"
    "    vec2 id = floor(px / cell);\n"
    "    vec2 f = fract(px / cell) - 0.5;\n"
    "    float h = hash(id);\n"
    "    if (h < 0.82) return 0.0;\n"                               /* so ~18% das celulas tem estrela */
    "    vec2 off = vec2(hash(id + 3.1), hash(id + 7.7)) * 0.6 - 0.3;\n"
    "    float d = length(f - off);\n"
    "    float tw = 0.65 + 0.35 * sin(t * (2.0 + h * 4.0) + h * 40.0);\n"
    "    return smoothstep(0.07, 0.0, d) * tw;\n"
    "}\n"
    "void main() {\n"
    "    float t = uTime * uAnim;\n"
    "    vec2 uv = fragTexCoord;\n"
    "    float asp = uRes.x / max(uRes.y, 1.0);\n"
    "    vec2 p = (uv * 2.0 - 1.0) * vec2(asp, 1.0);\n"
    "    vec2 q = p * 1.5 + vec2(t * 0.02, -t * 0.015);\n"
    "    float n = fbm(q + fbm(q * 1.3 + t * 0.04));\n"           /* nebulosa com dobra */
    "    vec3 neb = mix(vec3(0.10, 0.02, 0.24), vec3(0.04, 0.32, 0.58), smoothstep(0.30, 0.75, n));\n"
    "    neb = mix(neb, vec3(0.85, 0.25, 0.60), smoothstep(0.62, 0.95, n) * 0.6);\n"
    "    vec3 col = texture(texture0, uv).rgb;\n"
    "    float lum = dot(col, vec3(0.299, 0.587, 0.114));\n"
    "    vec3 scr = 1.0 - (1.0 - col) * (1.0 - neb * uAmt);\n"       /* screen: ilumina sem apagar a imagem */
    "    float dark = 1.0 - smoothstep(0.15, 0.6, lum) * 0.5;\n"     /* areas escuras pegam mais nebulosa */
    "    col = mix(col, scr, uAmt * 0.85 * dark);\n"
    "    vec2 px = uv * uRes;\n"
    "    float s = stars(px, 110.0, 25.0, t) + 0.6 * stars(px + 37.0, 55.0, 60.0, t);\n"  /* fundo + frente */
    "    col += vec3(0.85, 0.92, 1.0) * s * uAmt * (1.0 - lum * 0.6);\n"
    "    col *= 1.0 - uAmt * 0.35 * smoothstep(0.5, 1.3, length(p));\n"  /* vinheta espacial */
    "    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
    "}\n";

/* Pulso: batida no ritmo (animacao 100% = 120 BPM). Cada batida: zoom curto pro centro,
 * clarao e borda rosada. Com animacao 0 fica um pulso fixo de meia forca. */
static const char kFsPulse[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    float ph = fract(uTime * uAnim * 2.0);\n"                 /* fase da batida 0..1 */
    "    float beat = pow(1.0 - ph, 6.0);\n"                        /* tum: ataque forte, cai rapido */
    "    float tum2 = 0.45 * pow(1.0 - fract(ph * 2.0 + 0.5), 8.0);\n"  /* segundo tum no meio do compasso (ph 0.25 e 0.75) */
    "    float pulse = (uAnim > 0.0) ? (beat + tum2) : 0.5;\n"
    "    pulse = min(pulse, 1.0) * uAmt;\n"
    "    vec2 uv = (fragTexCoord - 0.5) * (1.0 - pulse * 0.03) + 0.5;\n"   /* zoom pro centro */
    "    vec3 col = texture(texture0, uv).rgb;\n"
    "    col *= 1.0 + pulse * 0.30;\n"                              /* clarao */
    "    vec2 p = fragTexCoord * 2.0 - 1.0;\n"
    "    float edge = smoothstep(0.35, 1.25, length(p));\n"
    "    col += edge * pulse * vec3(0.30, 0.04, 0.14);\n"            /* borda rosada */
    "    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
    "}\n";

typedef struct {
    Shader sh;
    int locRes, locTime, locAmt, locAnim;
    char ok;
    char synced;            /* ultimos valores enviados (so o tempo muda todo frame) */
    float lastRes[2], lastAmt, lastAnim;
} FxSlot;

#define FX_COUNT 7
static FxSlot slots[FX_COUNT];   /* [0] = TV, [1] = glitch, [2] = blackness, [3] = grayscale, [4] = bloom, [5] = space, [6] = pulse */
static char triedLoad;
static ShaderFxKind curKind = SHADERFX_NONE;
static float curAmt, curAnim;

static float Clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static void FxSlot_Load(FxSlot* s, const char* fs) {
    s->sh = LoadShaderFromMemory(NULL, fs);
    s->ok = (s->sh.id != 0);
    s->synced = 0;   /* programa novo: uniforms ainda nao enviados */
    if (!s->ok) return;
    s->locRes = GetShaderLocation(s->sh, "uRes");
    s->locTime = GetShaderLocation(s->sh, "uTime");
    s->locAmt = GetShaderLocation(s->sh, "uAmt");
    s->locAnim = GetShaderLocation(s->sh, "uAnim");
}

static void EnsureLoaded(void) {
    if (triedLoad) return;
    triedLoad = 1;
    FxSlot_Load(&slots[0], kFsTv);
    FxSlot_Load(&slots[1], kFsGlitch);
    FxSlot_Load(&slots[2], kFsBlackness);
    FxSlot_Load(&slots[3], kFsGrayscale);
    FxSlot_Load(&slots[4], kFsBloom);
    FxSlot_Load(&slots[5], kFsSpace);
    FxSlot_Load(&slots[6], kFsPulse);
}

static FxSlot* CurSlot(void) {
    if (curKind == SHADERFX_NONE) return NULL;
    EnsureLoaded();
    FxSlot* s = &slots[curKind - 1];
    return s->ok ? s : NULL;
}

void ShaderFx_Set(ShaderFxKind kind, float intensity, float speed) {
    curAmt = Clampf(intensity, 0.0f, 1.0f);
    curAnim = Clampf(speed, 0.0f, 2.0f);
    curKind = (kind == SHADERFX_TV || kind == SHADERFX_GLITCH || kind == SHADERFX_BLACKNESS || kind == SHADERFX_GRAYSCALE || kind == SHADERFX_BLOOM || kind == SHADERFX_SPACE || kind == SHADERFX_PULSE) ? kind : SHADERFX_NONE;
    if (curAmt <= 0.0f || !gameOptions.shaders) curKind = SHADERFX_NONE; /* opcao Shaders off: nenhum efeito entra */
}

void ShaderFx_Reset(void) {
    curKind = SHADERFX_NONE;
}

int ShaderFx_IsActive(void) {
    return CurSlot() != NULL;
}

int ShaderFx_BeginDraw(float w, float h) {
    FxSlot* s = CurSlot();
    if (s == NULL) return 0;
    const float res[2] = {w, h};
    const float t = (float)GetTime();
    /* Uniforms ficam no programa: so reenvia os que mudaram (o tempo muda todo frame). */
    if (!s->synced || s->lastRes[0] != w || s->lastRes[1] != h) {
        SetShaderValue(s->sh, s->locRes, res, SHADER_UNIFORM_VEC2);
        s->lastRes[0] = w;
        s->lastRes[1] = h;
    }
    /* uTime so entra multiplicado por uAnim nos shaders: com uAnim = 0 (efeito parado) nao importa. */
    if (!s->synced || curAnim > 0.0f)
        SetShaderValue(s->sh, s->locTime, &t, SHADER_UNIFORM_FLOAT);
    if (!s->synced || s->lastAmt != curAmt) {
        SetShaderValue(s->sh, s->locAmt, &curAmt, SHADER_UNIFORM_FLOAT);
        s->lastAmt = curAmt;
    }
    if (!s->synced || s->lastAnim != curAnim) {
        SetShaderValue(s->sh, s->locAnim, &curAnim, SHADER_UNIFORM_FLOAT);
        s->lastAnim = curAnim;
    }
    s->synced = 1;
    BeginShaderMode(s->sh);
    return 1;
}

void ShaderFx_EndDraw(void) {
    EndShaderMode();
}

void ShaderFx_Unload(void) {
    for (int i = 0; i < FX_COUNT; i++) {
        if (slots[i].ok) UnloadShader(slots[i].sh);
        slots[i].ok = 0;
    }
    triedLoad = 0;
    curKind = SHADERFX_NONE;
}
