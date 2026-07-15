#ifndef PHYSICS_H
#define PHYSICS_H

#include <lua5.4/lua.h>

/* Registra as funções de física (body, position, velocity, gravity,
 * onGround, collide, overlap, destroyBody) como globais no Lua. */
void register_physics_functions(lua_State *L);

/* Avança a simulação em dt segundos: aplica gravidade, move os corpos
 * pela velocidade e resolve colisões entre eles. Deve ser chamada uma
 * vez por frame, DEPOIS de update(dt) e ANTES de draw(). */
void physics_step(double dt);

#endif
