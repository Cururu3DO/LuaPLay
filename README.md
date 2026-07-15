# LuaPlay

Uma engine 2D escrita em **C puro**, com **Lua 5.4** como linguagem de script. O objetivo é ser mais simples que o Love2D: sem imports, sem objetos complexos, sem namespaces — só funções pequenas e diretas.

```lua
window(800, 600, "Meu Jogo")

function create()
end

function update(dt)
end

function draw()
end
```

Você escreve o jogo. A engine cuida do resto.

---

## Filosofia

- Simplicidade acima de tudo
- Poucas dependências
- API extremamente intuitiva (nomes curtos, sem namespaces, sem tabelas de configuração)
- Código organizado em módulos pequenos, cada um com uma responsabilidade
- Não compete com Unity/Godot nem substitui o Love2D — é pequena, rápida e simples de propósito

---

## Pré-requisitos

| Dependência | Para quê |
|---|---|
| GCC ou Clang | compilar a engine |
| Make | automatizar a build |
| SDL2 | janela, renderização, eventos de teclado/mouse |
| SDL2_image | carregar imagens (PNG, JPG, etc.) via `image()` |
| Lua 5.4 (dev) | interpretar os scripts do jogo |

### Instalando no Ubuntu / Debian

```bash
sudo apt update
sudo apt install build-essential libsdl2-dev libsdl2-image-dev liblua5.4-dev
```

### Instalando no Fedora

```bash
sudo dnf install gcc make SDL2-devel SDL2_image-devel lua-devel
```

### Instalando no Arch Linux

```bash
sudo pacman -S base-devel sdl2 sdl2_image lua
```

### Instalando no macOS (Homebrew)

```bash
brew install sdl2 sdl2_image lua
```

> No macOS, talvez seja necessário ajustar o `Makefile` para apontar para os caminhos do Homebrew caso o `pkg-config` não encontre as libs automaticamente.

---

## Compilando e rodando

```bash
make          # compila a engine (gera o binário "luaplay")
make run      # compila e já executa
make clean    # remove os arquivos gerados pela build
```

Por padrão a engine executa o `game.lua` da pasta atual. Para rodar outro arquivo:

```bash
./luaplay caminho/para/outro_jogo.lua
```

---

## Estrutura do projeto

```
LuaPlay/
│
├── main.c          # inicializa SDL2 e Lua, roda o loop principal
├── function.c/.h    # registra window() e chama create()/update()/draw()
├── graphics.c/.h    # desenho: clear, color, rect, circle, line, image
├── control.c/.h     # entrada: teclado e mouse
├── physics.c/.h     # gravidade e colisão (AABB)
├── game.lua          # o jogo do usuário (executado automaticamente)
└── Makefile
```

Cada módulo `.c` registra suas próprias funções Lua e é independente dos outros — `graphics.c` não sabe nada sobre `physics.c`, por exemplo. Isso deixa fácil adicionar novos módulos (áudio, câmera, cenas) sem mexer no que já existe.

### Fluxo de execução

```
main() inicia SDL2 e Lua
  → registra as funções de todos os módulos
  → executa game.lua (isso já chama window())
  → chama create()
  → entra no loop principal:
        eventos → update(dt) → física (gravidade/colisão) → draw() → apresenta o frame
  → repete até a janela ser fechada
```

---

## Documentação da API

### Janela e ciclo de vida

| Função | Descrição |
|---|---|
| `window(largura, altura, titulo)` | cria a janela do jogo. Chame uma única vez, no topo do `game.lua`. |
| `create()` | chamada uma vez, no início do jogo. Defina aqui (função opcional que você cria em Lua). |
| `update(dt)` | chamada uma vez por frame, antes de desenhar. `dt` é o tempo em segundos desde o frame anterior. |
| `draw()` | chamada uma vez por frame, para desenhar na tela. |

### Gráficos (`graphics.c`)

