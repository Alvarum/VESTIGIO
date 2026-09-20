# Manual profundo para construir desde cero un FPS estilo Doom en C + raylib

## La arquitectura que vamos a construir

Tu idea es, de hecho, **un proyecto excelente para aprender programación de videojuegos de verdad**: no solamente mover entidades con una API 3D, sino entender qué ocurre entre un mapa 2D, una cámara y los píxeles que finalmente aparecen en pantalla.

La estrategia que te recomiendo es deliberadamente distinta a hacer algo como esto:

```c
BeginMode3D(camera);
DrawCube(...);
DrawModel(...);
EndMode3D();
```

Eso serviría para crear **un FPS con estética Doom**, pero raylib resolvería por ti casi todo lo que precisamente quieres aprender.

Vamos a utilizar raylib como **capa de plataforma**:

```text
┌────────────────────────────────────────────┐
│                 TU JUEGO                   │
│                                            │
│  Gameplay                                  │
│  ├── jugador                               │
│  ├── armas                                 │
│  ├── enemigos                              │
│  ├── jefes                                 │
│  ├── items                                 │
│  └── triggers                              │
│                                            │
│  Motor                                     │
│  ├── renderer 2.5D        ← hecho por ti   │
│  ├── mapas                ← hecho por ti   │
│  ├── colisiones           ← hecho por ti   │
│  ├── BSP/portales         ← hecho por ti   │
│  ├── IA                   ← hecho por ti   │
│  └── estados              ← hecho por ti   │
│                                            │
│  Plataforma                                 │
│  └── raylib                                │
│      ├── ventana                           │
│      ├── input                             │
│      ├── audio                             │
│      ├── lectura de archivos               │
│      └── mostrar tu framebuffer            │
└────────────────────────────────────────────┘
```

Esto encaja perfectamente con la filosofía de raylib: el propio proyecto se presenta como una biblioteca para programar videojuegos sin editor ni abstracciones visuales pesadas, y recomienda aprender principalmente mediante su cheatsheet y su colección de ejemplos. La referencia oficial actual identifica la API estable como **raylib 6.0**; el changelog oficial todavía marca 6.0, publicada el 23 de abril de 2026, como “Current Release”, mientras que la entrada 6.2 conserva una fecha provisional `xx Sep 2026`. Por eso basaría el proyecto en **raylib 6.0** hasta que 6.2 aparezca formalmente publicada. citeturn14search1turn15view0turn15view1

### Una distinción importantísima: Doom no es simplemente un raycaster

Aquí conviene corregir una confusión extremadamente común.

Un motor tipo **Wolfenstein 3D** suele explicarse así:

```text
cada columna de pantalla
        ↓
lanzar un rayo
        ↓
encontrar pared
        ↓
calcular distancia
        ↓
dibujar columna de pared
```

Ese sistema es ideal como primer ejercicio.

Pero el Doom original hace algo más sofisticado.

Su renderer recorre un **árbol BSP** que divide el mapa en subsecciones convexas; cada subsector contiene segmentos de pared, se procesan sus sectores, sprites, planos de suelo/techo y segmentos visibles. El código original de id Software llama recursivamente a `R_RenderBSPNode()`, visita primero el lado donde está el observador y sólo procesa el otro lado cuando su bounding box sigue siendo potencialmente visible. citeturn16view1turn16view2

Las estructuras originales también muestran explícitamente:

- `sector_t`, con altura de suelo, altura de techo, texturas, iluminación y propiedades;
- `subsector_t`, descrito como una hoja convexa del BSP;
- `seg_t`, con los extremos del segmento y sectores frontal/trasero;
- `node_t`, con una recta de partición, bounding boxes y dos hijos. citeturn16view4turn16view5turn16view6turn16view7

Además, el Doom original procesa suelo y techo mediante **planos/visplanes y spans**, no simplemente dibujando triángulos 3D convencionales. citeturn16view3

Por tanto, nuestro objetivo final será:

> **un renderer 2.5D por software, basado en sectores, paredes, portales y finalmente BSP, presentado en pantalla mediante raylib.**

No vamos a intentar reproducir cada detalle del Doom original ni copiar su código. Vamos a **aprender sus ideas y reconstruirlas nosotros mismos**.

### Por qué se llama 2.5D

Nuestro mundo tendrá coordenadas horizontales:

\[
(x,y)
\]

y una altura:

\[
z
\]

pero un sector tendrá solamente una altura de suelo y una altura de techo:

```text
ceiling_z
────────────────────
        sector

        jugador

────────────────────
floor_z
```

Es justamente el tipo de representación visible en `sector_t` del Doom original: un valor de `floorheight` y otro de `ceilingheight` por sector. De esta representación se desprende la clásica limitación de este tipo de geometría: no existe arbitrariamente una habitación completa directamente encima de otra dentro del mismo espacio XY sin extensiones adicionales. citeturn16view4

Eso resulta maravilloso para aprender porque reduce mucho el problema 3D:

```text
Mundo lógico:

        Z
        ↑
        │
        │    jugador
        │
        └──────────────→ X,Y


Geometría principal:

vista superior
       B────────C
       │        │
       │ sector │
       │        │
       A────────D

más:

floor_z   = 0
ceiling_z = 128
```

### La ruta pedagógica

No te recomiendo comenzar escribiendo un BSP.

Sería como intentar aprender álgebra construyendo primero un compilador simbólico.

Vamos a avanzar así:

| Etapa | Motor |
|---|---|
| Ventana | raylib únicamente |
| Framebuffer | tus propios píxeles |
| Mapa superior | geometría 2D |
| Cámara | transformación mundo → cámara |
| Primera habitación | paredes planas |
| Texturizado | columnas y UV |
| Suelo/techo | plane casting |
| Sectores | alturas diferentes |
| Portales | habitaciones conectadas |
| Sprites | enemigos/items |
| Colisión | círculo contra segmentos |
| Combate | hitscan/proyectiles |
| IA | FSM + visión + sonido |
| Especiales | puertas/lifts/triggers |
| BSP | visibilidad eficiente |
| Juego | niveles, jefes, HUD, menús |
| Producción | audio, saves, configuración, pulido |

Lo importante es que **cada etapa produce algo jugable y observable**.

Nunca deberías estar tres meses programando infraestructura sin poder caminar por una habitación.

---

## Base del proyecto, raylib y el bucle del juego

Raylib es especialmente conveniente aquí porque podemos usar sus servicios de ventana, input, audio y presentación mientras mantenemos nuestro renderer completamente independiente. El proyecto oficial incluso proporciona un template pequeño para juegos en C puro con soporte para CMake, Visual Studio y otros sistemas de compilación. citeturn15view2

### Estructura recomendada

No pongas todo dentro de `main.c`.

Desde temprano separaría el proyecto así:

```text
doomlike/
├── CMakeLists.txt
│
├── external/
│   └── raylib/
│
├── assets/
│   ├── textures/
│   ├── sprites/
│   ├── sounds/
│   ├── music/
│   ├── fonts/
│   └── maps/
│
├── src/
│   ├── main.c
│   │
│   ├── core/
│   │   ├── game.c
│   │   ├── game.h
│   │   ├── input.c
│   │   └── input.h
│   │
│   ├── math/
│   │   ├── vec2.c
│   │   ├── vec2.h
│   │   ├── geometry.c
│   │   └── geometry.h
│   │
│   ├── renderer/
│   │   ├── renderer.c
│   │   ├── renderer.h
│   │   ├── framebuffer.c
│   │   ├── framebuffer.h
│   │   ├── wall.c
│   │   ├── plane.c
│   │   └── sprite.c
│   │
│   ├── world/
│   │   ├── map.c
│   │   ├── map.h
│   │   ├── collision.c
│   │   ├── collision.h
│   │   ├── bsp.c
│   │   └── bsp.h
│   │
│   ├── gameplay/
│   │   ├── player.c
│   │   ├── weapon.c
│   │   ├── entity.c
│   │   ├── enemy.c
│   │   ├── boss.c
│   │   └── trigger.c
│   │
│   ├── audio/
│   │   ├── audio.c
│   │   └── audio.h
│   │
│   └── ui/
│       ├── hud.c
│       ├── menu.c
│       └── menu.h
│
└── tools/
    ├── map_compiler/
    └── map_editor/
```

No necesitas crear todos esos archivos inmediatamente. La estructura representa **hacia dónde crecerá el programa**.

Al principio bastan:

```text
main.c
game.c
game.h
framebuffer.c
framebuffer.h
renderer.c
renderer.h
```

### CMake sencillo

Mantendría raylib fijada a su tag estable y agregada como dependencia del proyecto. El template oficial también propone trabajar explícitamente con la rama/tag correspondiente de raylib y ofrece CMake como sistema soportado. citeturn15view2

Un CMake mínimo puede conceptualizarse así:

```cmake
# Define la versión mínima de CMake que utilizará el proyecto.
cmake_minimum_required(VERSION 3.24)

# Declara un proyecto escrito exclusivamente en C.
project(hellforge LANGUAGES C)

# Elegimos C11 como base estable y suficientemente moderna.
set(CMAKE_C_STANDARD 11)

# Impedimos que CMake silenciosamente use una versión inferior.
set(CMAKE_C_STANDARD_REQUIRED ON)

# Evitamos extensiones específicas de GCC cuando no son necesarias.
set(CMAKE_C_EXTENSIONS OFF)

# Raylib vive dentro de external/raylib.
add_subdirectory(external/raylib)

# Construimos nuestro programa.
add_executable(
    hellforge
    src/main.c
    src/core/game.c
    src/renderer/framebuffer.c
    src/renderer/renderer.c
)

# Permitimos incluir headers desde src/.
target_include_directories(
    hellforge
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Enlazamos nuestro programa contra raylib.
target_link_libraries(
    hellforge
    PRIVATE
    raylib
)
```

### El framebuffer: el corazón de nuestro renderer

Ésta es una de las decisiones más importantes del proyecto.

**No vamos a pedirle a raylib que dibuje las paredes.**

Tendremos un array:

```text
pixels[y * width + x]
```

donde cada elemento representa un píxel.

Por ejemplo:

```text
320 × 200 píxeles
= 64 000 píxeles
```

Tú calcularás qué color tiene cada uno.

Raylib solamente recibe el resultado:

```text
CPU
┌───────────────────┐
│ framebuffer       │
│ Color pixels[]    │
└────────┬──────────┘
         │
         │ UpdateTexture()
         ▼
GPU
┌───────────────────┐
│ Texture2D         │
└────────┬──────────┘
         │
         ▼
      pantalla
```

Esta separación coincide con el modelo de raylib: sus `Image` existen en memoria CPU mientras que `Texture2D` corresponde a datos subidos a memoria gráfica. Los ejemplos oficiales muestran precisamente la generación/manipulación de píxeles en RAM y su posterior carga como textura. citeturn15view4turn18view1

Raylib 6.0 introdujo además su propio backend de software, pero **no lo usaría para resolver nuestro renderer**: nuestro objetivo pedagógico consiste precisamente en escribir nosotros la proyección, rasterización de paredes, planos, sprites y visibilidad. citeturn18view3

Una estructura mínima:

```c
// Incluye los tipos Image, Texture2D y Color proporcionados por raylib.
#include "raylib.h"

// Representa nuestra pantalla lógica de baja resolución.
typedef struct Framebuffer {
    // Mantiene los píxeles en RAM para que nuestro renderer los modifique.
    Image image;

    // Copia residente en GPU utilizada únicamente para presentar la imagen.
    Texture2D texture;

    // Ancho lógico del framebuffer.
    int width;

    // Alto lógico del framebuffer.
    int height;
} Framebuffer;
```

La inicialización conceptual:

```c
// Crea una imagen CPU completamente negra con las dimensiones internas.
fb->image = GenImageColor(width, height, BLACK);

// Sube inicialmente esos píxeles a una textura de la GPU.
fb->texture = LoadTextureFromImage(fb->image);

// Fuerza muestreo nearest-neighbour para conservar píxeles definidos.
SetTextureFilter(fb->texture, TEXTURE_FILTER_POINT);

// Conserva las dimensiones para indexar correctamente el buffer.
fb->width = width;

// Conserva la altura por la misma razón.
fb->height = height;
```

Durante cada frame:

```c
// Obtiene acceso tipado al bloque RGBA contenido por Image.
Color *pixels = (Color *)fb->image.data;

// Nuestro renderer modifica pixels[] completamente en CPU.
RenderWorld(game, pixels, fb->width, fb->height);

// Copia el framebuffer terminado hacia la textura gráfica.
UpdateTexture(fb->texture, pixels);
```

El ejemplo oficial de raw image data muestra precisamente un buffer `Color *` dinámico y la creación de imágenes/texturas desde información residente en CPU. citeturn18view1

Yo comenzaría con:

```text
INTERNAL_WIDTH  = 320
INTERNAL_HEIGHT = 200
```

No porque el motor dependa de esos números, sino porque **64 000 píxeles son suficientemente pocos para experimentar cómodamente con software rendering**, y visualmente entrega una estética retro inmediata.

Haz que estas dimensiones sean configurables.

### Escalado hacia la ventana

Tu renderer piensa que la pantalla mide:

```text
320 × 200
```

aunque la ventana realmente mida:

```text
1280 × 800
1920 × 1080
2560 × 1440
...
```

Calculas:

\[
scale =
\min
\left(
\frac{windowWidth}{320},
\frac{windowHeight}{200}
\right)
\]

Para pixel art conviene tomar la parte entera:

\[
scale = \lfloor scale \rfloor
\]

y centrar el resultado.

Eso produce letterboxing cuando las proporciones no coinciden.

Raylib proporciona precisamente render targets y `DrawTexturePro()` para dibujar una textura hacia un rectángulo de destino; su ejemplo oficial demuestra ese patrón de renderizar internamente y presentar después la textura final. citeturn15view3

### Separa actualización y renderizado

Tu juego debe tener conceptualmente:

```c
GameUpdate(...);
GameRender(...);
```

Nunca:

```c
GameDoEverything(...);
```

Queremos:

```text
INPUT
  ↓
SIMULACIÓN
  ↓
RENDER
  ↓
PRESENTACIÓN
```

Y aún mejor:

```text
tiempo real
   │
   ▼
acumulador
   │
   ├── update fijo
   ├── update fijo
   └── ...
        │
        ▼
      render
```

Usaría una simulación fija de:

\[
\Delta t = \frac{1}{60}\;s
\]

mientras que la pantalla puede renderizar a otra velocidad.

El algoritmo clásico del acumulador:

```text
accumulator += tiempo_real_transcurrido

mientras accumulator >= fixed_dt:
    update(fixed_dt)
    accumulator -= fixed_dt

render()
```

¿Por qué?

Supón movimiento:

\[
p_{nuevo} = p_{viejo} + velocidad \cdot \Delta t
\]

Si tus colisiones, IA, puertas y proyectiles reciben siempre el mismo `dt`, su comportamiento resulta mucho más fácil de razonar, reproducir y depurar.

### Input por acciones, no por teclas

No hagas esto por toda tu lógica:

```c
if (IsKeyDown(KEY_W)) {
    ...
}
```

Haz:

```text
ACTION_MOVE_FORWARD
ACTION_MOVE_BACKWARD
ACTION_STRAFE_LEFT
ACTION_STRAFE_RIGHT
ACTION_FIRE
ACTION_USE
ACTION_NEXT_WEAPON
ACTION_PAUSE
```

Y luego:

```text
W             ─┐
↑              ├─→ MOVE_FORWARD
gamepad stick ─┘
```

El propio repositorio de raylib tiene un ejemplo oficial dedicado a desacoplar las acciones del juego de teclas/botones concretos y permitir remapping. citeturn18view2

Esto hará que posteriormente tu menú de controles sea casi trivial.

### Estados globales del juego

También necesitas una máquina de estados para el programa:

```c
typedef enum GameMode {
    GAME_MODE_TITLE,
    GAME_MODE_MAIN_MENU,
    GAME_MODE_OPTIONS,
    GAME_MODE_LOADING,
    GAME_MODE_PLAYING,
    GAME_MODE_PAUSED,
    GAME_MODE_GAME_OVER,
    GAME_MODE_CREDITS
} GameMode;
```

Entonces:

```text
MAIN_MENU
    │
    ├─ New Game ───────→ LOADING ──→ PLAYING
    │                                  │
    │                                  ├─ ESC → PAUSED
    │                                  │          │
    │                                  │          └─ Resume
    │                                  │
    │                                  └─ death → GAME_OVER
    │
    ├─ Options → OPTIONS
    │
    └─ Quit
```

Esta misma idea reaparecerá más adelante para enemigos, armas, puertas y jefes.

**Ésta es una de las ideas fundamentales de todo el proyecto: el videojuego entero puede entenderse como múltiples máquinas de estados que se comunican.**

---

## La matemática que necesitas dominar

Aquí está el verdadero corazón del proyecto.

La buena noticia es que **no necesitas cálculo avanzado ni álgebra lineal universitaria completa**.

Necesitarás dominar muy bien unas pocas herramientas:

| Matemática | Para qué |
|---|---|
| Vectores 2D | posiciones, direcciones, velocidad |
| Magnitud | distancias |
| Normalización | direcciones unitarias |
| Producto escalar | proyecciones y ángulos |
| Producto cruzado 2D | lados, intersecciones, BSP |
| Seno/coseno | orientación de cámara |
| Interpolación lineal | animación y clipping |
| Rectas paramétricas | raycasting |
| Proyección perspectiva | mundo → pantalla |
| Geometría de segmentos | paredes y colisiones |
| Aritmética modular | texturas |
| Grafos | sectores, sonido, pathfinding |
| A* / BFS | navegación |
| Máquinas de estados | gameplay |

Vamos una por una.

### Vectores

Una posición:

\[
P=(x,y)
\]

Una dirección:

\[
D=(d_x,d_y)
\]

Dos posiciones:

\[
A=(2,3)
\]

\[
B=(7,5)
\]

La dirección de A hacia B es:

\[
B-A=(5,2)
\]

En C:

```c
// Un vector bidimensional extremadamente pequeño y explícito.
typedef struct Vec2 {
    // Componente horizontal.
    float x;

    // Componente vertical del plano del mapa.
    float y;
} Vec2;
```

Suma:

\[
(a_x,a_y)+(b_x,b_y)
=
(a_x+b_x,a_y+b_y)
\]

Multiplicación por escalar:

\[
(x,y)s=(xs,ys)
\]

Magnitud:

\[
|v|=\sqrt{x^2+y^2}
\]

Normalización:

\[
\hat{v}=\frac{v}{|v|}
\]

Ejemplo:

\[
v=(3,4)
\]

\[
|v|=\sqrt{9+16}=5
\]

\[
\hat v=(0.6,0.8)
\]

La gran utilidad de un vector unitario es:

```text
posición += dirección_unitaria × velocidad × dt
```

La velocidad deja de depender de la longitud accidental del vector.

### Producto escalar

Para:

\[
a=(a_x,a_y)
\]

\[
b=(b_x,b_y)
\]

tenemos:

\[
a\cdot b=a_xb_x+a_yb_y
\]

Y también:

\[
a\cdot b=|a||b|\cos(\theta)
\]

Por tanto:

- positivo → apuntan aproximadamente hacia el mismo lado;
- cero → son perpendiculares;
- negativo → aproximadamente opuestos.

Lo utilizaremos **constantemente**.

Por ejemplo, para averiguar cuánto de una velocidad apunta contra una pared:

\[
v_n = v\cdot n
\]

donde `n` es la normal de la pared.

Para eliminar ese movimiento:

\[
v_{slide}
=
v-n(v\cdot n)
\]

¡Y acabas de obtener **wall sliding**!

### Producto cruzado en 2D

En 3D existe un vector cruzado. En nuestro plano XY nos interesa solamente su componente perpendicular:

\[
cross(a,b)=a_xb_y-a_yb_x
\]

Esto es absurdamente útil.

Dados:

```text
A --------→ B

         P
```

podemos calcular:

\[
cross(B-A,P-A)
\]

El signo indica de qué lado de la recta AB se encuentra P.

Ese mismo cálculo será utilizado para:

- intersección de rayos;
- clasificar geometría;
- construir el BSP;
- saber qué hijo del BSP contiene la cámara.

Es una de esas fórmulas pequeñas que terminan apareciendo por todo el motor.

### Cámara

Decidamos una convención muy clara:

```text
X,Y = plano horizontal
Z   = altura
```

Jugador:

\[
P=(p_x,p_y,p_z)
\]

Orientación horizontal:

\[
\theta
\]

