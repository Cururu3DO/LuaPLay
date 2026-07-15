#include <stdio.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include "function.h"

/* ---------------------------------------------------------------------
 * Funções expostas ao Lua (chamadas pelo usuário dentro de game.lua)
 * --------------------------------------------------------------------- */

/* window(largura, altura, titulo) */
static int l_window(lua_State *L) {
    int w = (int)luaL_checknumber(L, 1);
    int h = (int)luaL_checknumber(L, 2);
    const char *title = luaL_optstring(L, 3, "LuaPlay");

    if (engine.window) {
        fprintf(stderr, "[LuaPlay] window() ja foi chamada, ignorando nova chamada.\n");
        return 0;
    }

    engine.width = w;
    engine.height = h;

    engine.window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        w, h,
        SDL_WINDOW_SHOWN
    );

    if (!engine.window) {
        fprintf(stderr, "[LuaPlay] Erro ao criar janela: %s\n", SDL_GetError());
        return luaL_error(L, "falha ao criar janela SDL2");
    }

    engine.renderer = SDL_CreateRenderer(
        engine.window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!engine.renderer) {
        /* Fallback: nem toda plataforma tem um driver acelerado disponivel
         * (ex: algumas VMs, WSL, containers). Tenta renderer por software. */
        fprintf(stderr, "[LuaPlay] Renderer acelerado indisponivel (%s), tentando modo software...\n",
                SDL_GetError());
        engine.renderer = SDL_CreateRenderer(engine.window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!engine.renderer) {
        fprintf(stderr, "[LuaPlay] Erro ao criar renderer: %s\n", SDL_GetError());
        return luaL_error(L, "falha ao criar renderer SDL2");
    }

    return 0;
}

/* Tabela de funções da engine registradas como globais em Lua */
void register_functions(lua_State *L) {
    lua_register(L, "window", l_window);
}

/* ---------------------------------------------------------------------
 * Chamada segura de funções Lua definidas pelo usuário (create/update/draw)
 * --------------------------------------------------------------------- */

/* Empilha a função global 'name' e retorna 1 se ela existe, 0 caso contrário. */
static int push_callback(lua_State *L, const char *name) {
    lua_getglobal(L, name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    return 1;
}

/* Executa a função no topo da pilha com nargs argumentos já empilhados.
 * Em caso de erro, imprime a mensagem e limpa a pilha, sem encerrar a engine. */
static void call_protected(lua_State *L, int nargs, const char *name) {
    if (lua_pcall(L, nargs, 0, 0) != LUA_OK) {
        fprintf(stderr, "[LuaPlay] Erro em %s(): %s\n", name, lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

void call_create(lua_State *L) {
    if (!push_callback(L, "create")) return;
    call_protected(L, 0, "create");
}

void call_update(lua_State *L, double dt) {
    if (!push_callback(L, "update")) return;
    lua_pushnumber(L, dt);
    call_protected(L, 1, "update");
}

void call_draw(lua_State *L) {
    if (!push_callback(L, "draw")) return;
    call_protected(L, 0, "draw");
}
