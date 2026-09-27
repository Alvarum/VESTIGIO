# Auditoría integral de `js-game` para VESTIGIO

Fecha: 2026-09-27. Fuente antigua: `C:/Users/alvar/Documents/dev/js-game`, HEAD `bca6f57af2805809e87674297a2555da3b6a3957`. Destino: VESTIGIO en este repositorio, con 17/30 tickets originales integrados al comenzar esta auditoría. La fuente antigua se leyó sin modificarla. El checkout de VESTIGIO tenía cambios de código concurrentes; esta entrega sólo toca planificación e investigación.

Este informe amplía la [arqueología selectiva anterior](02-threejs-engine-archeology.md). El [inventario por archivo](14-js-game-inventory.md) registra **90 módulos JS de motor, 18 de juego/demos, 63 documentos `.planning` y 105 GLB**, con SHA-256 abreviado, conexión estática y ticket de destino. Los 108 JS pasaron `node --check` el 2026-09-27. La prueba valida sintaxis; no valida imports de CDN, WebGL, audio, UX, rendimiento ni el recorrido jugable.

## Cómo leer la evidencia

| Clasificación | Qué se observó | Qué no autoriza afirmar |
|---|---|---|
| Código | Archivo y símbolos en la fuente | Que la función se monte o se use |
| Conectado a escena | Ruta de imports estáticos desde `game/main.js` o `game/sandbox/main.js`; se miró creación/update de los sistemas críticos | Que el navegador ejecute la ruta sin errores o que el resultado visual sea correcto |
| Probado en ejecución | Requiere arranque real, acciones y resultado registrado | No se obtuvo para `js-game` en esta auditoría |
| Aprobado visualmente | Requiere captura y revisión humana de composición, legibilidad y comportamiento | No se obtuvo para `js-game` en esta auditoría |
| Sólo plan | Requisito, idea, roadmap, resumen histórico o checklist | Que exista implementación o PASS actual |

**Demo visual: `NOT_RUN`.** No hubo navegador automatizable disponible en el entorno. El proyecto usa importmaps/CDN en `index.html` y `sandbox.html`; `node --check` no resuelve esas dependencias. La checklist de fase 36 conserva casillas de aceptación sin completar y una tabla de evidencia vacía. El hecho de que `game/sandbox/main.js` instancie un sistema se registra como conexión estática, nunca como prueba visual. La columna de disposición del inventario se refiere a la **capacidad equivalente en el nuevo VESTIGIO**, no a código de RetroForge.

## Dos experiencias y sus recorridos observables pendientes

| Experiencia | Cableado encontrado | Recorrido que deberá probarse en navegador | Estado |
|---|---|---|---|
| La Casa, `index.html` / `game/main.js` | Apartamento, habitaciones, cadáver/puertas, estado y acciones, audio, clima, niebla `ExteriorFog`, scripts, UI y `SceneEditor` tras `?editor=1` | Moverse, apuntar, abrir menú, ejecutar interacción y secuencia, activar editor, crear objeto, guardar, recargar y jugar | Código/conexión; visual `NOT_RUN` |
| Sandbox, `sandbox.html` / `game/sandbox/main.js` / `SandboxLevel.js` | Estaciones de puzzles, carry, agua, vegetación, niebla de capas y pase volumétrico, AI, armas, linterna, vitals, tres alturas/zonas, UI y postprocesado | Ruta guiada y estaciones; consola, foco del mouse, FPS, guardado y limpieza al cambiar escena | Código/conexión; visual `NOT_RUN` |

`SandboxLevel.js` monta una estación de secuencia real en `_buildScriptStation` y una estación rotulada como `AnimationController` en `_buildF2RoomAnimState`. Esta última crea `THREE.AnimationMixer` directamente sobre una malla dummy; **no ejercita el wrapper ni un clip GLB**. `AnimationController.js` sí contiene reproducción, crossfade y evento `anim:end`, y `Furniture.js` lo instancia cuando el GLB tiene animaciones. Por tanto el controlador existe en código y tiene una ruta condicional en La Casa, pero la estación del sandbox no lo demuestra.

### Guion de prueba visual pendiente

