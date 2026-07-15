#ifndef CONTROL_H
#define CONTROL_H

#include <lua5.4/lua.h>

/* Registra as funções de teclado e mouse (key, keyPressed, keyReleased,
 * mouse, mousePressed, mouseReleased, mouseX, mouseY) como globais no Lua. */
void register_control_functions(lua_State *L);

/* Deve ser chamada uma vez por frame, ANTES de processar os eventos SDL,
 * para guardar o estado do frame anterior e permitir detectar
 * keyPressed()/keyReleased()/mousePressed()/mouseReleased(). */
void control_begin_frame(void);

/* Libera o cache interno de nomes de tecla. Chamar uma vez ao encerrar a engine. */
void control_cleanup(void);

#endif