Su vector hacia delante es:

\[
F=(\cos\theta,\sin\theta)
\]

Y su vector hacia la derecha:

\[
R=(-\sin\theta,\cos\theta)
\]

Visualmente:

```text
                 F
                 ↑
                 │
                 │
          ←──── Player ────→ R
```

Ahora considera un vértice de pared:

\[
V=(v_x,v_y)
\]

Primero lo hacemos relativo al jugador:

\[
D=V-P
\]

\[
d_x=v_x-p_x
\]

\[
d_y=v_y-p_y
\]

Queremos averiguar:

> ¿Cuánto está hacia delante?

Producto escalar con `F`:

\[
depth
=
D\cdot F
\]

\[
depth
=
d_x\cos\theta+d_y\sin\theta
\]

Y:

> ¿Cuánto está hacia la derecha?

\[
cameraX
=
D\cdot R
\]

\[
cameraX
=
-d_x\sin\theta+d_y\cos\theta
\]

Acabas de transformar:

```text
coordenadas del mundo
        ↓
coordenadas relativas a la cámara
```

Esto es esencialmente un **cambio de base**.

### Perspectiva

Ahora viene la magia.

Imagina:

```text
                  punto
                    *
                   /|
                  / |
                 /  |
 cámara *-------+---|
              plano
             pantalla
```

Por triángulos semejantes:

\[
screenX
=
centerX +
\frac{cameraX}{depth}f
\]

donde:

\[
f=
\frac{W/2}{\tan(FOV/2)}
\]

`f` es la **distancia focal expresada en píxeles**.

Supongamos:

```text
W   = 320
FOV = 90°
```

Entonces:

\[
f=\frac{160}{\tan45^\circ}=160
\]

Un punto con:

```text
cameraX = 1
depth   = 4
```

aparece en:

\[
screenX
=
160+\frac{1}{4}(160)
\]

\[
screenX=200
\]

Eso explica intuitivamente la perspectiva:

```text
misma separación X
pero mayor profundidad
        ↓
menor desplazamiento en pantalla
```

Para altura:

\[
screenY
=
centerY
-
\frac{z-eyeZ}{depth}f_y
\]

El signo menos existe porque normalmente en pantalla:

```text
y = 0
↓
↓
↓
y = height
```

mientras que en el mundo Z crece hacia arriba.

### Una pared finalmente aparece

Supón un sector:

```text
floor_z   = 0
ceiling_z = 128
```

Y el ojo:

```text
eye_z = 64
```

Para cada extremo de una pared transformas:

```text
endpoint A → camera_right_A, depth_A
endpoint B → camera_right_B, depth_B
```

Y proyectas:

```text
screen_x_A
screen_x_B
```

Después calculas arriba y abajo:

\[
y_{top}
=
centerY-
\frac{ceilingZ-eyeZ}{depth}f
\]

\[
y_{bottom}
=
centerY-
\frac{floorZ-eyeZ}{depth}f
\]

Ya tienes:

```text
             topA ┌──────────┐ topB
                  │          │
                  │  pared   │
                  │          │
          bottomA └──────────┘ bottomB
```

No hemos usado ningún motor 3D.

**Esa pared apareció exclusivamente gracias a matemáticas que tú mismo calculaste.**

### El near plane

Existe un problema horrible cuando:

\[
depth \rightarrow 0
\]

porque:

\[
\frac{1}{depth}\rightarrow\infty
\]

Una pared que cruza detrás de la cámara puede explotar geométricamente por toda la pantalla.

Solución:

```text
near_plane = 0.01
```

Todo vértice debe cumplir:

\[
depth \ge near
\]

Pero no puedes simplemente borrar una pared si uno de sus extremos está detrás.

Debes **recortarla**.

Segmento:

\[
P(t)=A+t(B-A)
\]

Queremos el punto donde su depth sea `near`:

\[
t=
\frac{near-depth_A}
     {depth_B-depth_A}
\]

Luego interpolamos:

\[
cameraX_{new}
=
cameraX_A+t(cameraX_B-cameraX_A)
\]

También debes interpolar allí:

- posición;
- coordenada de textura;
- cualquier atributo necesario.

Esto se llama **near-plane clipping**.

Aprender clipping aquí te ahorrará una cantidad absurda de errores posteriores.

### Interpolación lineal

La fórmula:

\[
lerp(a,b,t)=a+(b-a)t
\]

con:

\[
0\le t\le1
\]

aparecerá constantemente:

```text
animaciones
puertas
plataformas
clipping
texturas
interpolación entre ticks
luces
HUD
```

Conviene escribirla una vez:

```c
// Interpola entre dos números según una fracción normalizada.
static float LerpFloat(
    const float a,
    const float b,
    const float t
)
{
    // t = 0 produce a; t = 1 produce b.
    return a + ((b - a) * t);
}
```

### Intersección rayo-segmento

Esto será tu arma hitscan, sistema de “Use”, line-of-sight y mucho más.

Rayo:

\[
R(t)=P+tD
\]

con:

\[
t\ge0
\]

Pared:

\[
S(u)=A+u(B-A)
\]

con:

\[
0\le u\le1
\]

Definamos:

\[
E=B-A
\]

Entonces:

\[
t=
\frac{cross(A-P,E)}
     {cross(D,E)}
\]

\[
u=
\frac{cross(A-P,D)}
     {cross(D,E)}
\]

Si:

```text
t >= 0
0 <= u <= 1
```

hay intersección.

Y la posición es:

\[
hit=P+tD
\]

Eso significa que **una sola función matemática** puede convertirse después en:

```text
TraceRay()
├── disparar escopeta
├── comprobar visión enemiga
├── encontrar pared apuntada
├── activar interruptor
└── detectar impactos de láser
```

Ésta es una gran lección de arquitectura:

> Construye primitivas geométricas generales; después el gameplay las reutiliza.

---

## Construcción del renderer 2.5D

Ahora ya tenemos suficiente matemática para comenzar el verdadero motor.

### Primer objetivo: una habitación sin textura

Construye un cuadrado:

```text
0 ───────────── 1
│                │
│                │
│       P        │
│                │
│                │
3 ───────────── 2
```

Con cuatro vértices:

```text
V0 = (-128, -128)
V1 = ( 128, -128)
V2 = ( 128,  128)
V3 = (-128,  128)
```

Y cuatro paredes:

```text
0 → 1
1 → 2
2 → 3
3 → 0
```

Por ahora:

```text
floor_z   = 0
ceiling_z = 128
```

Cada pared sigue exactamente este pipeline:

```text
world endpoints
      ↓
restar cámara
      ↓
transformar a camera space
      ↓
near clipping
      ↓
proyección
      ↓
clip horizontal
      ↓
rasterizar columnas
```

Tu primera versión puede dibujar un color sólido.

**Checkpoint:** debes poder caminar y girar dentro de una habitación de paredes blancas.

No continúes hasta conseguir esto.

### Rasterización por columnas

Una pared proyectada ocupa:

```text
x0 ... x1
```

Para cada columna:

```text
for x = x0 hasta x1
```

calculamos:

```text
wall_top(x)
wall_bottom(x)
```

y pintamos:

```text
for y = top hasta bottom
    pixel(x, y) = wall_color
```

Parece primitivo.

Pero ya acabas de escribir un rasterizador.

### Texturas de pared

Supón una textura:

```text
64 × 64
```

Necesitamos descubrir qué columna de ella corresponde a cada columna de pantalla.

Una primera intuición sería:

\[
u=lerp(u_0,u_1,s)
\]

donde:

\[
s=
\frac{x-x_0}{x_1-x_0}
\]

Pero eso produce deformación cuando los dos extremos de la pared tienen profundidades distintas.

¿Por qué?

Porque **la perspectiva no es lineal respecto de Z**.

Necesitamos interpolación corregida por perspectiva.

Para cada extremo tenemos:

\[
\frac{1}{z_0}
\]

\[
\frac{1}{z_1}
\]

y:

\[
\frac{u_0}{z_0}
\]

\[
\frac{u_1}{z_1}
\]

Interpolamos:

\[
invZ=
lerp
\left(
\frac1{z_0},
\frac1{z_1},
s
\right)
\]

\[
uOverZ=
lerp
\left(
\frac{u_0}{z_0},
\frac{u_1}{z_1},
s
\right)
\]

Entonces:

\[
u=
\frac{uOverZ}{invZ}
\]

y también:

\[
z=\frac1{invZ}
\]

Esto es **perspective-correct interpolation**.

Guárdalo en tu cerebro porque es una de las ideas fundamentales de rasterización 3D.

### Coordenada vertical de la textura

Para una columna determinada ya conoces:

```text
screen_top
screen_bottom
```

Así:

\[
v=
\frac{y-screenTop}
     {screenBottom-screenTop}
\]

Después:

\[
textureY=v\cdot textureHeight
\]

y:

\[
textureX=u\cdot textureWidth
\]

Muestras:

```c
texture_pixels[
    texture_y * texture_width + texture_x
]
```

Para paredes repetibles puedes usar wrapping.

Conceptualmente:

\[
coord \mod textureSize
\]

pero ten cuidado con coordenadas negativas en C.

### Mantén las texturas en CPU

Esto es importantísimo.

Como nuestro renderer es software, **no queremos leer texels desde un `Texture2D` de GPU**.

Queremos:

```c
typedef struct SoftwareTexture {
    Color *pixels;
    int width;
    int height;
} SoftwareTexture;
```

Puedes cargar PNG mediante raylib como `Image`, convertirlo al formato que quieras y conservar sus datos en RAM. Raylib distingue explícitamente sus imágenes en CPU de las texturas cargadas en GPU, justo lo que necesitamos para esta arquitectura. citeturn15view4turn18view1

Tu pipeline de asset:

```text
wall.png
   ↓
LoadImage()
   ↓
RGBA
   ↓
SoftwareTexture
   ↓
renderer software
```

La única textura necesariamente subida a GPU será principalmente:

```text
framebuffer final
```

### Depth buffer por columnas

Para sprites necesitaremos saber qué pared visible está delante en cada columna:

```c
float wall_depth[INTERNAL_WIDTH];
```

Al comenzar:

```text
wall_depth[x] = infinito
```

Cuando dibujamos una pared:

```text
si depth < wall_depth[x]:
    dibujar
    wall_depth[x] = depth
```