1. Servir `js-game` por HTTP local y abrir `index.html`, `index.html?editor=1` y `sandbox.html` en un navegador con WebGL. Anotar navegador, GPU, resolución, FPS y errores de consola desde el primer frame.
2. En La Casa: recorrer patio/apartamento, comparar niebla por capas en reposo y movimiento, accionar puertas/interacciones, ejecutar diálogo y elección; repetir con foco perdido/recuperado.
3. En el editor antiguo: colocar un GLB, una luz, sala irregular, vano/ventana y trigger; exportar, recargar y jugar cada tipo. Guardar `scene.json` de prueba y registrar el resultado exacto, incluyendo fallos esperables por el contrato de arrays.
4. En sandbox: completar el recorrido de la checklist de fase 36, incluyendo carry, agua, vegetación, luz/post-FX, scripts, animación, AI, combate, audio y cambios de piso. Capturar antes/después y consola por estación; comprobar desmontaje y reentrada.
5. Marcar cada estación `PASS`, `FAIL` o `NOT_RUN` con evidencia. Una captura de un efecto no certifica su editor, persistencia ni rendimiento. La aprobación visual humana se solicita sobre esas capturas y recorrido reproducible.

## Matriz de traslado: autoría y mundo

| Capacidad anterior y fuente | VESTIGIO observado | Disposición / ticket | Aceptación clave |
|---|---|---|---|
| Selección, gizmos, snap, undo/redo de `SceneEditor` | E02 integrado con gizmos GPU, multiselección y round-trip | Ya existe `E02`; `H01` añade colocación directa de nuevos tipos en la escena | Crear en Studio → guardar → cerrar → abrir → jugar; Play no muta Editar |
| Browser de modelos y packs en `SceneEditor` / `manifest.json` | E03 aún planificado; edición anterior colocaba un pilar predefinido | Parcial `E03`, `H02` | GLB, nodo/submodelo, identidad y referencias estables |
| Props geométricos de `PropFactory` | No hay autoría equivalente general | Falta `H03` | Receta paramétrica y regeneración con material/collider |
| `RoomBuilder`: perímetro, paredes, suelo, techo, UV | E04 sólo tiene una sala fija con vano | Parcial `E04`, `H09` | Perímetro irregular editable; suelo/techo y colisión concordantes |
| `WallHoleTool`: vanos y modelos de puerta/ventana | Vano fijo; no ventana editable | Parcial `E04`, `H05` | Vano, marco, vidrio y collider sobreviven reapertura |
| `LightPlacer` + `FlickerLight`: point, spot, directional y patrones | Luces puntuales de Atrium sin colocador visual | Parcial `V01`, `H04` | Colocación y parámetros de luz persistentes, patrones medibles |
| `TriggerZonePainter` + `TriggerZone`: enter/exit/once/cooldown | Puerta parcial; no pintor de triggers | Parcial `S03`, `H07` | Volumen visible en Studio, evento correcto en Player |
| `FloorManager`: pisos, ghosting y luces por planta | Una sala/altura principal | Falta `H06`, `H10` | Piso, conectores, huecos verticales y colisión consistentes |
| `SceneManager` / `SceneLoader` | Falta gestión editorial completa | Falta `H08` | Carga candidata, rollback si falla y savegame separado |
| Ideas de ascensor y pod de `.planning` | No están en VESTIGIO | Idea futura con ticket `H11`, `H12` | Prefabs parametrizados del núcleo, activables por proyecto |

Los componentes heredados se modelarán sobre el documento nativo y la Tool API de VESTIGIO. La autoría geométrica guardará una **receta**, no sólo triángulos derivados; el mismo dato generará render, collider y vistas del editor. La coordenada antigua Three.js es Y-up; VESTIGIO usa Z-up. Los tickets no piden compatibilidad binaria ni migración de `scene.json` antiguo.

## Matriz de traslado: imagen y ambiente

