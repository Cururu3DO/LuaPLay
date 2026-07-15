#include <stdio.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include "function.h"
#include "graphics.h"
#include "control.h"
#include "physics.h"

Engine engine = {0};

static void shutdown_engine(lua_State *L) {
    graphics_cleanup();
    control_cleanup();
    if (L) lua_close(L);
    if (engine.renderer) SDL_DestroyRenderer(engine.renderer);
    if (engine.window) SDL_DestroyWindow(engine.window);
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char *argv[]) {
    const char *script_path = (argc > 1) ? argv[1] : "game.lua";

    /* 1. Inicializa SDL2 */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "[LuaPlay] Erro ao iniciar SDL2: %s\n", SDL_GetError());
        return 1;
    }

    /* Inicializa SDL2_image com suporte a PNG e JPG (usado por image()) */
    int img_flags = IMG_INIT_PNG | IMG_INIT_JPG;
    if ((IMG_Init(img_flags) & img_flags) != img_flags) {
        fprintf(stderr, "[LuaPlay] Aviso: SDL2_image nao iniciou completamente: %s\n", IMG_GetError());
    }

    /* Cor padrao de desenho: branco opaco, para rect()/circle()/line()
     * aparecerem mesmo se o usuario esquecer de chamar color(). */
    engine.color_r = 255;
    engine.color_g = 255;
    engine.color_b = 255;
    engine.color_a = 255;

    /* 2. Inicializa Lua */
    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "[LuaPlay] Erro ao criar estado Lua.\n");
        IMG_Quit();
        SDL_Quit();
        return 1;
    }
    luaL_openlibs(L);

    /* 3. Registra as funções da engine no Lua */
    register_functions(L);
    register_graphics_functions(L);
    register_control_functions(L);
    register_physics_functions(L);

    /* 4. Executa game.lua (isso já dispara a chamada de window()) */
    if (luaL_dofile(L, script_path) != LUA_OK) {
        fprintf(stderr, "[LuaPlay] Erro ao carregar %s: %s\n", script_path, lua_tostring(L, -1));
        lua_pop(L, 1);
        shutdown_engine(L);
        return 1;
    }

    if (!engine.window) {
        fprintf(stderr, "[LuaPlay] Nenhuma janela foi criada. "
                        "Chame window(largura, altura, titulo) no seu game.lua.\n");
        shutdown_engine(L);
        return 1;
    }

    /* 5. create() */
    call_create(L);

    /* 6. Loop principal */
    engine.running = 1;
    Uint64 last_ticks = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    while (engine.running) {
        /* Guarda o estado do teclado/mouse do frame anterior,
         * usado por keyPressed()/keyReleased()/mousePressed()/mouseReleased() */
        control_begin_frame();

        /* Eventos */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                engine.running = 0;
            }
        }

        /* Delta time */
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = (double)(now - last_ticks) / (double)freq;
        last_ticks = now;

        /* update(dt) */
        call_update(L, dt);

        /* fisica: gravidade + colisao, depois que o usuario ja definiu
         * velocidades em update(), e antes de desenhar a posicao final */
        physics_step(dt);

        /* draw() */
        SDL_SetRenderDrawColor(engine.renderer, 0, 0, 0, 255);
        SDL_RenderClear(engine.renderer);

        call_draw(L);

        SDL_RenderPresent(engine.renderer);
    }

    /* 7. Finaliza a engine */
    shutdown_engine(L);
    return 0;
}