Esto no reproduce exactamente el mecanismo de Doom —el original mantiene estructuras de clipping, drawsegs, planos y BSP—, pero es una excelente primera arquitectura pedagógica antes de introducir portales y clipping más sofisticado. El renderer original limpia buffers de segmentos/planos/sprites y recorre su BSP antes de procesar planos y elementos enmascarados. citeturn16view0turn16view2

### Suelos y techos: ray-plane intersection

Ahora viene algo bellísimo.

Por cada píxel puedes imaginar un rayo desde la cámara:

```text
                  ceiling
─────────────────────────────────
                 /
                /
camera ●───────/
              /
             /
─────────────────────────────────
                   floor
```

En coordenadas de cámara definimos un rayo no normalizado:

\[
ray=
\left(
\frac{x-centerX}{f_x},
-\frac{y-centerY}{f_y},
1
\right)
\]

Interpretándolo como:

```text
right
vertical
forward
```

Su componente vertical es:

\[
r_z=
-\frac{y-centerY}{f_y}
\]

Queremos intersectarlo con un plano horizontal:

\[
z=h
\]

Nuestra cámara está a:

\[
z=eyeZ
\]

La ecuación es:

\[
eyeZ+t r_z=h
\]

Despejamos:

\[
t=
\frac{h-eyeZ}{r_z}
\]

Y listo.

Con `t` conocemos dónde cayó el rayo.

Dirección horizontal mundial:

\[
D_{xy}
=
F + R\cdot rayX
\]

Entonces:

\[
worldX=p_x+tD_x
\]

\[
worldY=p_y+tD_y
\]

Y utilizamos esas coordenadas para muestrear la textura:

```text
FLOOR01.png
```

Por ejemplo:

\[
textureX = worldX \mod width
\]

\[
textureY = worldY \mod height
\]

¡Y acabas de crear **perspective-correct floor mapping** sin polígonos!

### Optimizar el suelo después

No optimices al principio.

Primero:

```text
por cada píxel
    generar rayo
    intersectar plano
    samplear texel
```

Cuando funcione puedes observar que cada scanline comparte información y reemplazarlo con un algoritmo incremental por filas.

Ahí llegarás naturalmente al concepto de **span rendering**.

El renderer del Doom original organiza sus planos visibles y llama a rutinas de span mapping al procesar floors/ceilings, justamente una versión mucho más optimizada de esta clase de problema. citeturn16view3

Éste es un patrón pedagógico que debes repetir durante todo el proyecto:

```text
versión correcta pero lenta
        ↓
comprenderla
        ↓
medir
        ↓
optimizar
```

No:

```text
optimización sofisticada
        ↓
bug misterioso
        ↓
dolor
```

### Sectores

Ahora reemplazamos:

```text
un suelo
un techo
```

por:

```c
typedef struct Sector {
    float floor_z;
    float ceiling_z;

    int floor_texture;
    int ceiling_texture;

    float light_level;
} Sector;
```

Cada pared sabe cuál sector queda a cada lado.

```text
Sector A             Sector B

ceiling A ────────────────
                 ┌──────── ceiling B
                 │
                 │ upper wall
                 │
                 └────────
                     opening
                 ┌────────
                 │ lower wall
floor A ─────────┘
         floor B ─────────
```

Para una línea entre dos sectores:

\[
openingFloor
=
\max(floor_A,floor_B)
\]

\[
openingCeiling
=
\min(ceiling_A,ceiling_B)
\]

El hueco visible existe cuando:

\[
openingCeiling>openingFloor
\]

Entonces una línea de dos lados puede producir hasta tres regiones:

```text
upper wall
portal/opening
lower wall
```

Ésta es la idea que permite:

- escalones;
- ventanas;
- corredores;
- plataformas;
- puertas verticales;
- habitaciones con distinta altura.

Las estructuras originales de Doom reflejan precisamente esta relación entre segmentos y `frontsector`/`backsector`; las líneas de un solo lado carecen de sector trasero. citeturn16view5turn16view6

### Portales antes de BSP

Antes de escribir el BSP puedes renderizar sectores mediante **portal rendering**.

Partes del sector donde está el jugador:

```text
RenderSector(current)
```

Cuando encuentras un portal visible:

```text
Sector A → Sector B
```

calculas qué región de pantalla permite ver:

```text
x_min
x_max
top_clip[x]
bottom_clip[x]
```

y visitas B solamente a través de esa abertura.

Conceptualmente:

```text
Room A
┌──────────────────┐
│                  │
│      player      │
│            ┌─────┼──────┐
│            │portal      │
└────────────┼─────┘      │
             │   Room B   │
             └────────────┘
```

Esto enseña algo fundamental:

> La visibilidad no consiste solamente en preguntar “¿está delante de la cámara?”, sino “¿qué regiones de la pantalla siguen siendo visibles?”.

Un `visited[sector]` global no siempre basta, porque un mismo sector podría verse a través de diferentes portales. La unidad real de visibilidad acaba siendo algo como:

```text
sector + intervalo horizontal + clipping vertical
```

### Después: BSP

Cuando el renderer por sectores ya funciona, construyes el verdadero reto.

**BSP = Binary Space Partitioning.**

Tenemos una recta de partición:

```text
                 front
                   │
        A────────B │
                   │
───────────────────┼──────── partition
                   │
                   │
                 back
```

Una línea:

\[
L(t)=P+tD
\]

Para clasificar otro punto `Q`:

\[
side=cross(D,Q-P)
\]

Si:

```text
side > epsilon  → front
side < -epsilon → back
|side| <= epsilon → sobre el plano
```

Para un segmento analizas sus dos extremos.

Casos:

```text
front/front → front
back/back   → back

front/back
back/front
     ↓
hay que dividir el segmento
```

Entonces el constructor:

```text
BuildBSP(lines):
    si no quedan líneas:
        crear leaf

    elegir partition

    para cada línea:
        clasificar

        si front:
            front_list

        si back:
            back_list

        si crossing:
            dividir
            fragmento_front → front_list
            fragmento_back  → back_list

    node.front = BuildBSP(front_list)
    node.back  = BuildBSP(back_list)
```

La elección del splitter importa.

Una heurística razonable:

\[
score=
splitCount\cdot A
+
|frontCount-backCount|\cdot B
\]

Quieres:

```text
pocos splits
+
árbol relativamente balanceado
```

No necesitas encontrar matemáticamente el splitter perfecto.

### Render del BSP

En ejecución:

```text
RenderNode(node):
    side = SideOf(camera, node.partition)

    RenderNode(node.child[side])

    si el otro bounding box puede verse:
        RenderNode(node.child[1 - side])
```

Es prácticamente el mismo patrón de alto nivel observable en el Doom original: `R_RenderBSPNode()` determina el lado del punto de vista, procesa primero el hijo frontal y sólo visita el otro si `R_CheckBBox()` indica que todavía puede resultar visible. citeturn16view2

Cuando llegas a una hoja:

```text
subsector
```

rasterizas sus `seg`s.

El código original describe explícitamente sus subsectors como listas de `LineSegs` que representan los lados de una hoja convexa del BSP. citeturn16view5

### Sprites billboarding

Enemigos, barriles, pickups y proyectiles pueden representarse mediante planos que siempre miran a la cámara.

Para un enemigo:

\[
E=(e_x,e_y,e_z)
\]

reutilizas exactamente nuestra transformación:

\[
depth=(E-P)\cdot F
\]

\[
cameraX=(E-P)\cdot R
\]

Centro de pantalla:

\[
spriteX=centerX+\frac{cameraX}{depth}f
\]

Escala:

\[
scale\propto\frac{1}{depth}
\]

Altura aparente:

\[
screenHeight=
\frac{worldHeight}{depth}f
\]

Entonces:

```text
lejos:
    [monster]

cerca:
    [ M O N S T E R ]
```

Para cada columna del sprite:

```text
si sprite_depth < wall_depth[x]
    dibujar
```

y los texels transparentes simplemente no escriben ningún píxel.

### Sprites direccionales

Puedes crear ocho vistas del mismo enemigo:

```text
       0
   7       1

 6    👹    2

   5       3
       4
```

Calculas el ángulo relativo entre:

```text
dirección del monstruo
dirección hacia cámara
```

y cuantizas:

\[
index
=
\left\lfloor
\frac{angle+\pi/N}
     {2\pi/N}
\right\rfloor
\bmod N
\]

con:

```text
N = 8
```

Eso da la estética clásica sin modelos 3D.

### Iluminación

Empieza con una luz por sector:

```text
0.0 = negro
1.0 = brillo completo
```

Después puedes combinar:

\[
brightness=
sectorLight\cdot distanceAttenuation
\]

Una fórmula sencilla:

\[
attenuation=
\frac{1}{1+kd}
\]

o incluso una tabla discreta:

```text
depth 0..2   → light level 15
depth 2..4   → light level 14
...
```

Una técnica particularmente retro sería utilizar:

```text
palette[256]
light_table[level][color_index]
```

en lugar de multiplicar RGB.

El renderer original de Doom contiene precisamente estructuras/tablas de colormap e iluminación dependientes de escala, aunque para nuestro motor no necesitamos reproducir su implementación exacta. citeturn16view0

Tu renderer eventualmente tendrá un pipeline como éste:

```text
                     ┌──────────────┐
                     │   MAP DATA   │
                     └──────┬───────┘
                            │
                            ▼
                     ┌──────────────┐
                     │ BSP / Portal │
                     │ visibility   │
                     └──────┬───────┘
                            │
             ┌──────────────┼─────────────┐
             │              │             │
             ▼              ▼             ▼
         WALLS          PLANES         SPRITES
             │              │             │
             └──────────────┼─────────────┘
                            ▼
                  SOFTWARE FRAMEBUFFER
                            │
                            ▼
                       UpdateTexture
                            │
                            ▼
                          raylib
```

---

## Mapas, colisiones y mundo interactivo

Una vez que puedes renderizar sectores, necesitas definir **cómo se almacena realmente un nivel**.

Aquí te recomiendo inspirarte conceptualmente en las estructuras de Doom sin copiar su formato.

### Datos fuente y datos compilados

Separa:

```text
MAPA PARA EDITAR
      ↓
map compiler
      ↓
MAPA PARA EJECUTAR
```

Esto es una técnica extremadamente útil.

El mapa editable puede tener:

```text
Vertex
Sector
Side
Line
Thing
```

y el compilado añade:

```text
Seg
Subsector
BSP Node
sector adjacency
bounding boxes
lookup tables
```