| Capacidad anterior y fuente | VESTIGIO observado | Disposición / ticket | Prueba necesaria |
|---|---|---|---|
| `ExteriorFog` + `FogShaders`: mantos superpuestos, cuatro alturas de niebla, filamentos, motas, deriva/viento y ruido `fbm` | Shader GPU interpola un color por distancia entre `fog_start` y `fog_end` | Falta la apariencia local `V03`; V01 conserva fog básico | Capturas comparables con capas, movimiento, oclusión y parámetros de escena |
| `VolumetricFog`: pase raymarch de 8 pasos | No hay pase volumétrico | `V04` | Depth real ligado al target, prueba de oclusión y orden antes de PSX |
| `PostProcessor`, `PS1Shader`, `HorrorFXShader`, heat distortion | Perfil limpio/retro con cuantización y Bayer, sin pila editorial | `V05` tras V02 | Orden de passes, resize, presupuesto GPU |
| `Fluid`, `Neon`, `Organic`, `Puddle`, `TVStatic`, `Holographic` shaders | Materiales básicos del renderer | `V06` | Presets editables con recursos compartidos |
| `ParticleSystem`, `FireSystem` | Sin emisor/fuego del nuevo motor | `V07`, `V08` | Pool/limpieza, humo y luz vinculada |
| `DecalSystem`, `BreakableGlass` | Sin calcomanías ni ruptura | `V09` | Decals acotados, evento y collider de vidrio |
| `WaterSystem` y capa de fluido de idea v6 | Sin agua | `V10` | Superficie, entrada/salida y estado submarino |
| `SkySystem`, `Weather`, `VegetationSystem` | Sin estos sistemas | `V11`, `V12`, `V13` | Cielo/ciclo, lluvia/truenos y vegetación instanciada |
| `MirrorSystem`, `LensFlare`, `VideoPlayer` | Sin equivalentes | `V14`, `V15`, `V16` | Reflexión/oclusión/video con resize y liberación |
| `FleshInfestation` y efectos orgánicos de v6 | Sin equivalente | `V17` | Material compartido, instancias y estado por proyecto |
| Modo foto en investigación de v6 | Sólo idea | `X03` | Pausa/cámara/captura sin cambiar partida |

**Niebla texturizada:** en el código antiguo el detalle parece principalmente **procedural en shader**, no una textura de imagen identificada como fuente obligatoria. `ExteriorFog` sí acumula hojas transparentes, ruido, movimiento y partículas; por eso la niebla lineal actual de VESTIGIO no reproduce ese resultado. `V03` busca recuperar la composición visible; `V04` trata por separado profundidad/volumen. No se debe sustituir la una por la otra sin evidencia visual.

## Matriz de traslado: mecánicas, audio e interfaz

| Capacidad antigua | Estado actual / decisión | Tickets |
|---|---|
| Interacción por raycast, acciones condicionadas, bloqueo visible y foco | La puerta de VESTIGIO sólo cubre parte de la semántica | `S03`, `K01`, `U02` |
| EventBus, StateMachine y estado guardable | Contratos de eventos y persistencia por componente aún incompletos | `K02`, `P01` |
| ScriptEngine, secuencia de Día 1, timeline y dolly | Sin sistema narrativo equivalente general | `K03`, `K04` |
| Clips glTF, crossfade, skeleton y evento fin | Dos objetos rígidos de código no prueban importación ni skin | `A02`, `K04` |
| PuzzleManager, cerradura, terminal, cable, slider, carry | Sin sistemas del nuevo motor | `K05`, `K06` |
| AIController, vitals, linterna, armas, ragdoll | Sin equivalentes integrados | `K07`–`K11` |
| Ideas de criatura, impacto por parte/desmembramiento | Investigación, con límites técnicos no demostrados | `K12`, `K13` |
| AudioWorld, ReverbZones, FootstepSystem, MusicDirector | A01 cubre audio parcial de Atrium | `A03`–`A06` |
| Diálogo, subtítulos, elecciones, inventario, inspección, documentos/códice | Sin recorrido común del nuevo motor | `U01`–`U05` |
| HUD, HUD diegético, teléfono, Theme/i18n, settings/foco/pantallas | Sin UI equivalente general | `U06`–`U09` |
| Ideas de inventario en cuadrícula y proyección holográfica | Sólo investigación de v6 | `U10`, `U11` |
| TouchInput y plan móvil | Input común de escritorio existe; plataforma táctil sin verificar | `I02` |

La decisión de producto del usuario es que puzzles, inventario, combate, horror orgánico y UI heredada formen parte del **núcleo modular**, con contratos de documento/runtime/Studio y activación por proyecto. Esto no obliga a cada juego a mostrar esas mecánicas. Se revisa C06 para reflejarlo. El contenido narrativo concreto de La Casa sirve como caso de prueba, no como comportamiento obligatorio ni como migración.

## Defectos y riesgos encontrados antes de reutilizar

