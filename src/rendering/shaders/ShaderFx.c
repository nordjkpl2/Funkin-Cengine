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

/* Bloom (brilho): 1 passe com 48 amostras em espiral (Vogel) dentro do raio, peso gaussiano.
 * Limiar com joelho suave (sem corte seco) e um giro por pixel (ruido de gradiente) que
 * troca o serrilhado/bandas por grao fino. intensidade = raio, forca e queda do limiar. */
static const char kFsBloom[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 uRes;\n"
    "uniform float uAmt;\n"
    "out vec4 finalColor;\n"
    "const int N = 48;\n"
    "const float GOLDEN = 2.39996323;\n"
    "vec3 bright(vec3 c, float thr) {\n"                          /* joelho suave: cai de 0 a thr+KNEE */
    "    const float KNEE = 0.15;\n"
    "    float l = max(c.r, max(c.g, c.b));\n"
    "    float soft = clamp(l - thr + KNEE, 0.0, 2.0 * KNEE);\n"
    "    soft = soft * soft / (4.0 * KNEE + 1e-4);\n"
    "    float k = max(soft, l - thr) / max(l, 1e-4);\n"
    "    return c * max(k, 0.0);\n"
    "}\n"
    "void main() {\n"
    "    vec4 base = texture(texture0, fragTexCoord);\n"
    "    vec2 px = 1.0 / max(uRes, vec2(1.0));\n"
    "    float radius = mix(3.0, 24.0, uAmt);\n"                    /* raio em pixels */
    "    float thr = mix(0.85, 0.45, uAmt);\n"                      /* limiar de brilho */
    "    float rot = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715)))) * 6.2831853;\n"
    "    vec3 glow = vec3(0.0);\n"
    "    float wsum = 0.0;\n"
    "    for (int i = 0; i < N; i++) {\n"
    "        float f = (float(i) + 0.5) / float(N);\n"
    "        float r = sqrt(f);\n"                                  /* disco uniforme */
    "        float a = float(i) * GOLDEN + rot;\n"
    "        float w = exp(-2.5 * f);\n"                            /* gaussiano: centro pesa mais */
    "        vec2 o = vec2(cos(a), sin(a)) * r * radius * px;\n"
    "        glow += bright(texture(texture0, fragTexCoord + o).rgb, thr) * w;\n"
    "        wsum += w;\n"
    "    }\n"
    "    glow /= wsum;\n"
    "    vec3 col = base.rgb + glow * uAmt * 2.5;\n"
    "    finalColor = vec4(clamp(col, 0.0, 1.0), base.a);\n"
    "}\n";

/* Distorcao: ondas de calor (senos cruzados que andam no tempo) + leve curvatura de lente.
 * intensidade = forca do deslocamento; animacao = velocidade das ondas (0 = onda parada). */
static const char kFsDistortion[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 uRes;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    float t = uTime * uAnim;\n"
    "    float asp = uRes.x / max(uRes.y, 1.0);\n"
    "    vec2 c = (fragTexCoord * 2.0 - 1.0) * vec2(asp, 1.0);\n"
    "    float r2 = dot(c, c);\n"
    "    float w1 = sin(c.y * 7.0 + t * 2.1 + sin(c.x * 3.0 - t * 0.7));\n"
    "    float w2 = sin(c.x * 5.5 - t * 1.6 + c.y * 2.0);\n"
    "    vec2 d = vec2(w1 / asp, w2) * uAmt * 0.012;\n"            /* deslocamento em UV (sem esticar) */
    "    vec2 uv = fragTexCoord + d + (c / vec2(asp, 1.0)) * r2 * uAmt * 0.02;\n"  /* + curvatura (lente) */
    "    uv = clamp(uv, 0.0, 1.0);\n"
    "    finalColor = vec4(texture(texture0, uv).rgb, 1.0);\n"
    "}\n";

/* Aberracao cromatica: separa R e B radialmente do centro (mais forte nas bordas).
 * intensidade = distancia da separacao; animacao > 0 faz ela pulsar no tempo. */
static const char kFsChromatic[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec2 d = fragTexCoord - 0.5;\n"
    "    float pulse = uAnim > 0.0 ? 0.75 + 0.25 * sin(uTime * uAnim * 6.0) : 1.0;\n"
    "    vec2 off = d * (0.3 + length(d) * 1.4) * uAmt * 0.06 * pulse;\n"
    "    float r = texture(texture0, fragTexCoord + off).r;\n"
    "    vec4 g = texture(texture0, fragTexCoord);\n"
    "    float b = texture(texture0, fragTexCoord - off).b;\n"
    "    finalColor = vec4(r, g.g, b, g.a);\n"
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

/* Dissolver: ruido de valor (2 oitavas) corta os pixels abaixo de intensidade; a borda do corte brilha laranja.
 * intensidade = quanto da tela ja sumiu (0 = nada, 1 = tudo). animacao > 0 faz o ruido deslizar. */
static const char kFsDissolve[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec2 uRes;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }\n"
    "float vnoise(vec2 p) {\n"
    "    vec2 i = floor(p), f = fract(p);\n"
    "    f = f * f * (3.0 - 2.0 * f);\n"
    "    return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), f.x), mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);\n"
    "}\n"
    "void main() {\n"
    "    float asp = uRes.x / max(uRes.y, 1.0);\n"
    "    vec2 p = fragTexCoord * vec2(asp, 1.0) * 6.0;\n"
    "    vec2 drift = vec2(uTime * uAnim * 0.20, uTime * uAnim * 0.13);\n"  /* estatico: drift = 0 */
    "    float n = 0.6 * vnoise(p + drift) + 0.4 * vnoise(p * 2.3 + drift * 1.7 + 7.1);\n"
    "    float keep = step(uAmt, n);\n"
    "    float rim = (1.0 - smoothstep(0.0, 0.035, abs(n - uAmt))) * step(0.001, uAmt);\n"
    "    vec3 col = texture(texture0, fragTexCoord).rgb * keep;\n"
    "    col = mix(col, vec3(1.0, 0.55, 0.12), rim);\n"
    "    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
    "}\n";

/* Ilusao de profundidade: a luminancia vira um "mapa de profundidade" falso (escuro = longe).
 * Uma camera que balanca (senoides) desloca mais o que esta perto; o que esta longe ganha nevoa.
 * intensidade = forca do paralaxe e da nevoa; animacao 0 = so a nevoa, parado. */
static const char kFsDepth[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "uniform sampler2D texture0;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    vec2 uv = fragTexCoord;\n"
    "    vec3 c0 = texture(texture0, uv).rgb;\n"
    "    float lum = dot(c0, vec3(0.299, 0.587, 0.114));\n"
    "    float depth = clamp(mix(length(uv - 0.5) * 1.4, 1.0 - lum, 0.5), 0.0, 1.0);\n"  /* 0 = perto, 1 = longe */
    "    float t = uTime * uAnim;\n"
    "    vec2 cam = vec2(sin(t * 0.7), cos(t * 0.5) * 0.5) * uAmt * 0.04;\n"
    "    vec2 suv = clamp(uv + cam * (1.0 - depth), 0.0, 1.0);\n"
    "    vec3 col = texture(texture0, suv).rgb;\n"
    "    vec3 fog = vec3(0.10, 0.08, 0.22);\n"
    "    col = mix(col, fog, depth * uAmt * 0.45);\n"                   /* nevoa no fundo */
    "    finalColor = vec4(clamp(col, 0.0, 1.0), 1.0);\n"
    "}\n";

/* Bordas pretas: vinheta pelo centro da tela. intensidade = quanto escurece o canto;
 * animacao > 0 faz a borda respirar no tempo (pulso lento). */
static const char kFsVignette[] =
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
    "    vec3 col = texture(texture0, fragTexCoord).rgb;\n"
    "    vec2 c = (fragTexCoord - 0.5) * vec2(asp, 1.0);\n"
    "    float pulse = 1.0 + 0.06 * sin(uTime * uAnim * 2.0) * step(0.001, uAnim);\n"
    "    float v = smoothstep(0.30, 0.95, length(c) * pulse);\n"
    "    col *= 1.0 - uAmt * 0.95 * v;\n"
    "    finalColor = vec4(col, 1.0);\n"
    "}\n";

/* Ondulacao do fundo: NAO e da pilha. Roda so nas imagens da camada de tras do stage
 * (ShaderFx_BeginBackgroundWave / EndBackgroundWave), deslocando a UV por senoides. */
static const char kFsWave[] =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "uniform float uTime;\n"
    "uniform float uAmt;\n"
    "uniform float uAnim;\n"
    "out vec4 finalColor;\n"
    "void main() {\n"
    "    float t = uTime * uAnim;\n"
    "    vec2 uv = fragTexCoord;\n"
    "    uv.x += sin(uv.y * 18.0 + t * 2.4) * 0.020 * uAmt;\n"
    "    uv.y += sin(uv.x * 12.0 + t * 1.7) * 0.012 * uAmt;\n"
    "    vec4 c = texture(texture0, uv);\n"
    "    finalColor = c * colDiffuse * fragColor;\n"                   /* mantem tint e alpha do sprite */
    "}\n";

typedef struct {
    Shader sh;
    int locRes, locTime, locAmt, locAnim;
    char ok;
    char on;                /* efeito esta na pilha */
    char synced;            /* ultimos valores enviados (so o tempo muda todo frame) */
    float amt, anim;       /* valores deste efeito */
    float lastRes[2], lastAmt, lastAnim;
} FxSlot;

#define FX_COUNT 13
#define FX_STACK_MAX 3       /* efeitos ao mesmo tempo */
#define WAVE_SLOT 12         /* ondulacao do fundo: fora da pilha (on = ligada) */
static FxSlot slots[FX_COUNT];   /* [0] = TV, [1] = glitch, ..., [10] = depth, [11] = vinheta, [12] = wave (fundo) */
static ShaderFxKind order[FX_STACK_MAX];   /* ordem da pilha: order[0] = aplicado primeiro */
static int orderLen;
static char triedLoad;
static RenderTexture2D passTex[2];   /* ping-pong: cada passe le um e escreve no outro */
static char passReady;

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
    FxSlot_Load(&slots[7], kFsDistortion);
    FxSlot_Load(&slots[8], kFsChromatic);
    FxSlot_Load(&slots[9], kFsDissolve);
    FxSlot_Load(&slots[10], kFsDepth);
    FxSlot_Load(&slots[11], kFsVignette);
    FxSlot_Load(&slots[WAVE_SLOT], kFsWave);
}

static int IsValidKind(ShaderFxKind k) {
    return k > SHADERFX_NONE && k <= SHADERFX_VIGNETTE;
}

/* Tira da pilha a posicao idx (o efeito continua carregado, so deixa de ser aplicado). */
static void StackRemoveAt(int idx) {
    slots[order[idx] - 1].on = 0;
    for (int i = idx; i < orderLen - 1; i++) order[i] = order[i + 1];
    orderLen--;
}

/* Pega os efeitos ligados na ordem da pilha que carregaram. Retorna quantos. */
static int CollectActive(FxSlot* out[FX_STACK_MAX]) {
    if (orderLen == 0) return 0;
    EnsureLoaded();
    int n = 0;
    for (int i = 0; i < orderLen; i++) {
        FxSlot* s = &slots[order[i] - 1];
        if (s->ok) out[n++] = s;
    }
    return n;
}

static void SetUniforms(FxSlot* s, float w, float h) {
    const float res[2] = {w, h};
    const float t = (float)GetTime();
    /* Uniforms ficam no programa: so reenvia os que mudaram (o tempo muda todo frame). */
    if (!s->synced || s->lastRes[0] != w || s->lastRes[1] != h) {
        SetShaderValue(s->sh, s->locRes, res, SHADER_UNIFORM_VEC2);
        s->lastRes[0] = w;
        s->lastRes[1] = h;
    }
    /* uTime so entra multiplicado por uAnim nos shaders: com uAnim = 0 (efeito parado) nao importa. */
    if (!s->synced || s->anim > 0.0f)
        SetShaderValue(s->sh, s->locTime, &t, SHADER_UNIFORM_FLOAT);
    if (!s->synced || s->lastAmt != s->amt) {
        SetShaderValue(s->sh, s->locAmt, &s->amt, SHADER_UNIFORM_FLOAT);
        s->lastAmt = s->amt;
    }
    if (!s->synced || s->lastAnim != s->anim) {
        SetShaderValue(s->sh, s->locAnim, &s->anim, SHADER_UNIFORM_FLOAT);
        s->lastAnim = s->anim;
    }
    s->synced = 1;
}

/* Um passe: src (com srcRect) desenhado em dst pelo shader do efeito. */
static void DrawPass(FxSlot* s, Texture2D src, Rectangle srcRect, Rectangle dst, float w, float h) {
    SetUniforms(s, w, h);
    BeginShaderMode(s->sh);
    DrawTexturePro(src, srcRect, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
    EndShaderMode();
}

static int EnsurePassTex(float w, float h) {
    const int iw = (int)w, ih = (int)h;
    if (passReady && passTex[0].texture.width == iw && passTex[0].texture.height == ih) return 1;
    if (passReady) {
        UnloadRenderTexture(passTex[0]);
        UnloadRenderTexture(passTex[1]);
        passReady = 0;
    }
    passTex[0] = LoadRenderTexture(iw, ih);
    passTex[1] = LoadRenderTexture(iw, ih);
    passReady = (passTex[0].id != 0 && passTex[1].id != 0);
    return passReady;
}

void ShaderFx_Reset(void) {
    for (int i = 0; i < FX_COUNT; i++) slots[i].on = 0;
    orderLen = 0;
}

void ShaderFx_Remove(ShaderFxKind kind) {
    if (!IsValidKind(kind) || !slots[kind - 1].on) return;
    for (int i = 0; i < orderLen; i++) {
        if (order[i] == kind) {
            StackRemoveAt(i);
            return;
        }
    }
}

void ShaderFx_Push(ShaderFxKind kind, float intensity, float speed) {
    if (!IsValidKind(kind)) return;
    if (!gameOptions.shaders) { ShaderFx_Reset(); return; }   /* opcao Shaders off: nenhum efeito entra */
    const float amt = Clampf(intensity, 0.0f, 1.0f);
    if (amt <= 0.0f) { ShaderFx_Remove(kind); return; }
    FxSlot* s = &slots[kind - 1];
    s->amt = amt;
    s->anim = Clampf(speed, 0.0f, 2.0f);
    if (s->on) return;                       /* ja esta na pilha: so atualizou os valores */
    if (orderLen == FX_STACK_MAX) StackRemoveAt(0);   /* cheia: sai o mais antigo */
    order[orderLen++] = kind;
    s->on = 1;
}

void ShaderFx_Set(ShaderFxKind kind, float intensity, float speed) {
    ShaderFx_Reset();
    ShaderFx_Push(kind, intensity, speed);
}

int ShaderFx_IsActive(void) {
    FxSlot* act[FX_STACK_MAX];
    return CollectActive(act) > 0;
}

/* Desenha o canvas (src) em dst passando pela pilha. Sem efeito: so DrawTexturePro.
 * Com 2+ efeitos: os primeiros passes vao para texturas intermediarias e o ultimo vai para a tela. */
void ShaderFx_Present(Texture2D src, Rectangle dst, float w, float h) {
    FxSlot* act[FX_STACK_MAX];
    const int n = CollectActive(act);
    /* mesma inversao do RenderTexture usada no canvas, em todos os passes */
    const Rectangle srcRect = {0.0f, 0.0f, w, -h};
    if (n == 0) {
        DrawTexturePro(src, srcRect, dst, (Vector2){0.0f, 0.0f}, 0.0f, WHITE);
        return;
    }
    if (n == 1 || !EnsurePassTex(w, h)) {   /* sem textura extra: aplica so o ultimo */
        DrawPass(act[n - 1], src, srcRect, dst, w, h);
        return;
    }
    const Rectangle fullRect = {0.0f, 0.0f, w, h};
    Texture2D cur = src;
    for (int i = 0; i < n - 1; i++) {
        RenderTexture2D* rt = &passTex[i & 1];
        BeginTextureMode(*rt);
        DrawPass(act[i], cur, srcRect, fullRect, w, h);
        EndTextureMode();
        cur = rt->texture;
    }
    DrawPass(act[n - 1], cur, srcRect, dst, w, h);
}

void ShaderFx_SetBackgroundWave(float intensity, float speed) {
    FxSlot* w = &slots[WAVE_SLOT];
    const float amt = Clampf(intensity, 0.0f, 1.0f);
    if (!gameOptions.shaders || amt <= 0.0f) { w->on = 0; return; }
    w->amt = amt;
    w->anim = Clampf(speed, 0.0f, 2.0f);
    w->on = 1;
}

/* Liga o shader de ondulacao para as proximas imagens desenhadas. 1 = precisa de EndBackgroundWave. */
int ShaderFx_BeginBackgroundWave(void) {
    FxSlot* w = &slots[WAVE_SLOT];
    if (!w->on) return 0;
    EnsureLoaded();
    if (!w->ok) return 0;
    SetUniforms(w, 0.0f, 0.0f);   /* o shader nao usa uRes: 0 e ignorado */
    BeginShaderMode(w->sh);
    return 1;
}

void ShaderFx_EndBackgroundWave(void) {
    EndShaderMode();
}

void ShaderFx_Unload(void) {
    for (int i = 0; i < FX_COUNT; i++) {
        if (slots[i].ok) UnloadShader(slots[i].sh);
        slots[i].ok = 0;
        slots[i].on = 0;
    }
    if (passReady) {
        UnloadRenderTexture(passTex[0]);
        UnloadRenderTexture(passTex[1]);
        passReady = 0;
    }
    orderLen = 0;
    triedLoad = 0;
}