El Doom original también distingue estructuras de sectores, sidedefs/linedefs, segs, subsectors y nodos BSP durante su representación en memoria. citeturn16view4turn16view5turn16view7

### Tus estructuras principales

Conceptualmente:

```c
typedef struct MapVertex {
    float x;
    float y;
} MapVertex;
```

Sector:

```c
typedef struct MapSector {
    float floor_z;
    float ceiling_z;

    int floor_texture;
    int ceiling_texture;

    float light;

    int tag;
} MapSector;
```

Una cara de pared:

```c
typedef struct MapSide {
    int sector;

    int texture_upper;
    int texture_middle;
    int texture_lower;

    float texture_offset_x;
    float texture_offset_y;
} MapSide;
```

Línea:

```c
typedef struct MapLine {
    int vertex_a;
    int vertex_b;

    int front_side;
    int back_side;

    unsigned int flags;

    int special;
    int tag;
} MapLine;
```

Entidad colocada en el mapa:

```c
typedef struct MapThing {
    float x;
    float y;

    float angle;

    int type;
    unsigned int flags;
} MapThing;
```

Una línea exterior tendría:

```text
front_side = válido
back_side  = -1
```

Una línea entre habitaciones:

```text
front_side = válido
back_side  = válido
```

### Nunca guardes punteros en el archivo

Mal:

```text
sector_pointer = 0x7ffca7812340
```

Naturalmente esa dirección no significa nada después de reiniciar.

Guarda índices:

```text
sector_index = 7
```

En runtime puedes resolverlos como:

```c
Sector *sector = &map->sectors[sector_index];
```

Esta misma regla será crucial para saves.

### Un formato inicialmente humano

Durante el aprendizaje, algo como:

```text
vertex -128 -128
vertex  128 -128
vertex  128  128
vertex -128  128

sector 0 128 FLOOR01 CEIL01 0.85

side 0 WALL01 WALL01 WALL01 0 0

line 0 1 0 -1 0 0
line 1 2 0 -1 0 0
line 2 3 0 -1 0 0
line 3 0 0 -1 0 0

thing 0 0 0 PLAYER_START
```

es mucho mejor para aprender que comenzar inventando un formato binario sofisticado.

Después:

```text
.map
 ↓
mapc
 ↓
.bmap
```

El archivo compilado puede ser binario y rápido.

### El editor de mapas

Sí: crear mapas será otro desafío.

Y eso es bueno.

Yo haría tu propio pequeño editor 2D también con raylib.

Vista superior:

```text
┌─────────────────────────────────────┐
│ · · · · · · · · · · · · · · · · │
│ · · A──────────────B · · · · · · │
│ · · │              │ · · · · · · │
│ · · │   sector 0   │──────E · · · │
│ · · │              │      │ · · · │
│ · · D──────────────C      │ · · · │
│ · · · · · · · · · └──────F · · · │
└─────────────────────────────────────┘
```

Funciones progresivas:

```text
crear vértice
conectar línea
crear sector
seleccionar
mover
grid snapping
asignar textura
asignar altura
asignar luz
colocar Thing
asignar special/tag
guardar
compilar
probar nivel
```

**No hagas el editor antes del renderer.**

Primero crea varios mapas manualmente.

Sólo cuando editar texto comience a ser verdaderamente incómodo, construye la herramienta visual.

### Colisión del jugador

No representes al jugador como un punto.

Usa un cilindro:

```text
vista superior:

        radius
      ←───────→
       _______
     /         \
    | player    |
     \_________/
```

En XY es simplemente un círculo.

Necesitamos distancia círculo-segmento.

Segmento:

```text
A────────────B

        P
```

Primero:

\[
AB=B-A
\]

Proyectamos P:

\[
t=
\frac{(P-A)\cdot(B-A)}
     {(B-A)\cdot(B-A)}
\]

Limitamos:

\[
t=clamp(t,0,1)
\]

Punto más cercano:

\[
C=A+t(B-A)
\]

Distancia:

\[
d=|P-C|
\]

Hay penetración cuando:

\[
d<radius
\]

Normal:

\[
n=
\frac{P-C}{|P-C|}
\]

Profundidad:

\[
penetration=radius-d
\]

Corrección:

\[
P'=P+n\cdot penetration
\]

Y tienes colisión.

### Wall sliding

Supón:

```text
         pared
          │
        ↗ │ movimiento
      ↗   │
 player   │
```

No queremos:

```text
chocar → detenerse completamente
```

Queremos:

```text
chocar → continuar paralelo
```

Eliminamos la componente de velocidad contra la normal:

\[
v_{slide}
=
v-n(v\cdot n)
\]

Esto produce el movimiento suave típico de FPS.

### Colisión con portales

Una pared de dos lados puede ser atravesable.

Sector actual A, sector B:

\[
openFloor=
\max(floor_A,floor_B)
\]

\[
openCeiling=
\min(ceiling_A,ceiling_B)
\]

El jugador cabe si:

\[
openCeiling-openFloor
\ge
playerHeight
\]

Y puede subir si:

\[
floor_B-floor_A
\le
maxStepHeight
\]

Por ejemplo:

```text
playerHeight  = 56
maxStepHeight = 24
```

son parámetros de diseño, no leyes físicas.

Así puedes tener:

```text
escalón pequeño → caminar
pared alta      → bloquear
techo bajo      → bloquear
```

### Encuentra siempre el sector actual

Necesitarás saber:

```text
player.current_sector
```

porque de ahí salen:

- `floor_z`;
- `ceiling_z`;
- luz;
- sonidos ambientales;
- daño del suelo;
- sector secreto;
- underwater/efectos posteriores si inventas extensiones.

Con BSP puedes encontrar rápidamente la subsección que contiene una posición recorriendo el árbol hasta una hoja.

### Puertas

Una puerta clásica puede ser simplemente:

```text
sector.ceiling_z
```

cambiando a lo largo del tiempo.

FSM:

```text
CLOSED
  │ use
  ▼
OPENING
  │ reaches target
  ▼
OPEN
  │ timer
  ▼
CLOSING
  │ reaches floor
  ▼
CLOSED
```

No necesitas geometría especial.

Es simplemente:

\[
ceiling_z(t)
\]

interpolándose.

### Ascensores y plataformas

Misma idea:

```text
floor_z
```

cambia.

Estados:

```text
BOTTOM
MOVING_UP
TOP
WAITING
MOVING_DOWN
```

Ésta es una consecuencia preciosa del diseño por sectores:

> modificar el mundo se convierte frecuentemente en modificar unas pocas propiedades del sector.

### Linedef specials y tags

Haz que una línea tenga:

```text
special
tag
```

Por ejemplo:

```text
SPECIAL_DOOR_OPEN
tag = 7
```

Cuando el jugador pulsa `Use`:

```text
raycast corto
    ↓
encuentra línea
    ↓
line.special
    ↓
buscar sectores con tag 7
    ↓
activar puerta
```

Ahora puedes crear:

```text
interruptores
puertas
ascensores
teleports
exits
trampas
secretos
arenas de jefe
```

sin programar el comportamiento específicamente para cada mapa.

### Sistemas dirigidos por datos

No hagas:

```c
if (current_level == 7 &&
    player.x > 123.0f &&
    player.y < 321.0f) {
    OpenBossDoor();
}
```

Haz:

```text
trigger:
    type = BOSS_ARENA
    tag  = 15

sector:
    tag = 15
```

Tu mapa describe el comportamiento.

Eso es el comienzo de una arquitectura **data-driven**.

### Hitscan

Una pistola clásica puede:

```text
origen = posición cámara
dirección = forward
```

Encontrar la intersección más cercana entre:

```text
rayo
paredes
enemigos
```

Debes seleccionar el impacto con menor:

\[
t>0
\]

No:

```text
el primero que recorres en el array
```

porque el orden de datos del mapa no representa profundidad.

Para una escopeta:

```text
for pellet:
    direction = forward + random_spread
    trace()
```

### Proyectiles

Un cohete sí es una entidad:

```text
position
velocity
radius
damage
splash_radius
owner
```

Cada tick:

\[
P_{new}=P+V\Delta t
\]

Si la velocidad puede ser grande respecto al tamaño de las paredes, no dependas únicamente de:

```text
mover
→
preguntar si está adentro de algo
```

porque podrías atravesar obstáculos entre dos ticks.

La versión robusta trata el movimiento como un sweep:

```text
old_position ─────────────→ new_position
```

y busca una colisión a lo largo de ese desplazamiento.

### Daño de explosión

Para una explosión:

\[
d=|target-explosion|
\]

Puedes usar:

\[
damage=
maxDamage
\left(
1-\frac{d}{radius}
\right)
\]

cuando:

\[
d<radius
\]

y opcionalmente realizar line-of-sight para impedir daño a través de paredes.

### Sistema de “thinkers”

Una excelente idea conceptual tomada del diseño clásico de Doom es mantener entidades activas que reciben actualización periódica. El código original posee una lista de `thinker_t`; `P_RunThinkers()` la recorre, elimina elementos marcados y ejecuta la función de actualización de cada thinker. citeturn16view10

No necesitas copiar esa implementación.

Puedes tener:

```text
GameUpdate
├── UpdatePlayer()
├── UpdateEnemies()
├── UpdateProjectiles()
├── UpdatePickups()
├── UpdateDoors()
├── UpdatePlatforms()
└── UpdateTriggers()
```

Y posteriormente convertirlo en:

```text
for each entity:
    EntityThink(entity)
```

La idea importante es que el renderer **no controla el gameplay**.

```text
SIMULATION STATE
       │
       ├─────────→ renderer
       │
       ├─────────→ audio
       │
       └─────────→ UI
```

El estado del mundo es la fuente de verdad.

---

## Armas, enemigos, jefes y gameplay completo

Llegados aquí ya tienes esencialmente un **motor**.

Ahora puedes construir el **juego**.

### Entidades

No empieces con un sistema ECS enorme.

Una entidad clásica y explícita es más que suficiente:

```c
typedef struct Entity {
    bool active;

    int type;

    Vec2 position;

    float z;
    float angle;

    Vec2 velocity;
    float velocity_z;

    float radius;
    float height;

    int health;

    int current_state;
    float state_time;

    int target_entity;

    unsigned int flags;
} Entity;
```

Puedes tener:

```text
MAX_ENTITIES = 1024
```

y un array:

