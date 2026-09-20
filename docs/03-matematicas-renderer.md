# 03 · Del mapa al píxel

## Convenciones

- XY es el plano del suelo; Z crece hacia arriba. Unidades: metros.
- Yaw cero mira hacia +Y. Yaw positivo gira hacia +X.
- Pitch positivo mira arriba; los ángulos se expresan en radianes.
- Pantalla: X hacia la derecha, Y hacia abajo; centros de píxel en `(x+0.5,y+0.5)`.
- UV se mide en repeticiones de textura, no en texels.

El mapa sigue siendo 2.5D: una altura de suelo y otra de techo por sector. El
renderer hace proyección 3D real de esa geometría. Permite pitch sin deformar
la imagen desplazando artificialmente el horizonte.

## Vectores y cambio de base

Restar puntos da un desplazamiento. El producto escalar mide cuánto avanza un
vector en una dirección; el producto cruzado 2D indica el lado de una arista.

Para yaw `a`, la base horizontal es:

```text
forward = (sin(a), cos(a))
right   = (cos(a), -sin(a))
```

Al punto del mundo le restamos la posición de cámara. Sus componentes se
proyectan sobre esa base con productos escalares. Después rotamos forward/Z
según pitch. Es un cambio de coordenadas: no movemos físicamente el mundo.

## Perspectiva

En espacio de cámara, `z` significa distancia hacia delante. No es la altura
del mundo; la coincidencia de letra exige prestar atención al espacio usado.

```text
focal = width / (2 * tan(fov_horizontal / 2))
screen_x = width/2  + camera_x * focal / camera_z
screen_y = height/2 - camera_y * focal / camera_z
```

Duplicar la profundidad divide el tamaño aparente por dos. El mismo focal
en ambos ejes evita deformar la proporción. El campo vertical se deduce del
horizontal y de la relación de aspecto.

## Clipping antes de dividir

Nunca proyectes un punto detrás de la cámara esperando que el rasterizador lo
arregle. Cerca de `z=0`, la división produce valores enormes y puede llevar a
conversiones enteras inválidas.

Sutherland–Hodgman recorta cada triángulo contra seis semiespacios: near, far,
izquierda, derecha, arriba y abajo. Para una arista A→B que cruza un plano:

```text
t = distancia(A) / (distancia(A) - distancia(B))
intersección = A + t * (B - A)
```

Interpolamos también UV en el espacio original de esa arista. El polígono
resultante sigue siendo convexo, y se convierte en un abanico de triángulos.
Los buffers locales tienen capacidad 16; un triángulo cortado por seis planos
necesita como máximo nueve vértices.

## Cobertura y coordenadas baricéntricas

Una función de arista evalúa el lado de una línea:

```text
edge(A,B,P) = (Bx-Ax)*(Py-Ay) - (By-Ay)*(Px-Ax)
```

Dividir las tres funciones de arista por el área da los pesos `wa`, `wb`, `wc`.
Dentro del triángulo suman aproximadamente 1. Permiten interpolar atributos.

Se recorre sólo la caja delimitadora recortada a pantalla. La regla top-left
asigna los píxeles exactamente sobre una arista compartida a un único triángulo.
Sin una regla consistente aparecen agujeros o doble cobertura en las diagonales.

El código deja explícitas estas operaciones para estudiarlas. Un siguiente
paso de optimización sería incrementar funciones de arista entre píxeles,
evitando recalcular multiplicaciones; primero hay que medir.

## UV correcta en perspectiva

Interpolar `u` y `v` directamente en pantalla produce texturas que se doblan
al cambiar la triangulación. Lo lineal en pantalla es `u/z`, `v/z` y `1/z`:

```text
inverse_z = wa/za + wb/zb + wc/zc
u = (wa*ua/za + wb*ub/zb + wc*uc/zc) / inverse_z
v = (wa*va/za + wb*vb/zb + wc*vc/zc) / inverse_z
z = 1 / inverse_z
```

La muestra usa nearest neighbour. Las paredes repiten la textura y usan alturas
absolutas para que un tramo superior y uno inferior mantengan alineación.
Para envolver UV negativas se usa `u-floor(u)`, no un cast truncado a entero.

## Depth buffer

Cada píxel conserva su menor Z en espacio de cámara. Se inicializa a infinito.
Una superficie más lejana no reemplaza el color. Esto resuelve oclusión entre
paredes, suelos, techos y sprites sin ordenar todos los triángulos.

Un texel de sprite con alpha menor que 128 no escribe color ni profundidad.
Así el rectángulo invisible alrededor del guardia no tapa el escenario. Este
motor no implementa transparencias semitransparentes ordenadas: los sprites
del mundo usan recorte binario. El canvas de UI sí mezcla alpha sobre el color.

## Sectores y aberturas

Un sector convexo se triangula con un abanico desde su primer vértice. Cada
arista sólida produce un quad, dividido en dos triángulos.

Para un portal entre A y B:

```text
opening_floor   = max(A.floor, B.floor)
opening_ceiling = min(A.ceiling, B.ceiling)
```

Se dibujan los tramos inferiores/superiores y se deja libre la abertura. Si el
techo alcanza el suelo, el paso se cierra. El mismo dato mueve visualmente la
compuerta y bloquea rayos/cuerpos en `world.c`.

Los sprites son quads verticales orientados por el yaw de cámara. Mantener su
verticalidad conserva el aspecto retro al mirar arriba o abajo. Su profundidad
se calcula por píxel, como en cualquier otro triángulo.

## Iluminación y presentación

La luz de sector se atenúa con `light/(1+z*0.045)`, con un mínimo visible. Es una
decisión artística sencilla, no iluminación físicamente basada.

El framebuffer 480×270 ocupa 518 400 bytes de color más 518 400 de profundidad
en esta plataforma. raylib sube el color a una textura y dibuja un único quad
de presentación. La GPU no calcula la perspectiva del mundo.

El escalado usa el mayor entero que cabe. Una ventana 1000×700 muestra 960×540
centrados, con bandas. Si una superficie resultara menor que la resolución
interna, se conserva la proporción mediante escala fraccionaria nearest.
