#define _POSIX_C_SOURCE 200809L

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <SDL2/SDL.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include "control.h"

/* Guardamos uma copia do estado do teclado/mouse do frame anterior
 * para conseguir detectar o instante exato em que uma tecla/botao
 * foi pressionado ou solto (keyPressed/keyReleased). */
static Uint8 prev_keys[SDL_NUM_SCANCODES];
static Uint32 prev_mouse_buttons = 0;

void control_begin_frame(void) {
    const Uint8 *current = SDL_GetKeyboardState(NULL);
    memcpy(prev_keys, current, SDL_NUM_SCANCODES);
    prev_mouse_buttons = SDL_GetMouseState(NULL, NULL);
}

/* Converte o nome de uma tecla (ex: "space", "left", "a", "escape")
 * para o scancode correspondente. SDL_GetScancodeFromName faz uma busca
 * linear internamente, entao guardamos o resultado em cache (hash table
 * simples) para nao pagar esse custo de novo a cada frame. */
#define KEY_NAME_CACHE_BUCKETS 64

typedef struct KeyNameCache {
    char *name;
    SDL_Scancode scancode;
    struct KeyNameCache *next;
} KeyNameCache;

static KeyNameCache *key_name_buckets[KEY_NAME_CACHE_BUCKETS] = {0};

static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + (unsigned int)c;
    }
    return hash;
}

static SDL_Scancode scancode_from_name(const char *name) {
    unsigned int bucket = hash_string(name) % KEY_NAME_CACHE_BUCKETS;

    for (KeyNameCache *node = key_name_buckets[bucket]; node != NULL; node = node->next) {
        if (strcasecmp(node->name, name) == 0) {
            return node->scancode;
        }
    }

    SDL_Scancode sc = SDL_GetScancodeFromName(name);

    KeyNameCache *node = malloc(sizeof(KeyNameCache));
    node->name = strdup(name);
    node->scancode = sc;
    node->next = key_name_buckets[bucket];
    key_name_buckets[bucket] = node;

    return sc;
}

void control_cleanup(void) {
    for (int i = 0; i < KEY_NAME_CACHE_BUCKETS; i++) {
        KeyNameCache *node = key_name_buckets[i];
        while (node) {
            KeyNameCache *next = node->next;
            free(node->name);
            free(node);
            node = next;
        }
        key_name_buckets[i] = NULL;
    }
}

/* key(nome) -- true enquanto a tecla estiver pressionada. */
static int l_key(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    SDL_Scancode sc = scancode_from_name(name);
    if (sc == SDL_SCANCODE_UNKNOWN) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const Uint8 *current = SDL_GetKeyboardState(NULL);
    lua_pushboolean(L, current[sc]);
    return 1;
}

/* keyPressed(nome) -- true apenas no frame em que a tecla foi pressionada. */
static int l_key_pressed(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    SDL_Scancode sc = scancode_from_name(name);
    if (sc == SDL_SCANCODE_UNKNOWN) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const Uint8 *current = SDL_GetKeyboardState(NULL);
    lua_pushboolean(L, current[sc] && !prev_keys[sc]);
    return 1;
}

/* keyReleased(nome) -- true apenas no frame em que a tecla foi solta. */
static int l_key_released(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    SDL_Scancode sc = scancode_from_name(name);
    if (sc == SDL_SCANCODE_UNKNOWN) {
        lua_pushboolean(L, 0);
        return 1;
    }
    const Uint8 *current = SDL_GetKeyboardState(NULL);
    lua_pushboolean(L, !current[sc] && prev_keys[sc]);
    return 1;
}

/* Aceita tanto nome ("left","right","middle") quanto numero (1,2,3)
 * para identificar o botao do mouse, e devolve a mascara SDL_BUTTON(x). */
static Uint32 mouse_button_mask(lua_State *L, int arg) {
    if (lua_isnumber(L, arg)) {
        int n = (int)lua_tointeger(L, arg);
        return SDL_BUTTON(n);
    }

    const char *name = luaL_checkstring(L, arg);
    if (strcasecmp(name, "left") == 0)   return SDL_BUTTON(SDL_BUTTON_LEFT);
    if (strcasecmp(name, "right") == 0)  return SDL_BUTTON(SDL_BUTTON_RIGHT);
    if (strcasecmp(name, "middle") == 0) return SDL_BUTTON(SDL_BUTTON_MIDDLE);
    return 0;
}

/* mouse(botao) -- true enquanto o botao estiver pressionado.
 * botao pode ser "left", "right", "middle" ou o numero (1,2,3). */
static int l_mouse(lua_State *L) {
    Uint32 mask = mouse_button_mask(L, 1);
    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    lua_pushboolean(L, (buttons & mask) != 0);
    return 1;
}

/* mousePressed(botao) -- true apenas no frame em que o botao foi clicado. */
static int l_mouse_pressed(lua_State *L) {
    Uint32 mask = mouse_button_mask(L, 1);
    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    lua_pushboolean(L, (buttons & mask) && !(prev_mouse_buttons & mask));
    return 1;
}

/* mouseReleased(botao) -- true apenas no frame em que o botao foi solto. */
static int l_mouse_released(lua_State *L) {
    Uint32 mask = mouse_button_mask(L, 1);
    Uint32 buttons = SDL_GetMouseState(NULL, NULL);
    lua_pushboolean(L, !(buttons & mask) && (prev_mouse_buttons & mask));
    return 1;
}

/* mouseX() -- posicao X do cursor relativa a janela. */
static int l_mouse_x(lua_State *L) {
    int x;
    SDL_GetMouseState(&x, NULL);
    lua_pushinteger(L, x);
    return 1;
}

/* mouseY() -- posicao Y do cursor relativa a janela. */
static int l_mouse_y(lua_State *L) {
    int y;
    SDL_GetMouseState(NULL, &y);
    lua_pushinteger(L, y);
    return 1;
}

void register_control_functions(lua_State *L) {
    lua_register(L, "key", l_key);
    lua_register(L, "keyPressed", l_key_pressed);
    lua_register(L, "keyReleased", l_key_released);

    lua_register(L, "mouse", l_mouse);
    lua_register(L, "mousePressed", l_mouse_pressed);
    lua_register(L, "mouseReleased", l_mouse_released);
    lua_register(L, "mouseX", l_mouse_x);
    lua_register(L, "mouseY", l_mouse_y);
}