```c
Entity entities[MAX_ENTITIES];
```

Más adelante puedes construir pools más sofisticados si realmente los necesitas.

### Definiciones separadas de instancias

No guardes todos los valores estáticos repetidos.

Haz:

```text
EntityDef
├── max_health
├── speed
├── radius
├── height
├── pain_chance
├── sprite_set
├── attack_range
└── estados
```

y:

```text
Entity
├── type → EntityDef
├── current health
├── current position
├── current state
└── target
```

Entonces 50 enemigos del mismo tipo no repiten toda su configuración.

### FSM para enemigos

Una máquina básica:

```text
                 ┌───────────┐
                 │   IDLE    │
                 └─────┬─────┘
                       │
                ve/oye jugador
                       │
                       ▼
                 ┌───────────┐
            ┌───→│   CHASE   │←─────┐
            │    └─────┬─────┘      │
            │          │            │
            │     rango ataque      │
            │          │            │
            │    ┌─────┴─────┐      │
            │    ▼           ▼      │
            │ MELEE        RANGED   │
            │    │           │      │
            └────┴───────────┘      │
                                    │
              PAIN ─────────────────┘

               ↓ health <= 0

                  DEATH
```

Estados razonables:

```text
SPAWN
IDLE
ALERT
CHASE
MELEE_ATTACK
RANGED_ATTACK
PAIN
DEAD
```

Este enfoque coincide conceptualmente con el diseño clásico de Doom: por ejemplo, su rutina `A_Look` mantiene al actor buscando un jugador o reaccionando a un `soundtarget`, y después cambia al estado de persecución; `A_Chase` implementa posteriormente la lógica del actor mientras persigue. citeturn16view8turn16view9

### Estados como datos

Una arquitectura muy elegante:

```c
typedef struct StateDef {
    int sprite;

    int frame;

    float duration;

    int next_state;
} StateDef;
```

Y una tabla:

```text
STATE_IMP_IDLE_0
sprite = IMP_IDLE
frame = 0
duration = 0.15
next = STATE_IMP_IDLE_1

STATE_IMP_IDLE_1
sprite = IMP_IDLE
frame = 1
duration = 0.15
next = STATE_IMP_IDLE_0
```

La entidad solamente mantiene:

```text
state_id
state_timer
```

Cada tick:

```text
timer -= dt

si timer <= 0:
    state = current.next_state
    timer = new.duration
```

Ahora animaciones y comportamiento pueden sincronizarse.

Ejemplo:

```text
ATTACK_A
ATTACK_B
ATTACK_FIRE   ← aquí aparece proyectil
ATTACK_C
IDLE
```

El frame puede disparar una acción:

```text
OnEnterAttackFire()
```

### Percepción: visión

Enemigo:

```text
Enemy E
Player P
```

Dirección:

\[
D=P-E
\]

Distancia:

\[
d=|D|
\]

Primero puedes hacer:

```text
if d > vision_range:
    no
```

Después FOV:

\[
dot(enemyForward,\hat D)
\]

Si deseas un cono de visión de ángulo total \(\alpha\):

\[
dot
\ge
\cos(\alpha/2)
\]

Después:

```text
TraceLine(E, P)
```

Si una pared sólida aparece antes que el jugador:

```text
no line of sight
```

Esto reutiliza nuestra intersección de rayos.

### Percepción: sonido

Aquí puedes hacer algo muchísimo mejor que:

```c
if (distance < hearing_radius) {
    enemy->alert = true;
}
```

Tu mapa ya es un grafo:

```text
Sector A ─ portal ─ Sector B ─ portal ─ Sector C
```

Cuando se dispara un arma:

```text
sector_source
     ↓
BFS por sectores conectados
     ↓
marcar sound event
```

Puedes bloquear propagación mediante una flag:

```text
LINE_BLOCK_SOUND
```

El diseño original de Doom almacena de hecho un `soundtarget` en sus sectores y `A_Look` consulta ese objetivo para decidir si un enemigo reacciona al ruido. citeturn16view4turn16view8

Para nuestro motor, una **BFS sobre el grafo de sectores** es una forma clara y general de implementar esa idea.

### Pathfinding

No necesitas una grilla gigante.

Ya tienes:

```text
sector graph
```

Nodo:

```text
Sector
```

Arista:

```text
Portal atravesable
```

Puedes comenzar con BFS.

Después A*.

Para A*:

\[
f(n)=g(n)+h(n)
\]

donde:

- `g(n)` = coste recorrido;
- `h(n)` = estimación hasta destino.

Una heurística natural:

\[
h=
distance(centerSector,target)
\]

Resultado:

```text
Sector 2
   ↓
Sector 5
   ↓
Sector 8
   ↓
Sector 11
```

El enemigo camina hacia:

```text
centro del próximo portal
```

y utiliza collision sliding local.

Es una separación preciosa:

```text
A*
→ decide "por dónde"

steering/collision
→ decide "cómo moverse"
```

### No uses A* para todo

Un monstruo que ve directamente al jugador puede simplemente:

```text
desired_direction =
    normalize(player.position - enemy.position)
```

Sólo activa pathfinding si:

```text
bloqueado
o
no hay línea directa
```

Esto mantiene la IA barata y comprensible.

### Armas dirigidas por datos

Haz una definición:

```text
WeaponDef
├── ammo_type
├── ammo_per_shot
├── fire_interval
├── damage
├── pellet_count
├── spread
├── projectile_type
├── attack_type
├── firing_animation
├── muzzle_flash
├── sound
└── recoil
```

Entonces:

```text
Pistol:
    HITSCAN
    pellets = 1

Shotgun:
    HITSCAN
    pellets = 8
    spread > 0

Rocket:
    PROJECTILE

Plasma:
    PROJECTILE
    fire_interval pequeño
```

Tu código no necesita:

```c
if (weapon == SHOTGUN) {
   ...
} else if (weapon == ROCKET) {
   ...
}
```

por todas partes.

### Arma en primera persona

El arma de pantalla no necesita existir físicamente en el mundo.

Simplemente dibuja:

```text
world framebuffer
       ↓
weapon sprite
       ↓
HUD
```

Orden:

```text
3D/2.5D world
sprites del mundo
arma del jugador
HUD
menús
```

Puedes animar:

```text
IDLE
LOWERING
RAISING
FIRING
REFIRE
```

### Cadencia

Nunca hagas:

```c
if (IsActionDown(FIRE)) {
    Shoot();
}
```

porque dispararías una vez por frame.

Usa:

```text
weapon.cooldown
```

Cada tick:

\[
cooldown=\max(0,cooldown-dt)
\]

Sólo puedes disparar cuando:

```text
cooldown == 0
```

Tras disparar:

```text
cooldown = weapon.fire_interval
```

### Recoil y spread

Spread horizontal:

\[
angle=
playerAngle+random(-spread,+spread)
\]

Dirección:

\[
D=(\cos angle,\sin angle)
\]

Para reproducibilidad conviene eventualmente tener tu propio PRNG del gameplay:

```text
GameRandom()
```

con una semilla almacenada en saves.

Así:

```text
misma seed
+
mismos inputs
=
misma secuencia aleatoria
```

lo cual resulta muy útil para debug y, posteriormente, replays.

### Daño

Mantén una función central:

```text
DamageEntity(
    target,
    attacker,
    source,
    damage,
    damage_type
);
```

No escribas:

```text
enemy.health -= 10;
```

en veinte lugares.

Centralizar permite implementar después:

```text
armor
resistencias
pain state
death state
gibs
knockback
estadísticas
friendly fire
boss immunity
```

sin revisar el código de todas las armas.

### Pickups

Un pickup puede ser una entidad con:

```text
ENTITY_FLAG_PICKUP
```

Al intersectar jugador:

```text
switch pickup type
```

o mejor:

```text
PickupDef
```

con:

```text
health_amount
ammo_type
ammo_amount
key_type
sound
message
```

### Llaves

Define:

```text
KEY_RED
KEY_BLUE
KEY_YELLOW
```

Una puerta puede tener:

```text
required_key
```

Al intentar activarla:

```text
si player.keys contiene required_key:
    OpenDoor()
else:
    ShowMessage()
    PlayLockedSound()
```

### Jefes

No trates un jefe como algo totalmente diferente a un enemigo.

Empieza:

```text
Boss = Entity + FSM más grande
```

Añade fases:

```text
PHASE_ONE
health > 66%

PHASE_TWO
33% < health <= 66%

PHASE_THREE
health <= 33%
```

Cada fase controla:

```text
attack table
movement speed
cooldowns
spawn patterns
arena events
```

Por ejemplo:

```text
              SPAWN
                │
                ▼
             PHASE 1
                │
          health <= 66%
                ▼
          TRANSITION A
                │
        abrir compuertas
                │
                ▼
             PHASE 2
                │
          health <= 33%
                ▼
          TRANSITION B
                │
                ▼
             PHASE 3
                │
             health 0
                ▼
              DEATH
                │
                ▼
          EXIT TRIGGER
```

Lo hermoso es que ya posees todos los bloques necesarios:

```text
FSM
timers
projectiles
triggers
doors
sector movement
enemy spawning
sound
animation
```

Un boss no requiere un “sistema de bosses”.

Requiere **composición de sistemas existentes**.

### Diseña ataques legibles

Un buen ataque complejo suele tener:

```text
TELEGRAPH
    ↓
WINDUP
    ↓
ATTACK
    ↓
RECOVERY
```

No hagas:

```text
estado CHASE
↓
instantáneamente quitar 80 HP
```

Puedes crear cosas increíbles incluso en 2.5D:

```text
salvas radiales
proyectiles en abanico
misiles dirigidos
summons
pisotones
columnas de fuego
trampas del escenario
puertas que cambian la arena
suelo dañino
teleports
```

### Sistema de eventos

Cuando el juego empiece a crecer, en lugar de acoplar:

```text
EnemyDeath()
    → OpenDoor()
    → PlayMusic()
    → ActivateLift()
```

puedes emitir:

```text
EVENT_ENTITY_DIED
```

y los sistemas reaccionan.

Por ejemplo:

```text
boss dies
    ↓
EVENT_BOSS_DIED(tag=666)
    │
    ├── trigger → abre salida
    ├── audio → cambia música
    └── HUD → muestra mensaje
```

No necesitas implementar un event bus complejo desde el día uno, pero es una evolución natural.

---

## Audio, música, interfaz, menús y persistencia

