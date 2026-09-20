# 13 · Diálogos, checkpoints, guardado e iluminación

## Conversaciones ramificadas

`retro_dialogue 1` almacena nodos por ID. Cada nodo tiene hablante, texto, modo
de pausa, un siguiente nodo opcional y hasta cuatro elecciones.

```text
retro_dialogue 1
node survivor_intro "Elena" true "¿Puedes oírme?" next end
choice help "Te ayudaré." survivor_thanks - bool true
choice leave "Ahora no." end power bool false
end
```

La condición de una opción consulta una variable booleana. `-` significa que la
opción siempre está disponible. Elegir emite `dialogue_choice`; alcanzar `end`
emite `dialogue_finished`. Una regla puede usar estos eventos para entregar un
objeto, cambiar un objetivo o iniciar otra conversación. El texto y sus efectos
permanecen separados, por lo que traducir una línea no altera la lógica.

## Checkpoint y retry exactos

Un checkpoint captura:

- cuerpo, salud y vidas del jugador;
- variables, inventario y objetivos;
- reglas ejecutadas y cooldowns;
- triggers, luces y pickups;
- barreras, personajes y estado del generador aleatorio.

Al restaurarlo se descartan las colas de eventos posteriores, el diálogo activo,
victoria y game over. Esa limpieza evita repetir un drop o ejecutar una acción
de la línea temporal descartada.

El manifiesto configura la muerte:

```text
death restart_level
death checkpoint
death limited_lives
death permadeath
```

Haunted usa `checkpoint`: la captura devuelve al último punto sin consumir una
vida. `limited_lives` resta una vida después de restaurar; al agotarlas vuelve al
menú. `restart_level` recompone el estado inicial. `permadeath` termina el intento
y vuelve al menú principal.

## Partidas persistentes

`re_save_write` construye el payload completo en memoria, calcula CRC32, escribe
`archivo.tmp`, cierra y reemplaza el destino de forma atómica. La cabecera
incluye magia `RFSAVE2`, versión, tamaño e ID del proyecto. `re_save_read` valida
todo antes de tocar el runtime; una ranura corrupta o de otro juego produce un
error recuperable y conserva la partida actual.

El reproductor ofrece:

- tres ranuras manuales desde Pausa;
- autoguardado al crear o mover un checkpoint;
- F5 para guardado rápido y F9 para carga rápida;
- Continuar desde el autoguardado, con fallback al guardado rápido.

Los archivos viven en `%LOCALAPPDATA%/RetroForge/<project-id>`. El ejecutable
puede instalarse en `Program Files` o ejecutarse desde una carpeta de sólo
lectura. El formato binario es deliberadamente local a esta versión de Windows:
un cambio incompatible incrementa versión y tamaño en vez de interpretar bytes
con otra estructura.

## Iluminación retro

Cada sector conserva una luz ambiental. Después del mundo y los sprites,
`re_apply_lights` recorre tiles de 16×16, elige hasta ocho de las 32 luces activas
y reconstruye la posición aproximada del píxel desde profundidad y cámara.

Para una luz puntual, la contribución usa una caída cuadrática acotada:

```text
d = distancia(píxel, luz)
a = max(0, 1 - d / radio)
energía = intensidad × a²
```

La luz de cono multiplica esa energía por un factor angular. `flicker` modula la
intensidad con tiempo de simulación. El color final se cuantiza a pasos de 16 para
mantener la estética retro. No hay sombras dinámicas en esta versión.

Agrupar por tiles evita probar 32 luces para cada uno de los 129.600 píxeles. El
límite superior aproximado pasa a ser `tiles × 32 + píxeles × 8`, con memoria
fija y sin asignaciones en el frame.

## Portada y recursos

`title_art` en `retro_project 2` apunta a un PNG relativo. El reproductor lo
adapta con recorte *cover* y nearest, sin exigir una resolución concreta. La
portada de Haunted está en `assets/studio/art/haunted-title.png`; es arte original
generado para este proyecto y su procedencia se registra en `assets/README.md`.

