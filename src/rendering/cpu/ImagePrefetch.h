/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/* Pre-decodificacao de imagens em threads de fundo (CPU). Nao usa GPU.
 *
 * Uso: quem sabe de antemao quais PNG vai precisar chama ImagePrefetch_Start(caminho) para cada um;
 * a decodificacao (a parte lenta) roda em paralelo. Depois Render_LoadTexture(caminho) pega a imagem
 * pronta (ImagePrefetch_Take) e so envia para a GPU. Se nao houver pre-decodificacao, tudo funciona
 * como antes: e so uma dica de desempenho, nunca muda o resultado.
 *
 * Tudo aqui deve ser chamado pela thread principal.
 */
#pragma once
#ifndef RENDERING_CPU_IMAGEPREFETCH_H
#define RENDERING_CPU_IMAGEPREFETCH_H

#include "raylib.h"
#include <stdbool.h>

// Comeca a decodificar 'path' em fundo. Ignora o pedido se ja existe, se nao for PNG legivel
// ou se passaria do limite de memoria (IMAGE_PREFETCH_CAP_MB).
void ImagePrefetch_Start(const char* path);
// Se 'path' foi pre-decodificado, espera terminar e entrega a Image (quem recebe chama UnloadImage). false = nao ha.
bool ImagePrefetch_Take(const char* path, Image* out);
// Descarta pedidos que ninguem usou (libera a memoria).
void ImagePrefetch_Clear(void);

#define IMAGE_PREFETCH_CAP_MB 664

#endif