Raylib puede encargarse muy bien de esta parte sin quitarte el aprendizaje fundamental del motor.

### Audio

La inicialización oficial básica es:

```c
// Inicializa el dispositivo de salida de audio de raylib.
InitAudioDevice();
```

Y al cerrar:

```c
// Libera el dispositivo únicamente después de descargar sonidos y música.
CloseAudioDevice();
```

Los ejemplos oficiales de raylib 6.0 muestran `LoadSound()` para efectos WAV/OGG y `LoadMusicStream()` para música, mientras que el ejemplo de streaming llama a `UpdateMusicStream()` continuamente durante el bucle principal. citeturn18view0turn17view0

Arquitectura:

```text
AudioSystem
├── SFX
│   ├── pistol
│   ├── shotgun
│   ├── door
│   ├── enemy_pain
│   └── pickup
│
└── MUSIC
    ├── level_01
    ├── boss
    └── menu
```

Para tus efectos:

```text
producción:
WAV master
    ↓
runtime:
WAV u OGG
```

Raylib demuestra oficialmente la carga de ambos como `Sound`. citeturn18view0

Para música:

```text
master de alta calidad
    ↓
formato comprimido apropiado
    ↓
LoadMusicStream()
```

El ejemplo oficial utiliza un MP3 y mantiene el buffer mediante `UpdateMusicStream()`. citeturn17view0

### Audio espacial

No necesitas audio 3D complejo.

Tenemos:

```text
listener = player
source   = monster
```

Distancia:

\[
d=|source-listener|
\]

Volumen sencillo:

\[
volume=
clamp
\left(
1-\frac d{maxDistance},
0,
1
\right)
\]

Para paneo calculas:

\[
dir=
normalize(source-listener)
\]

y después:

\[
panSignal=dir\cdot playerRight
\]

Así:

```text
-1 aproximadamente izquierda
 0 centro
+1 aproximadamente derecha
```

Luego lo conviertes al rango esperado por tu capa de audio.

Ahora oirás:

```text
imp a la izquierda
puerta detrás
cohete pasando a la derecha
```

sin necesitar un sistema acústico completo.

### Oclusión de sonido

Puedes extenderlo:

```text
TraceLine(listener, source)
```

Si existe una pared:

```text
volume *= 0.5
```

O hacer algo todavía más Doom-like usando sectores:

```text
source sector
     ↓
portal graph
     ↓
listener sector
```

y atenuar según las puertas/portales atravesados.

### Música adaptativa

Tu propio trabajo musical puede ser mucho más interesante que simplemente:

```text
PlayMusic(level_song)
```

Por ejemplo:

```text
EXPLORATION
    ↓ enemigos activos
COMBAT
    ↓ boss enters
BOSS
    ↓ boss dies
AFTERMATH
```

Puedes preparar stems o tracks diferentes.

Pero implementaría esto **muy tarde**. Primero consigue simplemente una canción por nivel.

### HUD

El HUD puede dibujarse después del framebuffer 3D:

```text
┌────────────────────────────────┐
│                                │
│            mundo               │
│                                │
│              +                 │
│                                │
├────────────────────────────────┤
│ HP 84   ARM 32   AMMO 18       │
└────────────────────────────────┘
```

Orden:

```text
RenderWorld()
UploadFramebuffer()
DrawWorldTexture()
DrawWeapon()
DrawHUD()
DrawMessages()
DrawMenuOverlay()
```

Mantén completamente separado:

```text
world-space UI
screen-space UI
```

Un enemigo está en coordenadas del mundo.

`HEALTH: 100` está en coordenadas de pantalla.

### Menús

Tu máquina global:

```text
TITLE
MAIN_MENU
OPTIONS
PLAYING
PAUSED
GAME_OVER
CREDITS
```

Menú:

```text
NEW GAME
LOAD GAME
OPTIONS
CREDITS
QUIT
```

No codifiques el menú como veinte `if`.

Usa items:

```c
typedef struct MenuItem {
    const char *label;

    bool enabled;

    void (*activate)(void *context);
} MenuItem;
```

O IDs si no quieres callbacks.

El menú mantiene:

```text
selected_index
```

Input:

```text
ACTION_MENU_UP
ACTION_MENU_DOWN
ACTION_MENU_CONFIRM
ACTION_MENU_BACK
```

Como ya abstrajimos input al principio, teclado y gamepad funcionan sin reescribir el menú. El ejemplo oficial de raylib sobre “input actions” utiliza precisamente esta separación entre una acción lógica y su tecla/botón físico. citeturn18view2

### Opciones

Separa tres tipos:

```text
VIDEO
├── fullscreen
├── window size
├── internal resolution
└── pixel filtering

AUDIO
├── master
├── music
└── effects

INPUT
├── sensitivity
├── invert
└── bindings
```

Y guarda una configuración independiente del savegame.

Por ejemplo:

```text
config.cfg
```

### Pantalla de pausa

No necesitas otro mundo.

Simplemente:

```text
GAME_MODE_PAUSED
```

hace:

```text
no GameSimulationUpdate()
sí UIUpdate()
sí RenderCurrentWorld()
sí RenderPauseMenu()
```

Curiosamente, el Doom original también diferencia el tick de juego y retorna sin avanzar la simulación bajo determinadas condiciones de pausa/menu, visible en su `P_Ticker()`. citeturn16view10

### Saves

Nunca serialices:

```c
fwrite(&entire_game_struct, sizeof(Game), 1, file);
```

Parece fácil.

Luego cambias:

```c
struct Entity
```

y todos los saves mueren.

Diseña:

```text
MAGIC
VERSION
LEVEL_ID
RNG_SEED

PLAYER
    position
    angle
    health
    armor
    ammo
    weapons
    keys

SECTORS
    dynamic floor positions
    dynamic ceiling positions
    states

ENTITIES
    type
    position
    state
    health
    target ID
    timers

GLOBALS
    triggers
    secrets
    level timer
```

Header conceptual:

```text
HFGS
version = 3
```

Cuando cargas:

```text
if magic incorrecto:
    error

if version incompatible:
    migration o rechazo
```

Y nuevamente:

> serializa IDs, no punteros.

Mal:

```text
enemy.target = 0x12345678
```

Bien:

```text
enemy.target_id = 42
```

Tras cargar:

```text
entity #42
```

### Assets

Yo usaría una convención predecible:

```text
assets/
├── textures/
│   ├── walls/
│   ├── floors/
│   └── ceilings/
├── sprites/
│   ├── monsters/
│   ├── weapons/
│   ├── pickups/
│   └── effects/
├── sounds/
├── music/
└── maps/
```

Para renderer software:

```text
PNG
 ↓
Image CPU
 ↓
RGBA SoftwareTexture
```

La distinción `Image`/RAM frente a `Texture`/VRAM está explícitamente documentada en los ejemplos oficiales de raylib, y encaja perfectamente con este pipeline. citeturn15view4turn18view1

Yo empezaría artísticamente con:

```text
walls       64×64
flats       64×64
sprites     dimensiones variables
weapon HUD  dimensiones variables
```

No porque el motor deba exigir esas dimensiones, sino porque simplifica muchísimo tus primeros experimentos de wrapping y pixel art.

Después haz el renderer completamente agnóstico:

```text
texture.width
texture.height
```

### Atlas o texturas individuales

Al principio:

```text
una imagen por textura
```

es perfecto.

Más tarde puedes compilar:

```text
textures.wad-like-pack
sprites.pack
sounds.pack
```

No inventes tu propio WAD el primer día.

Primero haz el juego.

Luego construye el sistema de assets.

---

## La ruta completa desde una ventana vacía hasta tu propio “Doom”

Éste es el orden que seguiría personalmente. Es importante respetarlo porque cada etapa introduce **un solo grupo importante de conceptos nuevos**.

| Fase | Debes construir | Conceptos que estás aprendiendo | Criterio para avanzar |
|---|---|---|---|
| Bootstrap | ventana y build | CMake, raylib | compila desde cero |
| Framebuffer | array de píxeles | memoria, RGBA | puedes dibujar píxeles |
| Presentación | framebuffer escalado | CPU/GPU, texturas | pixel-perfect estable |
| Matemática | `Vec2` + geometry | dot, cross, normalize | tests correctos |
| Top-down | mapa 2D | vértices/segmentos | ves el mapa desde arriba |
| Jugador | caminar/girar | vectores, dt | movimiento estable |
| Cámara | world → camera | cambio de base | coordenadas verificables |
| Proyección | paredes planas | perspectiva | habitación visible |
| Clipping | near plane | rectas/lerp | puedes atravesar paredes sin explosiones |
| Texturas | walls | UV/perspectiva | paredes sin distorsión |
| Planos | floor/ceiling | ray-plane intersection | suelo texturizado |
| Sectores | alturas | representación 2.5D | escalones/ventanas |
| Portales | visibilidad | recursion/clipping | varias habitaciones |
| Collision | círculo/segmento | proyección vectorial | sliding estable |
| Sprites | billboards | depth/proyección | enemigo visible |
| Hitscan | pistola | ray intersection | puedes disparar |
| Projectiles | rockets | integración/collision | proyectiles sólidos |
| Entities | sistema general | lifecycle | items/enemigos coexistentes |
| FSM | IA | estados/timers | enemigo busca y ataca |
| Percepción | sight/sound | raycasts/grafos | IA reacciona naturalmente |
| Specials | puertas/lifts | tags/eventos | nivel interactivo |
| BSP | compilador | partición espacial | mapas mayores |
| Armas | arsenal | data-driven design | varias armas |
| Boss | fases | composición de sistemas | pelea completa |
| Audio | SFX/music | mixing/streaming | nivel sonoro completo |
| HUD | estado de jugador | UI | gameplay legible |
| Menús | frontend | global FSM | juego navegable |
| Saves | persistencia | serialización | cargar reproduce estado |
| Editor | tooling | UX interna | puedes hacer niveles cómodamente |
| Pulido | optimización | profiling | episodio jugable |

### Primer mini-proyecto: framebuffer

Antes de paredes:

```text
dibujar:
pixel
línea
rectángulo
círculo
```

Implementa tú mismo al menos:

```text
PutPixel()
DrawVerticalLine()
DrawHorizontalLine()
ClearBuffer()
```

No necesitas reimplementar toda raylib.

El objetivo es aprender a pensar:

```text
2D coordinates
→
array offset
```

La fórmula:

\[
index=y\cdot width+x
\]

