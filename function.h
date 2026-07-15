#ifndef FUNCTION_H
#define FUNCTION_H

#include <SDL2/SDL.h>
#include <lua5.4/lua.h>

/* Estado global da engine, compartilhado entre main.c e function.c */
typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    int running;
    int width;
    int height;

    /* Cor atual usada por rect(), circle(), line().
     * Definida via color(r,g,b,a). Comeca em branco opaco. */
    Uint8 color_r;
    Uint8 color_g;
    Uint8 color_b;
    Uint8 color_a;
} Engine;

extern Engine engine;

/* Registra todas as funções da engine dentro do estado Lua,
 * para que o script (game.lua) possa chamá-las diretamente. */
void register_functions(lua_State *L);

/* Chama as funções de callback definidas pelo usuário em game.lua.
 * Cada uma verifica se a função existe antes de chamar, e trata
 * erros de execução Lua sem derrubar a engine. */
void call_create(lua_State *L);
void call_update(lua_State *L, double dt);
void call_draw(lua_State *L);

#endif
