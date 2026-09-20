# 07 · Decisiones, ejercicios y evolución

## Relación con la investigación original

El documento aportado es una guía conceptual extensa. Su versión original queda
intacta. Sus marcadores de cita internos no son enlaces recuperables por sí solos;
la documentación del proyecto añade enlaces oficiales donde corresponde.

| Idea original | Decisión implementada |
|---|---|
| C11 como base | C23 publicado y comprobado por el compilador. |
| raylib como plataforma | Se conserva; el mundo se rasteriza en CPU propia. |
| Sectores 2.5D | Se conservan con polígonos convexos y portales recíprocos. |
| Rasterización por columnas | Triángulos con perspectiva para admitir pitch real y salto. |
| Portales antes de BSP | Portales para geometría/física/grafo; visibilidad por depth buffer. |
| BSP como evolución | Diferido hasta que mediciones justifiquen partición espacial. |
| Motor y juego | Bibliotecas separadas y segundo consumidor independiente. |
| Desarrollo progresivo | Ventana, píxeles, geometría, mundo, reglas y pruebas observables. |
| No optimizar prematuramente | float, un hilo, buffers contiguos y perfilado explícito. |

Este proyecto se inspira en ideas de FPS retro. No reproduce el renderer de
Doom ni promete compatibilidad con WAD, demos históricas o sus reglas exactas.

## Por qué no un raycaster de cuadrícula

Un raycaster tipo Wolfenstein es un excelente ejercicio, pero una cuadrícula de
paredes de altura uniforme limita los sectores y la mirada vertical que pediste.
La malla derivada de sectores conserva una representación sencilla del mapa y
permite estudiar el pipeline 3D sin delegarlo a `BeginMode3D`.

## Por qué capacidad fija para mapas y entidades

Los límites están declarados, validados y documentados. Evitan reasignar memoria
durante el juego y mantienen índices estables. No constituyen un sistema de
streaming para mundos grandes. Si cambian los requisitos, usa arrays dinámicos
con capacidad y generación de identificadores; no elimines los límites sin
revisar todos los accesos y costes.

## Costes que puedes medir

| Operación | Orden aproximado |
|---|---|
| Validación de convexidad | O(vértices²) por sector, sólo al cargar. |
| Validación de solapamiento | Comparación de pares de sectores con SAT. |
| Dibujo | Triángulos + píxeles dentro de sus cajas de pantalla. |
| Movimiento | Hasta 4 barridos por todas las aristas. |
| Raycast | Todas las aristas y planos; sin índice espacial. |
| BFS | O(V+E), arrays temporales de capacidad fija. |

La complejidad asintótica no cuenta toda la historia: localidad de memoria,
ramificaciones, divisiones y tamaño real del nivel también importan. Una
estructura sofisticada puede perder frente a un array lineal en seis sectores.

## Ejercicios, en orden

1. **Vectores:** dibuja en el automapa forward y right. Comprueba perpendicularidad.
2. **Memoria:** calcula bytes de color y profundidad para 320×180 y 640×360.
3. **Cámara:** cambia FOV de 75° a 90° y explica qué ocurre con focal.
4. **UV:** sustituye temporalmente la interpolación correcta por afín; observa
   la distorsión y la prueba `renderer_contracts` que debe fallar.
5. **Clipping:** añade contadores por plano y mira una pared desde muy cerca.
6. **Tiempo:** limita presentación a 30/60/120 FPS sin cambiar la velocidad de andar.
7. **Colisión:** cambia radio y paso; crea un test antes de ajustar un caso límite.
8. **Datos:** cambia la geometría del mapa manteniendo conexiones recíprocas.
9. **Estados:** añade un aviso visual antes del ataque enemigo.
10. **Reutilización:** construye el paseo descrito en el capítulo 06.

## Hoja de ruta razonada

Primero mejorar la base observable: interpolar actores, navegación con radio,
funnel y separación de multitudes; añadir métricas y pruebas de casos difíciles.
Después incorporar audio espacial y memoria de percepción.

Para niveles mayores, medir antes de elegir entre portal culling, rejilla
espacial o BSP. Los datos visibles y los físicos pueden necesitar índices
distintos. Conserva una implementación sencilla como oráculo de pruebas.

Studio, habitaciones superpuestas, reglas y partidas guardadas ya forman parte
de la base. Los proyectos siguen usando texto validado; las partidas usan un
snapshot binario versionado, acotado, con CRC y asociado al ID del proyecto.

Los siguientes saltos grandes son IPC editable entre ventanas de Studio,
prefabs/materiales como archivos independientes, audio espacial, timers de autor
y acciones para crear o mover entidades arbitrarias. Multijugador, pendientes,
transparencias mezcladas y plugins dinámicos siguen fuera del alcance actual.
