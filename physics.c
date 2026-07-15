#include <math.h>
#include <string.h>

#include <lua5.4/lauxlib.h>
#include <lua5.4/lualib.h>

#include "physics.h"

/* ---------------------------------------------------------------------
 * Fisica simples: cada "body" e um retangulo (AABB - Axis Aligned
 * Bounding Box) com posicao e velocidade. Corpos "estaticos" (isStatic)
 * nunca se movem (chao, paredes); corpos normais sofrem gravidade e
 * colidem contra todos os outros corpos.
 * --------------------------------------------------------------------- */
#define MAX_BODIES 128

typedef struct {
    int active;
    int is_static;
    double x, y, w, h;
    double vx, vy;
    int use_gravity;
    int on_ground;
} Body;

static Body bodies[MAX_BODIES];
static int body_count = 0;

static double gravity_value = 900.0; /* pixels por segundo^2 */

/* collided[i][j] = 1 se os corpos i e j se sobrepuseram no ultimo passo
 * de fisica. Resetada a cada physics_step(). Usada por collide(). */
static char collided[MAX_BODIES][MAX_BODIES];

/* ---------------------------------------------------------------------
 * Simulacao
 * --------------------------------------------------------------------- */

/* Aplica a correcao 'signed_amount' num corpo dinamico ao longo do eixo
 * indicado, zera a velocidade correspondente e marca on_ground quando a
 * correcao empurrou o corpo para cima (ou seja, ele estava caindo sobre
 * algo e ficou apoiado nele). */
static void apply_axis_correction(Body *body, double signed_amount, char axis) {
    if (axis == 'x') {
        body->x += signed_amount;
        body->vx = 0;
    } else {
        body->y += signed_amount;
        body->vy = 0;
        if (signed_amount < 0) {
            body->on_ground = 1;
        }
    }
}

static void resolve_pair(int i, int j) {
    Body *a = &bodies[i];
    Body *b = &bodies[j];
    if (!a->active || !b->active) return;

    double a_cx = a->x + a->w / 2.0, a_cy = a->y + a->h / 2.0;
    double b_cx = b->x + b->w / 2.0, b_cy = b->y + b->h / 2.0;

    double dx = a_cx - b_cx;
    double dy = a_cy - b_cy;

    double overlap_x = (a->w / 2.0 + b->w / 2.0) - fabs(dx);
    double overlap_y = (a->h / 2.0 + b->h / 2.0) - fabs(dy);

    if (overlap_x <= 0 || overlap_y <= 0) return; /* nao esta sobrepondo */

    collided[i][j] = 1;
    collided[j][i] = 1;

    if (a->is_static && b->is_static) return; /* nada pra mover */

    if (overlap_x < overlap_y) {
        double corr = (dx > 0 ? 1.0 : -1.0) * overlap_x;
        if (a->is_static) {
            apply_axis_correction(b, -corr, 'x');
        } else if (b->is_static) {
            apply_axis_correction(a, corr, 'x');
        } else {
            apply_axis_correction(a, corr / 2.0, 'x');
            apply_axis_correction(b, -corr / 2.0, 'x');
        }
    } else {
        double corr = (dy > 0 ? 1.0 : -1.0) * overlap_y;
        if (a->is_static) {
            apply_axis_correction(b, -corr, 'y');
        } else if (b->is_static) {
            apply_axis_correction(a, corr, 'y');
        } else {
            apply_axis_correction(a, corr / 2.0, 'y');
            apply_axis_correction(b, -corr / 2.0, 'y');
        }
    }
}

void physics_step(double dt) {
    /* reseta flags do frame anterior */
    for (int i = 0; i < body_count; i++) {
        if (bodies[i].active) bodies[i].on_ground = 0;
    }
    memset(collided, 0, sizeof(collided));

    /* aplica gravidade e integra posicao */
    for (int i = 0; i < body_count; i++) {
        Body *b = &bodies[i];
        if (!b->active || b->is_static) continue;

        if (b->use_gravity) {
            b->vy += gravity_value * dt;
        }
        b->x += b->vx * dt;
        b->y += b->vy * dt;
    }

    /* resolve colisoes entre todos os pares de corpos ativos */
    for (int i = 0; i < body_count; i++) {
        if (!bodies[i].active) continue;
        for (int j = i + 1; j < body_count; j++) {
            if (!bodies[j].active) continue;
            resolve_pair(i, j);
        }
    }
}

/* ---------------------------------------------------------------------
 * Funcoes expostas ao Lua
 * --------------------------------------------------------------------- */

/* Valida um id de corpo vindo do Lua (1-based) e devolve o indice
 * interno (0-based), ou -1 se invalido. */