1. **Exportación incompatible:** `SceneEditor._export()` escribe `position`, `rotationDeg` y `scale` como arrays; `SceneLoader.load()` lee `.x/.y/.z`. Además, el exportador genera `version: 2` y serializa props genéricos; `SceneLoader` contempla v3, `floors` y `floorHoles`. Herramientas de luces, triggers, salas y huecos no tienen una receta completa en ese export. Es una incompatibilidad de contrato demostrada por lectura, pendiente de reproducción visual.
2. **Persistencia parcial:** `_autosave()` recorre sólo `_spawned`; guardar en localStorage no demuestra que cada entidad de herramientas vuelva a aparecer tras cerrar y abrir. En VESTIGIO cada ticket de autoría exige round-trip y undo/redo donde corresponda.
3. **Geometría de sala irregular:** `RoomBuilder._buildFloorCeiling()` usa mínimos y máximos X/Z y `PlaneGeometry(w,d)`. Un polígono cóncavo o irregular puede terminar con suelo fuera del contorno. `H09` exige triangulación del perímetro y collider derivado de la misma receta.
4. **Depth de niebla volumétrica:** `VolumetricFog` declara `tDepth` y lo deja `{ value: null }`; no se localizó asignación posterior del depth texture. No se afirma que el pase funcione. `V04` exige target/depth verificados y oclusión visible.
5. **Prueba de animación confusa:** la estación del sandbox rotula el controlador, pero usa `THREE.AnimationMixer` directo sobre un dummy. `A02` exige clip GLB/skin por instancia y evidencia real.
6. **Catálogos especializados vacíos:** `assets/models/doors/index.json` y `windows/index.json` son `[]`; los GLB en `doors and gates/` no prueban un catálogo funcional de ventanas o puertas.
7. **Assets y licencias:** el manifest general enumera los 105 GLB, pero no hay atribución/licencia por archivo. La búsqueda de ruta, nombre y clave no encontró referencias literales a esos modelos en JS/JSON de motor y juego, fuera del manifest; pueden cargarse dinámicamente desde el catálogo, por lo que esto no demuestra que nunca se usen. El README sugiere sitios de descarga, sin identificar la procedencia efectiva de cada modelo. `X02` debe resolver fuente y permiso antes de copiar alguno.
8. **QA histórica:** `.planning/STATE.md` marca v6 en planificación/0 %, mientras el código contiene módulos v6 y el roadmap usa estados que no coinciden con archivos actuales. Las tres `36-xx-SUMMARY.md` no versionadas son relatos, no aceptación. La checklist de fase 36 sigue sin completar. No se suma ningún PASS histórico.

## Ideas examinadas y disposición

| Idea antigua | Disposición | Motivo |
|---|---|
| Ascensor, pod, fluido amniótico, criaturas y golpe por parte | Tickets `H11`, `H12`, `V10`, `K12`, `K13` | Son componentes/presets reutilizables si se separan del argumento de La Casa |
| Inventario holográfico, cuadrícula y modo foto | Tickets `U11`, `U10`, `X03` | Funciones generales, con aceptación de foco, rendimiento y persistencia |
| Plataforma táctil | Ticket `I02` | Puede venir después del host Windows; no se declara implementada por la existencia de `TouchInput.js` |
| Generador aleatorio completo de niveles | No trasladable como requisito de esta ampliación | La propia investigación de v6 lo identifica como anti-feature para horror autorado; `H09`/`H06` cubren composición manual |
| Multijugador/co-op | No trasladable en este alcance | La investigación lo excluye y el motor/planes presentes no tienen red ni servidor |
| Portar JS, DOM, WebAudio o importmaps literalmente | No trasladable | VESTIGIO es C/Studio WPF/GPU nativo; se recuperan contratos y experiencia, no compatibilidad del prototipo |

## Backlog y puertas de aceptación

La fuente de tickets es [backlog.json](../implementation/backlog.json); [TASKS.md](../implementation/TASKS.md) y [WAVES.md](../implementation/WAVES.md) son vistas generadas. Se conservaron los 30 tickets originales y sus 17 estados `INTEGRATED` al comenzar la auditoría. Se precisaron criterios de E03, S03, V01, V02, A01, A02, P01 y Q01, y se añadieron **60 tickets** trazables (incluidos `X01` demo, `X02` procedencia, `ZA1` cierre). La selección de submodelos corresponde a H02 y las capacidades avanzadas de salas/ventanas/pisos a H05/H06/H09/H10; E03 y E04 mantienen un cierre acotado y verificable. `Z01` sigue cerrando sólo los 30 originales; `ZA1` depende de ese cierre, de la demo y de la procedencia.

La aceptación de cada capacidad se ejecutará en un candidato concreto. Las pruebas de autoría usan **crear → guardar → cerrar → abrir → jugar**, las de mecánica guardan y restauran estado cuando procede, y las visuales comparan captura, consola y coste GPU. `X01` recorre todas las estaciones heredadas; hasta disponer de navegador y revisión visual permanece `NOT_RUN`. El plan valida su DAG, pero ese PASS no equivale a función implementada. Cada ticket nuevo requiere encargo propio antes de ejecutarse.
