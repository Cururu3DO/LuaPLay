#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include "function.h"
#include "graphics.h"

/* ---------------------------------------------------------------------
 * Cache de imagens: evita recarregar a mesma textura do disco toda vez
 * que image() e chamada. O usuario so passa o caminho do arquivo,
 * a engine cuida de carregar e guardar a textura internamente.
 *
 * Implementado como hash table com chaining (em vez de lista ligada
 * pura) para a busca ficar O(1) na media mesmo com muitas imagens
 * carregadas, ao inves de O(n) percorrendo tudo a cada chamada.
 * --------------------------------------------------------------------- */
#define IMAGE_CACHE_BUCKETS 64

typedef struct ImageCache {
    char *path;
    SDL_Texture *texture;
    int width;
    int height;
    struct ImageCache *next; /* proximo item no mesmo bucket (colisao de hash) */
} ImageCache;

static ImageCache *image_cache_buckets[IMAGE_CACHE_BUCKETS] = {0};

/* Hash djb2 - simples, rapida e com boa distribuicao para strings curtas
 * como caminhos de arquivo. */
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + (unsigned int)c;
    }
    return hash;
}

static SDL_Texture *get_texture(const char *path, int *out_w, int *out_h) {
    unsigned int bucket = hash_string(path) % IMAGE_CACHE_BUCKETS;

    for (ImageCache *node = image_cache_buckets[bucket]; node != NULL; node = node->next) {
        if (strcmp(node->path, path) == 0) {
            *out_w = node->width;
            *out_h = node->height;
            return node->texture;
        }
    }

    SDL_Texture *texture = IMG_LoadTexture(engine.renderer, path);
    if (!texture) {
        fprintf(stderr, "[LuaPlay] Erro ao carregar imagem '%s': %s\n", path, IMG_GetError());
        return NULL;
    }

    int w, h;
    SDL_QueryTexture(texture, NULL, NULL, &w, &h);

    ImageCache *node = malloc(sizeof(ImageCache));
    node->path = strdup(path);
    node->texture = texture;
    node->width = w;
    node->height = h;
    node->next = image_cache_buckets[bucket];
    image_cache_buckets[bucket] = node;

    *out_w = w;
    *out_h = h;
    return texture;
}

void graphics_cleanup(void) {
    for (int i = 0; i < IMAGE_CACHE_BUCKETS; i++) {
        ImageCache *node = image_cache_buckets[i];
        while (node) {
            ImageCache *next = node->next;
            SDL_DestroyTexture(node->texture);
            free(node->path);
            free(node);
            node = next;
        }
        image_cache_buckets[i] = NULL;
    }
}

/* ---------------------------------------------------------------------
 * Funcoes expostas ao Lua
 * --------------------------------------------------------------------- */

/* clear(r, g, b, a) -- todos os argumentos sao opcionais, padrao preto opaco.
 * Limpa a tela inteira com a cor informada. */
static int l_clear(lua_State *L) {
    int r = (int)luaL_optnumber(L, 1, 0);
    int g = (int)luaL_optnumber(L, 2, 0);
    int b = (int)luaL_optnumber(L, 3, 0);
    int a = (int)luaL_optnumber(L, 4, 255);

    SDL_SetRenderDrawColor(engine.renderer, (Uint8)r, (Uint8)g, (Uint8)b, (Uint8)a);
    SDL_RenderClear(engine.renderer);
    return 0;
}

/* color(r, g, b, a) -- define a cor usada pelos proximos rect()/circle()/line().
 * 'a' e opcional (padrao 255 = opaco). */
static int l_color(lua_State *L) {
    int r = (int)luaL_checknumber(L, 1);
    int g = (int)luaL_checknumber(L, 2);
    int b = (int)luaL_checknumber(L, 3);
    int a = (int)luaL_optnumber(L, 4, 255);

    engine.color_r = (Uint8)r;
    engine.color_g = (Uint8)g;
    engine.color_b = (Uint8)b;
    engine.color_a = (Uint8)a;
    return 0;
}

