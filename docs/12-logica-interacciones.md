# 12 · Lógica, triggers e interacciones

## La idea central

La capa `retro_gameplay` separa **hechos** de **consecuencias**. El mapa o la
simulación emite un evento; las reglas se recorren por prioridad; cada regla
comprueba sus condiciones y ejecuta sus acciones en el orden escrito.

```text
evento -> cola FIFO -> reglas CUANDO -> condiciones SI -> acciones HACER
                              ^                         |
                              +--- nuevos eventos <----+
```

Una acción nunca llama recursivamente a otra regla. Si genera un evento, éste se
añade al final de la cola. Esto hace determinista el resultado y evita desbordar
la pila de C. Cada tick admite como máximo 256 eventos encadenados; al alcanzar
el presupuesto se vacía la cola, `cycle_limited` queda activo y el juego puede
mostrar el error. Consulta `re_interaction_tick` en `src/gameplay/interaction.c`.

Las definiciones de `ReInteractionDefinitions` son inmutables durante la
partida. `ReInteractionState` contiene variables, inventario, objetivos,
cooldowns, triggers consumidos, luces y pickups. Esta división permite iniciar
otra partida copiando estado inicial y guardar sin serializar punteros.

## Formato `retro_rules 1`

El archivo admite comentarios `#`, identificadores ASCII sin espacios y textos
entre comillas. Un ejemplo reducido:

```text
retro_rules 1

variable power bool false
item brass_key "Llave de latón" 1
objective find_key "Consigue la llave"

trigger basement box 4 8 0 2 1.5 1.2 true
light warning point 4 8 2 1.0 0.15 0.08 7 1.4 false

rule enemy_drops_key entity_died caretaker-main 100 true 0
action spawn_pickup brass_key int 1
action message - text "El enemigo dejó caer una llave."
end

rule unlock interact stair_panel 110 true 0
condition item brass_key ge int 1
action take_item brass_key int 1
action open_barrier stair-door none -
end
```

La cabecera de una regla es:

```text
rule ID EVENTO ORIGEN PRIORIDAD UNA_VEZ COOLDOWN
```

Las prioridades altas se evalúan primero; un empate conserva el orden del
archivo. `ORIGEN` puede ser `*`. `UNA_VEZ` impide una segunda ejecución aun si el
mismo evento llega de nuevo. El cooldown se expresa en segundos de simulación.

Los tipos de valor son `bool`, `int`, `float`, `text` y `none`. Las comparaciones
son `eq`, `ne`, `lt`, `le`, `gt` y `ge`. Un archivo inválido no modifica la
definición anterior: el cargador construye un candidato, lo valida y sólo luego
lo publica.

## Volúmenes trigger 3D

Hay tres formas:

- `sector ID_SECTOR ONCE`: ocupa el volumen transitable completo del sector.
- `box X Y Z HX HY HZ ONCE`: caja centrada en `(X,Y,Z)` con semiejes `H*`.
- `cylinder X Y Z RADIO MEDIA_ALTURA ONCE`: cilindro vertical.

Cada tick compara la pertenencia actual con `trigger_inside` y produce
`trigger_enter`, `trigger_stay` o `trigger_exit`. Se comprueba XY y separación
vertical; dos plantas coincidentes no comparten un trigger. Un trigger `once`
marca `trigger_consumed` después de entrar.

Studio dibuja los triggers en magenta y las luces con su radio real en el espacio
**MAPA**. El playtest usa la misma implementación que `retro_player`.

## Interacción y línea de visión

`re_interaction_interact` recibe origen, dirección normalizada y distancia en
metros. Selecciona el marcador interactuable más cercano dentro de un cono
pequeño y confirma que `re_world_trace` no encuentre antes una barrera que
bloquee visión. Por eso E no atraviesa paredes, vidrios opacos ni pisos.

Los IDs conectan sistemas sin punteros persistentes:

- el marcador `fuse_box` origina `interact fuse_box`;
- la barrera `stair-door` es destino de `open_barrier`;
- el actor `caretaker-main` origina `entity_died`;
- el ítem `brass_key` enlaza drop, inventario y condición.

Cambiar un ID exige actualizar sus referencias. El panel de errores debe tratar
una referencia rota como dato inválido, nunca como una búsqueda silenciosa.

## Receta: enemigo que suelta una llave

1. Crea un ítem con pila máxima uno.
2. Coloca una instancia de enemigo y conserva su ID estable.
3. Crea una regla `entity_died` con ese ID como origen.
4. Añade `spawn_pickup` y marca la regla `once`.
5. Crea una segunda regla que reaccione a `item_picked` y active el objetivo.
6. En la puerta, condiciona la apertura a `item ... ge 1` y quita el ítem.

Haunted implementa exactamente este recorrido. La posición del drop procede del
payload del evento de muerte. La prueba `interaction_journey` confirma que la
llave sólo se crea una vez, se recoge, se consume y no reaparece al restaurar un
checkpoint anterior.

## Añadir una acción en C

Una capacidad nueva requiere cuatro cambios deliberados:

1. Añadir el valor a `enum ReActionKind` en `interaction.h`.
2. Añadir su nombre en las tablas de carga y guardado de `interaction.c`.
3. Implementar el caso en `execute_action` sin reservar memoria por tick.
4. Añadir una prueba que parta de un evento y observe el efecto público.

La acción debe validar IDs y tipos antes de mutar. Si genera otro hecho, debe
usar `re_interaction_emit`; no debe ejecutar reglas directamente.

