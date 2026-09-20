# 10 · Gameplay, perseguidores y jefes

## Definición frente a instancia

`ReCharacterDef` es inmutable durante una partida. Contiene dimensiones,
velocidades, percepción, clips y fases. Cien enemigos del mismo tipo comparten
una definición. `ReCharacter` guarda sólo su vida, cuerpo, estado, temporizadores
y memoria. Esta separación evita copiar tablas grandes y permite reiniciar una
partida desde datos limpios.

`ReEntityId` combina índice y generación. Cuando un slot se reutiliza, aumenta
la generación; un identificador viejo deja de encontrar accidentalmente a una
entidad distinta.

## Ciclo del perseguidor

```text
PATRULLA --ve/oye--> PERSECUCIÓN --pierde contacto--> BÚSQUEDA
    ^                                                |
    +----------------- agota memoria ----------------+

PERSECUCIÓN --alcanza con línea libre--> CAPTURA (evento único)
```

En modo `perception`, el campo de visión usa producto punto y un rayo con canal
`RE_BLOCK_SIGHT`. Un vidrio puede dejar pasar ese rayo mientras sigue bloqueando
el cuerpo y el disparo. Un ruido entrega un radio durante un tick; el personaje
recuerda la última posición durante `memory_time`.

En modo `omniscient`, el objetivo se actualiza siempre, pero el personaje aún
debe recorrer una ruta físicamente posible. No atraviesa paredes, techos ni una
ventana cerrada.

La captura exige simultáneamente distancia horizontal, solapamiento vertical y
línea libre. `capture_emitted` garantiza un único evento. La aplicación decide
que `RE_EVENT_CAPTURED` significa game over; el núcleo geométrico no conoce esa
regla.

## Navegación A*

Cada volumen es un nodo y cada portal recíproco una arista. A* rechaza conexiones
estrechas, techos bajos y escalones demasiado altos para el cuerpo. Una puerta
cerrada tiene coste extra; sólo se atraviesa si la definición permite abrirla.
La heurística es la distancia entre centroides y nunca sobreestima una ruta
euclidiana sin costes negativos.

El objetivo intermedio se sitúa un radio dentro del próximo volumen. El punto
exacto del portal pertenece a ambos polígonos y provocaría una selección ambigua.
Este pequeño desplazamiento es una invariante importante del seguimiento.

## Fases de jefe

Las fases se declaran de umbral de vida mayor a menor. Cada una elige seguimiento,
acción, multiplicador de velocidad, enfriamiento y límite de invocaciones. En un
tick con varias condiciones válidas gana la fase de umbral más bajo ya alcanzado.

Acciones incluidas: esperar, cuerpo a cuerpo, proyectil, capturar, invocar y
activar un objeto. La simulación produce eventos; el juego aplica daño, crea el
proyectil concreto o resuelve el identificador del objeto activado. Esto mantiene
el sistema reutilizable.

## Añadir una acción en C

1. Añade un valor a `enum ReGameplayAction`.
2. Amplía el analizador y serializador manteniendo un nombre estable.
3. Emite un evento con los datos mínimos, sin llamar a raylib.
4. Enseña al reproductor o juego a consumirlo.
5. Añade una prueba sin ventana para prioridad, repetición y reinicio.

No guardes punteros a entidades en definiciones: usa `ReEntityId` o IDs de texto.
No reserves memoria desde `re_gameplay_tick`; aumenta límites explícitos si un
juego realmente los necesita y documenta el coste.
