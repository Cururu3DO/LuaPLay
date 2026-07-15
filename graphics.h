#ifndef GRAPHICS_H
#define GRAPHICS_H

#include <lua5.4/lua.h>

/* Registra as funções gráficas (clear, color, rect, circle, line, image)
 * como globais dentro do estado Lua. */
void register_graphics_functions(lua_State *L);

/* Libera todas as texturas de imagem carregadas em cache.
 * Deve ser chamada uma vez, ao encerrar a engine. */
void graphics_cleanup(void);

#endif
