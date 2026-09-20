# 09 · RetroForge Studio

## Qué problema resuelve

El motor sigue siendo una biblioteca C, pero un juego basado en las reglas
incluidas ya no necesita declarar cada habitación o enemigo en código. Studio
edita el manifiesto, mapa, personajes, reglas y diálogos versionados. **Probar**
entrega una copia de esos datos a la misma
biblioteca `retro_gameplay` que usa `retro_player`.

```text
archivos editables -> validación -> copia de prueba -> simulación a 60 Hz
        |                                  |
        +----------- guardar <------------+
                     (sólo por acción explícita)
```

Detener una prueba descarta posiciones de enemigos, puertas abiertas, vidrios
rotos y fases activas. El documento del editor nunca se contamina con estado de
una partida.

## Recorrido recomendado

1. Ejecuta `retro_studio.exe`. Se abre `assets/studio/haunted.retro`.
2. Elige una planta con `+` y `-`. La lista muestra la altura de cada volumen.
3. Pulsa una habitación para editarla. Los círculos de sus esquinas son
   vértices; arrástralos y se ajustarán a una cuadrícula de 25 cm.
4. Usa **+ SALA** para una habitación rectangular, **DUPLICAR** para otra
   habitación al lado o **APILAR** para una planta superior con el mismo plano.
5. Para unir dos habitaciones, selecciona el vértice inicial de una arista,
   pulsa **CONECTAR**, selecciona la arista coincidente en sentido opuesto y
   vuelve a pulsar **CONECTAR**.
6. **PELDAÑO** crea y conecta un volumen un metro hacia fuera y 25 cm más alto.
   Repite sobre su arista exterior para construir una escalera.
7. Elige un personaje y una habitación; **ACTOR** coloca una instancia en el
   centro. **PUERTA** y **VENTANA** actúan sobre una arista ya conectada.
8. Selecciona un personaje y arrastra un PNG sobre la ventana: Studio lo copia
   a `art/` y lo asigna a esa definición. Un WAV sólo se importa. Las referencias
   son rutas relativas, así que mover el proyecto no las rompe.
9. Pulsa **PROBAR**. WASD mueve, botón derecho mira, E abre puertas y el botón
   izquierdo rompe vidrios. Esc vuelve a edición.
10. Guarda con Ctrl+S. Ctrl+Z y Ctrl+Y recorren hasta 32 estados completos.

## Espacios de trabajo

La barra superior separa cinco tareas para reducir densidad:

- **Mapa:** planta, alturas, conexiones, puertas, ventanas y overlays de
  triggers y luces.
- **Entidades:** instancias, personajes y edición de animaciones.
- **Lógica:** tarjetas CUANDO/SI/HACER, prioridad, ejecución única y cooldown.
- **Diálogo:** nodos, ramas y modo de pausa.
- **Prueba:** framebuffer 480×270, actores, pickups e iluminación real.

Los archivos de reglas y diálogos se escriben junto al mapa al guardar. Mientras
hay cambios, Studio crea cada 30 segundos una copia en `.retroforge/autosave`.
Al iniciar, **RECUPERAR** carga esa copia en memoria y la marca como pendiente;
el proyecto real sólo cambia tras pulsar **GUARDAR**.

**? AYUDA** o F1 abre siete recorridos dentro de Studio: habitación/escalera,
sprite/animación, conversación, drop de llave, llave/puerta/objetivo,
perseguidora/checkpoint/jefe y exportación/guardado.

## Animaciones

Cada personaje lista clips y fotogramas. Las flechas recorren ambos niveles; los
botones **CELDA -**, **CELDA +** y **+ FRAME** permiten montar la secuencia y el
slider cambia su duración. **+ PERSONAJE** duplica una definición como punto de
partida. La hoja se divide mediante `cell_width` y `cell_height`; `directions`
puede ser 1, 4 u 8. Las celdas de una dirección son contiguas y luego se repite
la misma cantidad para la dirección siguiente.

El tiempo avanza en simulación, no en dibujo. Por eso pausar el juego congela
la animación y dibujar dos veces nunca duplica un evento de ataque.

## Exportación

**EXPORTAR** guarda primero y ejecuta `tools/export-project.ps1`. El resultado
incluye el reproductor genérico renombrado, `project.retro`, niveles, actores,
arte, avisos y licencias. El ejecutable busca `project.retro` junto a sí mismo y resuelve
todas las rutas desde allí, incluso si la carpeta contiene espacios.

```powershell
powershell -ExecutionPolicy Bypass -File tools/export-project.ps1 `
  -Project assets/studio/haunted.retro -Output dist/Games
```

Las extensiones con acciones C nuevas sí necesitan compilar un reproductor
propio; las acciones incluidas funcionan con el binario genérico.

## Límite actual de varias ventanas

`--workspace 0..4` permite abrir Studio directamente en un espacio concreto y
es útil para capturas o una segunda vista de sólo lectura. La sincronización
editable por named pipes y el bloqueo de un host único todavía no forman parte
del ejecutable entregado. Abrir el mismo proyecto en dos procesos y guardar en
ambos puede sobrescribir cambios; usa un único proceso editable.