static int check_body_id(lua_State *L, int arg) {
    int id = (int)luaL_checkinteger(L, arg);
    int idx = id - 1;
    if (idx < 0 || idx >= body_count || !bodies[idx].active) {
        luaL_error(L, "id de corpo invalido: %d", id);
        return -1;
    }
    return idx;
}

/* body(x, y, w, h, isStatic) -- cria um novo corpo e retorna seu id.
 * isStatic e opcional (padrao false = afetado por gravidade/colisao). */
static int l_body(lua_State *L) {
    if (body_count >= MAX_BODIES) {
        return luaL_error(L, "limite de %d corpos de fisica atingido", MAX_BODIES);
    }

    double x = luaL_checknumber(L, 1);
    double y = luaL_checknumber(L, 2);
    double w = luaL_checknumber(L, 3);
    double h = luaL_checknumber(L, 4);
    int is_static = lua_isboolean(L, 5) ? lua_toboolean(L, 5) : 0;

    int idx = body_count++;
    Body *b = &bodies[idx];
    b->active = 1;
    b->is_static = is_static;
    b->x = x; b->y = y; b->w = w; b->h = h;
    b->vx = 0; b->vy = 0;
    b->use_gravity = !is_static;
    b->on_ground = 0;

    lua_pushinteger(L, idx + 1);
    return 1;
}

/* destroyBody(id) -- remove um corpo da simulacao. */
static int l_destroy_body(lua_State *L) {
    int idx = check_body_id(L, 1);
    bodies[idx].active = 0;
    return 0;
}

/* position(id, [x, y]) -- define (opcional) e retorna a posicao do corpo. */
static int l_position(lua_State *L) {
    int idx = check_body_id(L, 1);
    if (lua_gettop(L) >= 3) {
        bodies[idx].x = luaL_checknumber(L, 2);
        bodies[idx].y = luaL_checknumber(L, 3);
    }
    lua_pushnumber(L, bodies[idx].x);
    lua_pushnumber(L, bodies[idx].y);
    return 2;
}

/* velocity(id, [vx, vy]) -- define (opcional) e retorna a velocidade do corpo. */
static int l_velocity(lua_State *L) {
    int idx = check_body_id(L, 1);
    if (lua_gettop(L) >= 3) {
        bodies[idx].vx = luaL_checknumber(L, 2);
        bodies[idx].vy = luaL_checknumber(L, 3);
    }
    lua_pushnumber(L, bodies[idx].vx);
    lua_pushnumber(L, bodies[idx].vy);
    return 2;
}

/* gravity([g]) -- define (opcional) e retorna a gravidade global (px/s^2). */
static int l_gravity(lua_State *L) {
    if (lua_gettop(L) >= 1) {
        gravity_value = luaL_checknumber(L, 1);
    }
    lua_pushnumber(L, gravity_value);
    return 1;
}

/* onGround(id) -- true se o corpo esta apoiado em cima de outro corpo. */
static int l_on_ground(lua_State *L) {
    int idx = check_body_id(L, 1);
    lua_pushboolean(L, bodies[idx].on_ground);
    return 1;
}

/* collide(id1, id2) -- true se os dois corpos se sobrepuseram no ultimo
 * passo de fisica (chamada apos physics_step, reflete o frame atual). */
static int l_collide(lua_State *L) {
    int i = check_body_id(L, 1);
    int j = check_body_id(L, 2);
    lua_pushboolean(L, collided[i][j]);
    return 1;
}

/* overlap(id1, id2) -- teste de sobreposicao AABB imediato, sem depender
 * do passo de fisica. Util para zonas de gatilho (ex: pegar uma moeda),
 * inclusive entre corpos estaticos que nunca disparariam collide(). */
static int l_overlap(lua_State *L) {
    int i = check_body_id(L, 1);
    int j = check_body_id(L, 2);
    Body *a = &bodies[i];
    Body *b = &bodies[j];

    int overlap_x = a->x < b->x + b->w && a->x + a->w > b->x;
    int overlap_y = a->y < b->y + b->h && a->y + a->h > b->y;

    lua_pushboolean(L, overlap_x && overlap_y);
    return 1;
}

void register_physics_functions(lua_State *L) {
    lua_register(L, "body", l_body);
    lua_register(L, "destroyBody", l_destroy_body);
    lua_register(L, "position", l_position);
    lua_register(L, "velocity", l_velocity);
    lua_register(L, "gravity", l_gravity);
    lua_register(L, "onGround", l_on_ground);
    lua_register(L, "collide", l_collide);
    lua_register(L, "overlap", l_overlap);
}