debe volverse completamente natural.

### Segundo mini-proyecto: automap

Antes de perspectiva:

```text
render top-down
```

Dibuja:

```text
paredes
jugador
forward vector
collision radius
sector IDs
```

Ese visualizador permanecerá durante **todo el desarrollo**.

Nunca lo borres.

Te permitirá pulsar F1 y ver:

```text
┌───────────────────────────┐
│          BSP node         │
│              │            │
│  monster →   P────→ view  │
│              │            │
│──── wall ────┼────────────│
│              │            │
└───────────────────────────┘
```

Es posiblemente tu mejor debugger.

### Tercer mini-proyecto: cámara matemática

No dibujes texturas todavía.

Visualiza valores:

```text
world vertex:
(100, 50)

camera:
right = 22
depth = 103

screen:
x = 194
```

Haz que tenga sentido mentalmente.

Cuando te sitúas delante:

```text
depth positivo
```

Cuando está detrás:

```text
depth negativo
```

Cuando queda a tu derecha:

```text
cameraX positivo
```

Comprender esto evita días enteros de invertir signos al azar.

### Cuarto mini-proyecto: una habitación

Objetivo:

```text
████████████████████████████
█                          █
█                          █
█           +              █
█                          █
████████████████████████████
```

Debes:

```text
caminar
girar
chocar
```

Nada más.

### Quinto: texturas

Ahora:

```text
BRICK
BRICK
BRICK
```

Verifica especialmente:

```text
pared frontal
pared oblicua
pared parcialmente detrás
pared casi perpendicular
```

Si una pared oblicua estira su textura de forma extraña, revisa:

```text
perspective-correct interpolation
```

### Sexto: floors y ceilings

Al terminar esto sentirás por primera vez:

> “holy shit, hice un engine”.

Porque ya tendrás:

```text
paredes
suelo
techo
movimiento
perspectiva
```

y todo construido por ti.

### Séptimo: múltiples sectores

Crea:

```text
Room A
floor = 0

Room B
floor = 24
```

Después:

```text
Room C
ceiling = 80
```

Después una ventana.

Después una puerta.

### Octavo: sprites

Primero:

```text
árbol
```

estático.

Después:

```text
barril
```

Después:

```text
enemigo quieto
```

Después animación.

Sólo después IA.

### Noveno: una sola arma

No programes diez.

Haz una pistola perfecta:

```text
fire
muzzle flash
hitscan
impact effect
enemy damage
sound
ammo
animation
```

Cuando funcione, las demás armas reutilizan infraestructura.

### Décimo: un solo enemigo

Lo mismo.

Un enemigo:

```text
Idle
SeePlayer
Chase
Attack
Pain
Death
```

Cuando sea sólido puedes definir diez tipos mediante datos.

### Undécimo: un nivel entero

Haz un nivel pequeño:

```text
start
  ↓
corridor
  ↓
first fight
  ↓
blue key
  ↓
blue door
  ↓
lift
  ↓
arena
  ↓
exit
```

No construyas un editor gigantesco antes de conseguir esto.

### Duodécimo: BSP

Una vez que poseas un renderer de sectores funcional, implementa:

```text
map compiler
    ↓
split segments
    ↓
nodes
    ↓
subsectors
```

Ahora comprenderás **por qué existe el BSP**.

Si lo haces antes, simplemente memorizarás un algoritmo misterioso.

El código original de Doom constituye aquí un excelente material de estudio: su `R_Subsector()` determina planos, agrega sprites y procesa segmentos, mientras que `R_RenderBSPNode()` recorre recursivamente el árbol desde el nodo raíz. citeturn16view1turn16view2

### Debug views obligatorias

Tu motor debería terminar teniendo teclas internas como:

```text
F1  automap/debug map
F2  sector IDs
F3  collision shapes
F4  BSP partitions
F5  portal visibility
F6  sprite bounds
F7  depth visualization
F8  AI states
F9  line-of-sight
F10 performance counters
```

Especialmente útil:

```text
depth → grayscale
```

cerca:

```text
blanco
```

lejos:

```text
negro
```

Si ves discontinuidades extrañas sabes inmediatamente que la profundidad está rota.

### Tests geométricos

Tus funciones matemáticas merecen pequeños tests independientes.

Por ejemplo:

```text
Dot((1,0),(0,1)) == 0

Length((3,4)) == 5

ClosestPoint(segment, point)
RaySegmentIntersection(...)
PointSideOfLine(...)
```

El renderer puede ser difícil de testear automáticamente.

La geometría **no**.

### Invariantes

Usa invariantes como:

```text
sector.floor_z < sector.ceiling_z

line.vertex_a != line.vertex_b

front_side < side_count

texture_id < texture_count

entity.radius > 0

near_plane > 0
```

Un map loader debe rechazar datos corruptos antes de que éstos lleguen al renderer.

### No optimices con fixed-point todavía

El Doom histórico trabaja extensamente con tipos fixed-point, visible incluso en sus estructuras de sectores, vértices y nodos (`fixed_t`). citeturn16view4turn16view7

Tú deberías comenzar con:

```c
float
```

porque ahora el objetivo es:

```text
comprensión
→
correctitud
→
perfilado
→
optimización
```

No:

```text
nostalgia tecnológica
→
complejidad accidental
```

Cuando el juego funcione puedes implementar fixed-point **como proyecto educativo independiente** y comparar rendimiento, precisión y reproducibilidad.

### No optimices trigonometría todavía

Usa:

```c
sinf()
cosf()
```

Cuando sea necesario.

Después puedes experimentar con:

```text
lookup tables
```

como hacían muchos motores clásicos.

Pero nuevamente:

> primero entiende la transformación.

### No empieces por multithreading

Un framebuffer diminuto y un renderer eficiente por columnas/spans pueden rendir perfectamente sin que tu primera preocupación sea paralelizar.

Primero mide:

```text
wall rendering
plane rendering
sprite rendering
AI
texture upload
```

Entonces sabrás dónde está realmente el costo.

### Arquitectura final

Al acabar, tu programa podría verse así:

```text
main()
 │
 ├── PlatformInit()
 │   ├── InitWindow()
 │   ├── InitAudioDevice()
 │   └── InputInit()
 │
 ├── GameInit()
 │   ├── AssetManagerInit()
 │   ├── RendererInit()
 │   ├── AudioSystemInit()
 │   ├── MenuInit()
 │   └── LoadConfig()
 │
 └── while running
     │
     ├── ReadInput()
     │
     ├── FixedSimulation()
     │   │
     │   ├── PlayerUpdate()
     │   ├── WeaponUpdate()
     │   ├── EntityUpdate()
     │   │   ├── EnemyAI()
     │   │   ├── ProjectileUpdate()
     │   │   └── PickupUpdate()
     │   │
     │   ├── SectorSpecialsUpdate()
     │   ├── TriggerUpdate()
     │   └── CollisionUpdate()
     │
     ├── AudioUpdate()
     │
     ├── Render()
     │   │
     │   ├── FindPlayerSubsector()
     │   ├── ClearVisibility()
     │   ├── TraverseBSP()
     │   │   ├── RenderWalls()
     │   │   └── CollectVisibleSprites()
     │   │
     │   ├── RenderPlanes()
     │   ├── RenderSprites()
     │   ├── UploadFramebuffer()
     │   ├── DrawWeapon()
     │   ├── DrawHUD()
     │   └── DrawMenu()
     │
     └── Present()
```

Eso ya no sería simplemente:

> “un juego hecho con raylib”.

Sería:

> **tu propio pequeño engine 2.5D escrito en C, donde raylib funciona principalmente como plataforma.**

Y ésa es exactamente la clase de proyecto de la que aprendes una cantidad absurda.

### El mapa conceptual completo de conocimientos

Al terminarlo habrás practicado:

| Área | Lo que habrás implementado |
|---|---|
| C | structs, arrays, pointers, módulos, memoria |
| Matemática | vectores, dot, cross, trigonometría |
| Álgebra lineal | cambio de base/cámara |
| Geometría | segmentos, rayos, planos |
| Graphics | perspectiva, rasterización, UV |
| Graphics | clipping y depth |
| Rendering | paredes, planes, billboards |
| Estructuras | BSP |
| Grafos | sectores |
| Algoritmos | BFS |
| Algoritmos | A* |
| Física | circle/segment collision |
| Física | sliding/sweeps |
| Gameplay | FSM |
| Gameplay | armas y daño |
| IA | visión, sonido, navegación |
| Tooling | compilador/editor de mapas |
| Audio | SFX, streaming, espacialización |
| UI | HUD, menús |
| Persistencia | configuración y saves |
| Arquitectura | data-driven design |
| Debugging | visualización de subsistemas |

Y lo más bonito es que estas cosas no estarán escondidas detrás de un editor.

Vas a saber exactamente por qué:

```text
un vértice
→
se transforma
→
se proyecta
→
se convierte en una columna
→
se texturiza
→
se ilumina
→
termina siendo un píxel
```

El renderer original de Doom es especialmente valioso como referencia conceptual para este estudio porque permite observar esa separación entre sectores, subsectors, BSP, planos y sprites directamente en el código fuente publicado por id Software. citeturn16view1turn16view2turn16view3turn16view4

Para raylib, las referencias que realmente necesitas mantener abiertas mientras trabajas son la **cheatsheet oficial de raylib 6.0**, los **ejemplos oficiales** y, cuando quieras verificar una firma exacta, `raylib.h`. El propio proyecto recomienda justamente la cheatsheet y los ejemplos como sus recursos principales de aprendizaje. citeturn14search1turn15view0

La filosofía que mantendría durante todo el desarrollo sería ésta:

```text
no copiar un motor
        ↓
entender un problema

no pedir "hazme el sistema"
        ↓
derivar la matemática

no implementar diez features
        ↓
hacer una vertical slice correcta

no esconder los bugs
        ↓
visualizarlos

no optimizar por intuición
        ↓
medir

no construir Doom entero de golpe
        ↓
construir:
píxel
→ línea
→ pared
→ habitación
→ sector
→ portal
→ nivel
→ enemigo
→ arma
→ juego
```

Ese orden es probablemente la parte más importante de todo este manual. Si construyes cada peldaño tú mismo antes de subir al siguiente, al final no solamente tendrás un clon espiritual de Doom: **vas a entender por qué funciona un engine de este tipo desde los píxeles hasta la IA del jefe final.**