static void apply_current_color(void) {
    SDL_SetRenderDrawBlendMode(engine.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(engine.renderer, engine.color_r, engine.color_g,
                            engine.color_b, engine.color_a);
}

/* rect(x, y, w, h) -- retangulo preenchido com a cor atual. */
static int l_rect(lua_State *L) {
    int x = (int)luaL_checknumber(L, 1);
    int y = (int)luaL_checknumber(L, 2);
    int w = (int)luaL_checknumber(L, 3);
    int h = (int)luaL_checknumber(L, 4);

    apply_current_color();
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(engine.renderer, &rect);
    return 0;
}

/* circle(x, y, radius) -- circulo preenchido com a cor atual, centrado em (x,y). */
static int l_circle(lua_State *L) {
    int cx = (int)luaL_checknumber(L, 1);
    int cy = (int)luaL_checknumber(L, 2);
    int radius = (int)luaL_checknumber(L, 3);

    apply_current_color();

    /* Preenche o circulo desenhando uma linha horizontal por linha (scanline),
     * usando o algoritmo do ponto medio para achar a largura de cada linha. */
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        SDL_RenderDrawLine(engine.renderer, cx - x, cy + y, cx + x, cy + y);
        SDL_RenderDrawLine(engine.renderer, cx - x, cy - y, cx + x, cy - y);
        SDL_RenderDrawLine(engine.renderer, cx - y, cy + x, cx + y, cy + x);
        SDL_RenderDrawLine(engine.renderer, cx - y, cy - x, cx + y, cy - x);

        y += 1;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
    return 0;
}

/* line(x1, y1, x2, y2) -- linha reta com a cor atual. */
static int l_line(lua_State *L) {
    int x1 = (int)luaL_checknumber(L, 1);
    int y1 = (int)luaL_checknumber(L, 2);
    int x2 = (int)luaL_checknumber(L, 3);
    int y2 = (int)luaL_checknumber(L, 4);

    apply_current_color();
    SDL_RenderDrawLine(engine.renderer, x1, y1, x2, y2);
    return 0;
}

/* image(caminho, x, y, scale) -- desenha uma imagem na tela.
 * 'scale' e opcional (padrao 1). A imagem e carregada do disco na
 * primeira vez que for usada e depois fica em cache automaticamente,
 * entao o usuario nunca precisa gerenciar handles ou texturas. */
static int l_image(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int x = (int)luaL_checknumber(L, 2);
    int y = (int)luaL_checknumber(L, 3);
    double scale = luaL_optnumber(L, 4, 1.0);

    int w, h;
    SDL_Texture *texture = get_texture(path, &w, &h);
    if (!texture) {
        return luaL_error(L, "nao foi possivel carregar a imagem '%s'", path);
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    SDL_Rect dst = { x, y, (int)(w * scale), (int)(h * scale) };
    SDL_RenderCopy(engine.renderer, texture, NULL, &dst);
    return 0;
}

/* imageSize(caminho) -- retorna largura, altura da imagem (carrega/cacheia se preciso).
 * Util para centralizar ou posicionar sprites sem precisar abrir o arquivo manualmente. */
static int l_image_size(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    int w, h;
    SDL_Texture *texture = get_texture(path, &w, &h);
    if (!texture) {
        return luaL_error(L, "nao foi possivel carregar a imagem '%s'", path);
    }
    lua_pushinteger(L, w);
    lua_pushinteger(L, h);
    return 2;
}

/* ---------------------------------------------------------------------
 * Registro das funcoes no Lua
 * --------------------------------------------------------------------- */
void register_graphics_functions(lua_State *L) {
    lua_register(L, "clear", l_clear);
    lua_register(L, "color", l_color);
    lua_register(L, "rect", l_rect);
    lua_register(L, "circle", l_circle);
    lua_register(L, "line", l_line);
    lua_register(L, "image", l_image);
    lua_register(L, "imageSize", l_image_size);
}