| Função | Descrição |
|---|---|
| `clear(r, g, b, a)` | limpa a tela com uma cor. Todos os argumentos são opcionais (padrão preto opaco). |
| `color(r, g, b, a)` | define a cor usada pelos próximos `rect()`, `circle()` e `line()`. `a` é opcional (padrão 255). |
| `rect(x, y, w, h)` | retângulo preenchido. |
| `circle(x, y, raio)` | círculo preenchido, centrado em `(x, y)`. |
| `line(x1, y1, x2, y2)` | linha reta. |
| `image(caminho, x, y, escala)` | desenha uma imagem (PNG/JPG). `escala` é opcional (padrão 1). A imagem é carregada e cacheada automaticamente na primeira chamada. |
| `imageSize(caminho)` | retorna `largura, altura` da imagem. |

Todas as coordenadas aceitam número inteiro ou decimal.

### Entrada (`control.c`)

| Função | Descrição |
|---|---|
| `key(nome)` | `true` enquanto a tecla estiver pressionada. Ex: `key("left")`, `key("space")`, `key("a")`. |
| `keyPressed(nome)` | `true` só no frame exato em que a tecla foi apertada. |
| `keyReleased(nome)` | `true` só no frame exato em que a tecla foi solta. |
| `mouse(botao)` | `true` enquanto o botão do mouse estiver pressionado. `botao` pode ser `"left"`, `"right"`, `"middle"` ou um número (1, 2, 3). |
| `mousePressed(botao)` / `mouseReleased(botao)` | detecção de clique/soltura no frame exato. |
| `mouseX()` / `mouseY()` | posição do cursor relativa à janela. |

### Física (`physics.c`)

| Função | Descrição |
|---|---|
| `body(x, y, w, h, isStatic)` | cria um corpo físico retangular (AABB) e retorna um id. `isStatic` é opcional (padrão `false`); corpos estáticos nunca se movem (chão, paredes) mas bloqueiam os outros. |
| `destroyBody(id)` | remove um corpo da simulação. |
| `position(id, x, y)` | lê a posição do corpo. Se `x, y` forem passados, também a define. |
| `velocity(id, vx, vy)` | lê a velocidade do corpo. Se `vx, vy` forem passados, também a define. |
| `gravity(g)` | lê a gravidade global em px/s² (padrão `900`). Se `g` for passado, também a define. |
| `onGround(id)` | `true` se o corpo está apoiado em cima de outro corpo. |
| `collide(id1, id2)` | `true` se os dois corpos colidiram no último passo de física. |
| `overlap(id1, id2)` | teste de sobreposição imediato entre dois corpos, útil para zonas de gatilho (ex: pegar um item) sem esperar a resolução física. |

A física roda automaticamente uma vez por frame, depois do seu `update(dt)` e antes do `draw()` — você só precisa criar corpos com `body()` e mexer na velocidade deles; a engine cuida de gravidade e colisão sozinha.

---

## Exemplo completo (plataforma simples)

```lua
window(800, 600, "Meu Jogo")

local player, floor

function create()
    floor  = body(0, 560, 800, 40, true)    -- chao estatico
    player = body(380, 100, 40, 40, false)  -- jogador, cai por gravidade
end

function update(dt)
    local vx, vy = velocity(player)

    vx = 0
    if key("left")  then vx = -200 end
    if key("right") then vx = 200 end

    if keyPressed("space") and onGround(player) then
        vy = -500  -- pulo
    end

    velocity(player, vx, vy)
end

function draw()
    clear(30, 30, 40)

    color(120, 120, 120)
    local fx, fy = position(floor)
    rect(fx, fy, 800, 40)

    color(80, 200, 255)
    local px, py = position(player)
    rect(px, py, 40, 40)
end
```

---

## Roadmap

Já implementado: janela, ciclo de vida, gráficos básicos, imagens, teclado/mouse, gravidade e colisão.

Planejado:

- Áudio (`playSound()`, `playMusic()`)
- Câmera (`camera()`, `cameraFollow()`)
- Texto e fontes (`font()`, `text()`)
- Cenas (`scene()`, `loadScene()`)
- Timers (`timer()`, `wait()`)
- Sprites e animação (`sprite()`, `animation()`)
