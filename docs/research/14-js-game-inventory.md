# Inventario trazable de `js-game`

Generado por `python docs/research/build-js-game-inventory.py --source <ruta-js-game>`.
Instantánea local: `bca6f57af2805809e87674297a2555da3b6a3957`; incluye tres resúmenes no versionados de fase 36.

**Alcance:** 90 módulos de motor, 18 archivos de juego, 63 archivos `.planning`, 105 GLB. SHA-256 abreviado permite detectar cambios.
`Conectado` significa alcanzable por imports estáticos desde `game/main.js` (Casa) o `game/sandbox/main.js` (Sandbox). No acredita ejecución, uso del símbolo ni calidad visual. `Código` significa que se encontró el archivo sin ruta estática desde esas entradas. `Sólo plan` nunca es prueba del motor.
La disposición compara capacidades del **nuevo VESTIGIO**. `Parcial` exige el ticket indicado; `Existe` remite al ticket ya integrado; `Falta` indica ausencia de la capacidad específica.

## 90 módulos de motor

| Módulo | Intención localizada | Métodos de ciclo | Ruta estática | Evidencia | VESTIGIO | Ticket | SHA-256 |
|---|---|---|---|---|---|---|---|
| `engine/AIController.js` | AIController — Controlador de IA para NPCs con FSM (idle→investigate→chase→lost). | constructor, stop, update | Sandbox | Conectado | Falta | `K07` | `378502923117` |
| `engine/AnimationController.js` | AnimationController — Wrapper sobre THREE.AnimationMixer para clips embebidos en GLTF. | constructor, play, stop, pause, update, dispose | Casa/Sandbox | Conectado | Parcial | `A02` | `25b2a054ff52` |
| `engine/Assert.js` | Assert — guard clauses for engine module boundaries. | — | — | Código | Existe | `R03` | `0e3a359c30ef` |
| `engine/AssetLoader.js` | AssetLoader — Cargador centralizado de assets (texturas, audio, GLTF). | constructor, dispose | — | Código | Existe | `R02` | `57a0981db67e` |
| `engine/AudioManager.js` | AudioManager — Gestor de audio basado en Web Audio API. | constructor, init | Casa/Sandbox | Conectado | Parcial | `A01` | `1a4d3a0d8cee` |
| `engine/AudioWorld.js` | AudioWorld — Audio 3D posicional con Web Audio PannerNode. | constructor, init, stop, update | Casa/Sandbox | Conectado | Falta | `A03` | `1d46b76029f0` |
| `engine/Camera.js` | Camera — Wrapper de THREE.PerspectiveCamera para FPS. | constructor | Casa/Sandbox | Conectado | Existe | `R03` | `32cfcb2f07b9` |
| `engine/CameraDolly.js` | CameraDolly — Automated camera movement along a CatmullRomCurve3 path (REQ-v6-E01). | constructor, play, stop, update | Sandbox | Conectado | Falta | `K04` | `b61a258fadfb` |
| `engine/CarrySystem.js` | CarrySystem — Recoger objetos 3D y depositarlos en zonas específicas. | constructor, update, destroy | Sandbox | Conectado | Falta | `K06` | `5c01c982fd69` |
| `engine/Debug.js` | Debug — Overlay de desarrollo con backtick (') para toggle. | constructor, mount, unmount | Casa/Sandbox | Conectado | Falta | `Q01` | `af4abfbe6acd` |
| `engine/DecalSystem.js` | DecalSystem — Calcomanías proyectadas sobre superficies. | constructor, update, destroy | Sandbox | Conectado | Falta | `V09` | `22753b012087` |
| `engine/effects/BreakableGlass.js` | BreakableGlass — Efecto de cristal roto con fragmentos que caen. | constructor, update, destroy | Sandbox | Conectado | Falta | `V09` | `9e840b96070a` |
| `engine/effects/ExteriorFog.js` | Capas de niebla exterior, filamentos, viento y motas con shader procedural | constructor, update, dispose | Casa/Sandbox | Conectado | Falta | `V03` | `b400307e57cc` |
| `engine/effects/FireSystem.js` | FireSystem — Fuego con partículas + PointLight dinámica + humo. | constructor, stop, update, destroy | Sandbox | Conectado | Falta | `V08` | `69266237bbc3` |
| `engine/effects/FleshInfestation.js` | FleshInfestation — InstancedMesh body-horror overlay (REQ-v6-B01, CC-02). | init, update, destroy | Sandbox | Conectado | Falta | `V17` | `a43ee0447705` |
| `engine/effects/FlickerLight.js` | FlickerLight — Luz dinámica con patrones de parpadeo. | constructor, update, dispose | Casa/Sandbox | Conectado | Parcial | `H04` | `645e84d239fe` |
| `engine/effects/FogShaders.js` | Constantes GLSL para el efecto de niebla exterior. | — | Casa/Sandbox | Conectado | Falta | `V03` | `e4ed2baf30f9` |
| `engine/effects/LensFlare.js` | LensFlare — Destellos de lente basados en sprites con test de oclusión. | constructor, update, destroy | Sandbox | Conectado | Falta | `V15` | `bf33ca3df21f` |
| `engine/effects/ParticleSystem.js` | ParticleSystem — Emisor de partículas GPU genérico. | constructor, update, dispose | Casa/Sandbox | Conectado | Falta | `V07` | `0940ae7a2476` |
| `engine/effects/SkySystem.js` | SkySystem — Cielo procedural con ciclo día/noche, sol y estrellas. | constructor, update, destroy | Sandbox | Conectado | Falta | `V11` | `5140319d875b` |
| `engine/effects/VegetationSystem.js` | VegetationSystem — Hierba instanciada con viento + árboles procedurales. | constructor, update, destroy | Sandbox | Conectado | Falta | `V13` | `50ca5fc47197` |
| `engine/effects/VolumetricFog.js` | VolumetricFog — Full-resolution WebGLRenderTarget fog composited | constructor, mount, unmount, update | Sandbox | Conectado | Falta | `V04` | `52c60c4de635` |
| `engine/effects/WaterSystem.js` | WaterSystem — Superficies de agua animadas + modo submarino. | constructor, update, destroy | Sandbox | Conectado | Falta | `V10` | `1905a785c1ad` |
| `engine/effects/Weather.js` | Weather — Sistema de lluvia, niebla y truenos. | constructor, init, update | Casa/Sandbox | Conectado | Falta | `V12` | `db7a5f9b8f64` |
| `engine/Entity.js` | Entity — Clase base para todos los objetos del juego 3D. | constructor, update, destroy | Casa/Sandbox | Conectado | Existe | `R01` | `ae397cc2eb31` |
| `engine/EventBus.js` | EventBus — Canal de comunicación desacoplado (pub/sub). | constructor | Casa/Sandbox | Conectado | Parcial | `K02` | `424d83801bad` |
| `engine/Flashlight.js` | Flashlight — SpotLight with battery drain + VolumetricSpot shader (REQ-v6-D01, CC-05). | constructor, mount, unmount, update | Sandbox | Conectado | Falta | `K09` | `9e163cd42a39` |
| `engine/FloorManager.js` | FloorManager — Multi-floor state and light culling. | constructor | Sandbox | Conectado | Falta | `H06` | `f2285711a3fc` |
| `engine/FootstepSystem.js` | FootstepSystem — Sonidos de pasos con detección de superficie. | constructor, update | Casa/Sandbox | Conectado | Falta | `A05` | `6c9a1ba21b3e` |
| `engine/FPSController.js` | FPSController — Controlador de primera persona completo. | constructor, update | Casa/Sandbox | Conectado | Existe | `S02` | `2e902d6d9671` |
| `engine/Furniture.js` | Furniture — Objeto interactuable simple para muebles y props del escenario. | constructor | Casa/Sandbox | Conectado | Parcial | `A02` | `1f6fa2f8b682` |
| `engine/i18n.js` | i18n — Sistema de internacionalización ligero para el motor. | constructor, load | Sandbox | Conectado | Falta | `U08` | `2308c74414f5` |
| `engine/Input.js` | Input — Gestor de entrada de teclado. | constructor, init | — | Código | Existe | `I01` | `fb8deccd7ee0` |
| `engine/InteractionSystem.js` | InteractionSystem — Detección de interacción FPS via Raycaster. | constructor, mount, unmount, update, destroy | Casa/Sandbox | Conectado | Parcial | `K01` | `c71f597fcb85` |
| `engine/LightPlacer.js` | LightPlacer — Herramienta visual de colocación de luces en el SceneEditor. | constructor, update, destroy | Casa | Conectado | Parcial | `H04` | `0cd3bdc2b724` |
| `engine/MirrorSystem.js` | MirrorSystem — Espejos en tiempo real usando WebGLRenderTarget. | constructor, update, destroy | Sandbox | Conectado | Falta | `V14` | `dd07812bfaef` |
| `engine/MusicDirector.js` | MusicDirector — Música adaptativa con stems y crossfade por tensión. | constructor, init, stop, pause | Casa/Sandbox | Conectado | Falta | `A06` | `18ff524b7e8e` |
| `engine/PhysicsWorld.js` | PhysicsWorld — Mundo de colisión basado en Octree de Three.js addons. | constructor | Casa/Sandbox | Conectado | Existe | `S01` | `a1c24158eb03` |
| `engine/PlayerVitals.js` | PlayerVitals — HP / O2 / Hunger / Sanity / Stamina tracking (REQ-v6-D02). | constructor, update | Sandbox | Conectado | Falta | `K08` | `14bc9f793ae6` |
| `engine/PostProcessor.js` | PostProcessor — Pipeline de post-procesado Three.js para La Casa. | constructor, init, render | Casa/Sandbox | Conectado | Parcial | `V05` | `e8937c47f645` |
| `engine/PropFactory.js` | ── Presets de material PSX ────────────────────────────────────────────── | — | Casa/Sandbox | Conectado | Falta | `H03` | `9264a08cb57e` |
| `engine/PropLoader.js` | PropLoader — Carga GLTF con fallback silencioso a geometría existente. | — | Casa/Sandbox | Conectado | Existe | `R02` | `a7bdfc150e2c` |
| `engine/PuzzleManager.js` | PuzzleManager — Máquina de estados central para puzzles. | constructor | Sandbox | Conectado | Falta | `K05` | `fa539e4e84dc` |
| `engine/Ragdoll.js` | Ragdoll — cannon-es physics ragdoll with 6 bodies (CC-12). | constructor, update, destroy | Sandbox | Conectado | Falta | `K11` | `e4aceceec059` |
| `engine/Renderer.js` | Renderer — Envuelve THREE.WebGLRenderer. | constructor, init | Casa/Sandbox | Conectado | Existe | `G01` | `528b9bdf01ff` |
| `engine/ReverbZones.js` | ReverbZones — Zonas de reverberación con Web Audio API. | constructor, init, update, destroy | Sandbox | Conectado | Falta | `A04` | `fffb96640b0d` |
| `engine/RoomBuilder.js` | RoomBuilder — Constructor de habitaciones visual integrado en el SceneEditor. | constructor, update, destroy | Casa | Conectado | Parcial | `H09` | `d8d1e636aa88` |
| `engine/SaveManager.js` | SaveManager — Persistencia de datos en localStorage. | constructor, save, load | Casa/Sandbox | Conectado | Parcial | `P01` | `c6ab6f9a074b` |
| `engine/SceneEditor.js` | SceneEditor — Modo editor activado via ?editor=1 en la URL. | constructor, mount, unmount, update | Casa | Conectado | Parcial | `H01` | `88391b1ef8a3` |
| `engine/SceneLoader.js` | SceneLoader — Carga props desde game/apartment/scene.json al renderer. | constructor, load | Casa | Conectado | Falta | `H08` | `7243e77722de` |
| `engine/SceneManager.js` | SceneManager — Gestiona el ciclo de vida de las escenas. | constructor, update | Casa | Conectado | Falta | `H08` | `78a744c71c6b` |
| `engine/ScriptEngine.js` | ScriptEngine — Secuenciador de pasos narrativos para cutscenes e historia. | constructor, stop | Casa/Sandbox | Conectado | Falta | `K03` | `c8d41a1d33d7` |
| `engine/shaders/FluidShader.js` | FluidShader — Fábrica de materiales animados para fluidos exóticos. | — | Sandbox | Conectado | Falta | `V06` | `9a91d4658401` |
| `engine/shaders/HeatDistortionShader.js` | HeatDistortionShader — ShaderPass de post-proceso para distorsión de calor. | — | Casa | Conectado | Parcial | `V05` | `0ed14f00644c` |
| `engine/shaders/HolographicShader.js` | HolographicShader — Scan-line + chromatic aberration + blue-cyan glow. | — | Sandbox | Conectado | Falta | `V06` | `290c8606b296` |
| `engine/shaders/HorrorFXShader.js` | HorrorFXShader — ShaderPass unificado para efectos atmosféricos de horror. | — | Casa/Sandbox | Conectado | Parcial | `V05` | `b199161246c1` |
| `engine/shaders/NeonShader.js` | NeonShader — Material GLSL para tubos de neón, letreros, luces de ambiente. | constructor, update, dispose | Casa/Sandbox | Conectado | Falta | `V06` | `5c3d9deb74d1` |
| `engine/shaders/OrganicShader.js` | OrganicShader — Material GLSL para superficies orgánicas que palpitan. | — | Casa/Sandbox | Conectado | Falta | `V06` | `5b8e2986cab1` |
| `engine/shaders/PS1Shader.js` | PS1Shader — Cuantización de color 5-bit + dithering Bayer 4×4. | — | Casa/Sandbox | Conectado | Parcial | `V05` | `bdd5d532858f` |
| `engine/shaders/PuddleShader.js` | PuddleShader — Material GLSL para superficies mojadas / charcos. | constructor, update, dispose | Casa/Sandbox | Conectado | Falta | `V06` | `e3b95b1dace4` |
| `engine/shaders/TVStaticShader.js` | TVStaticShader — Material GLSL para pantallas de TV/monitor. | constructor, update, dispose | Casa/Sandbox | Conectado | Falta | `V06` | `0e1dab7b07bc` |
| `engine/StateMachine.js` | StateMachine — Máquina de estados genérica y reactiva al EventBus. | constructor, stop, update | Sandbox | Conectado | Parcial | `K02` | `0e0c579b557f` |
| `engine/Theme.js` | Theme — Runtime theme switching for the horror engine. | — | Casa/Sandbox | Conectado | Falta | `U08` | `65e99de02267` |
| `engine/Timeline.js` | Timeline — Sistema de secuencias de acciones con keyframes temporales. | constructor, play, stop, pause, update | Sandbox | Conectado | Falta | `K04` | `b0e1a6645a0b` |
| `engine/TouchInput.js` | TouchInput — Control táctil para dispositivos móviles. | constructor, mount, destroy | Casa | Conectado | Falta | `I02` | `9cc8003455f9` |
| `engine/TriggerZone.js` | TriggerZone — Sistema de eventos espaciales por proximidad o volumen. | constructor, update | Casa/Sandbox | Conectado | Falta | `H07` | `2713194dac16` |
| `engine/TriggerZonePainter.js` | TriggerZonePainter — Herramienta visual de colocación de TriggerZones en el editor. | constructor | Casa | Conectado | Falta | `H07` | `64d4f2a3a982` |
| `engine/ui/ChoiceUI.js` | ChoiceUI — Overlay de opciones para bifurcaciones narrativas. | constructor, destroy | Casa/Sandbox | Conectado | Falta | `U02` | `fa1c60764b43` |
| `engine/ui/CodexUI.js` | CodexUI — Tabbed collectibles viewer that extends DocumentUI (REQ-v6-C03). | constructor | Sandbox | Conectado | Falta | `U05` | `41f1e5b6bf0a` |
| `engine/ui/CombinationLockUI.js` | CombinationLockUI — Tres tipos de cerradura para puzzles variados. | constructor | Sandbox | Conectado | Falta | `K05` | `051d9ebed9c6` |
| `engine/ui/ContextMenu.js` | ContextMenu — Menú contextual flotante HTML para interacciones point & click. | constructor, destroy | Casa/Sandbox | Conectado | Falta | `U02` | `50fdefa831c7` |
| `engine/ui/DialogManager.js` | DialogManager — Sistema de diálogos con efecto máquina de escribir. | constructor | Casa/Sandbox | Conectado | Falta | `U01` | `df5dc04e3158` |
| `engine/ui/DiegeticHUD.js` | DiegeticHUD — Canvas-texture plane attached to the camera rig. | constructor, mount, unmount, update | Sandbox | Conectado | Falta | `U06` | `02c75ec2f499` |
| `engine/ui/DocumentUI.js` | DocumentUI — Modal viewer for notes and audio logs (REQ-v6-C01, CC-07). | constructor | Sandbox | Conectado | Falta | `U05` | `f4f3196d09f8` |
| `engine/ui/HUD.js` | HUD — Actualiza los elementos DOM del HUD según eventos del bus. | constructor | Casa/Sandbox | Conectado | Falta | `U06` | `0a99eba10f34` |
| `engine/ui/InventoryUI.js` | InventoryUI — Panel de inventario DOM activado con la tecla I. | constructor, destroy | Casa/Sandbox | Conectado | Falta | `U03` | `7f74bbb1ab89` |
| `engine/ui/ItemInspectUI.js` | ItemInspectUI — Inspector 3D de objetos con rotación de arrastrar. | constructor, destroy | Sandbox | Conectado | Falta | `U04` | `9267eb70e2fc` |
| `engine/ui/ItemRevealUI.js` | this._visible = false; | constructor | Casa/Sandbox | Conectado | Falta | `U04` | `2802368d3fda` |
| `engine/ui/LoadingScreen.js` | LoadingScreen — Transition overlay with progress bar (REQ-v6-E02). | constructor | Sandbox | Conectado | Falta | `U09` | `6c6eddf6bf42` |
| `engine/ui/PauseMenu.js` | PauseMenu — Menú de pausa activado con Escape durante el juego. | constructor, destroy | Casa/Sandbox | Conectado | Falta | `U09` | `d02113e7dbb7` |
| `engine/ui/PhoneUI.js` | PhoneUI — Interfaz de teléfono simulado dentro del juego. | constructor, destroy | Casa/Sandbox | Conectado | Falta | `U07` | `b0f823a8c0ce` |
| `engine/ui/PuzzleHelpers.js` | PuzzleHelpers — Tres mini-puzzles listos para usar. | constructor, render | Sandbox | Conectado | Falta | `K05` | `420ff1e68aa0` |
| `engine/ui/ScreenManager.js` | ScreenManager — Gestiona la visibilidad de pantallas HTML y las transiciones de fade. | constructor | Casa | Conectado | Falta | `U09` | `1993cd90a8d8` |
| `engine/ui/SettingsMenu.js` | SettingsMenu — 4-tab settings panel (REQ-v6-C02, CC-07, CC-08). | constructor | Sandbox | Conectado | Falta | `U09` | `737724af5845` |
| `engine/ui/SubtitleUI.js` | SubtitleUI — Subtítulos estilo película en la franja inferior. | constructor, mount, unmount | Casa/Sandbox | Conectado | Falta | `U01` | `c318734fa007` |
| `engine/ui/TerminalUI.js` | TerminalUI — Ordenador tipo terminal CRT para puzzles. | constructor, destroy | Sandbox | Conectado | Falta | `K05` | `ff7d493bd1b1` |
| `engine/UIFocusManager.js` | UIFocusManager - centralizes DOM UI focus and pointer-lock transitions. | constructor, destroy | Casa/Sandbox | Conectado | Falta | `U09` | `51eda6d978d8` |
| `engine/VideoPlayer.js` | VideoPlayer — Reproduce video HTML5 en texturas Three.js sobre mallas de la escena. | constructor, stop, destroy | Sandbox | Conectado | Falta | `V16` | `ced734245302` |
| `engine/WallHoleTool.js` | WallHoleTool — Herramienta para añadir huecos (puertas/ventanas) en paredes. | constructor | Casa | Conectado | Falta | `H05` | `7f895bd4341a` |
| `engine/Weapon.js` | Weapon — Raycast-based weapon system with ammo FSM (REQ-v6-D03). | constructor, mount, unmount, update | Sandbox | Conectado | Falta | `K10` | `a7c5b91d2e91` |

## 18 archivos de juegos y demos

| Fuente | Papel | Ruta estática | Evidencia | Ticket | SHA-256 |
|---|---|---|---|---|---|
| `game/apartment/ApartmentLevel.js` | Construcción de la escena del apartamento, puertas y habitaciones | Casa | Conectado | `X01` | `3bd158bc26b1` |
| `game/apartment/Day1Script.js` | Day1Script — Secuencia narrativa completa del Dia 1. | Casa | Conectado | `K03` | `834606c4733d` |
| `game/apartment/effects.js` | buildApartmentEffects — integra todas las features nuevas del motor en el apartamento. | Casa | Conectado | `X01` | `a599faee1825` |
| `game/apartment/entities/Corpse.js` | Corpse — Cadaver de "La Casa" con 5 estados visuales y narrativos. | Casa | Conectado | `X01` | `10b47f877dba` |
| `game/apartment/entities/Door.js` | Door — Puerta interactuable con animación suave de apertura/cierre. | Casa | Conectado | `K01` | `2a1925f958f2` |
| `game/apartment/GameEvents.js` | GameEvents — Registra los handlers de eventos de alto nivel del juego. | Casa | Conectado | `K01` | `7aa510da11ac` |
| `game/apartment/Interactables.js` | buildInteractables — Orquestador de props interactuables del apartamento. | Casa | Conectado | `K01` | `1c84fee22fb8` |
| `game/apartment/LaCasaState.js` | LaCasaState — Estado global del juego "La Casa". | Casa | Conectado | `K02` | `a879ad54cef8` |
| `game/apartment/lighting.js` | lighting.js — Iluminación del apartamento. | Casa | Conectado | `H04` | `1590d860294e` |
| `game/apartment/rooms/bano.js` | Composición de props e interacciones de la habitación bano | Casa | Conectado | `X01` | `2a087f4fc750` |
| `game/apartment/rooms/cocina.js` | Composición de props e interacciones de la habitación cocina | Casa | Conectado | `X01` | `5df9d86899ae` |
| `game/apartment/rooms/dormitorio.js` | Composición de props e interacciones de la habitación dormitorio | Casa | Conectado | `X01` | `10beb30021b1` |
| `game/apartment/rooms/living.js` | Composición de props e interacciones de la habitación living | Casa | Conectado | `X01` | `cb73ca418076` |
| `game/apartment/rooms/patio.js` | Composición de props e interacciones de la habitación patio | Casa | Conectado | `X01` | `cdc205ef98a3` |
| `game/apartment/textures.js` | textures.js — Texturas procedurales del apartamento. | Casa | Conectado | `V06` | `5fd741eb79ff` |
| `game/main.js` | main.js — Punto de entrada del juego La Casa. | Casa | Conectado | `X01` | `980a1ac71765` |
| `game/sandbox/main.js` | game/sandbox/main.js — Bootstrap mínimo para el nivel sandbox. | Sandbox | Conectado | `X01` | `19ca12988253` |
| `game/sandbox/SandboxLevel.js` | SandboxLevel — Showcase maxima del Motor de Horror. | Sandbox | Conectado | `X01` | `a498fa39d200` |

## 63 documentos de planificación

| Documento | Tipo documental | Tema inicial | Evidencia | Ticket o decisión | SHA-256 |
|---|---|---|---|---|---|
| `.planning/config.json` | contexto/QA | config | Sólo plan | `ZA1` | `4f4ff7aca630` |
| `.planning/continue.md` | contexto/QA | Handoff — La Casa / PSX Horror Engine (2026-05-06) | Sólo plan | `ZA1` | `6733886540a9` |
| `.planning/debug/sandbox-black-screen.md` | contexto/QA | sandbox-black-screen | Sólo plan | `X01` | `5ce28a4c1266` |
| `.planning/phases/01-html-renderer-base/01-01-PLAN.md` | plan | 01-01-PLAN | Sólo plan | `G01` | `0c30bfb23c39` |
| `.planning/phases/01-html-renderer-base/01-01-SUMMARY.md` | resumen histórico | Phase 1 — Plan 01-01 Summary: HTML importmap + WebGLRenderer | Sólo plan | `G01` | `fd3e93047fad` |
| `.planning/phases/01-html-renderer-base/01-02-PLAN.md` | plan | 01-02-PLAN | Sólo plan | `G01` | `a2477e754bc2` |
| `.planning/phases/01-html-renderer-base/01-02-SUMMARY.md` | resumen histórico | Phase 1 — Plan 01-02 Summary: game/main.js Three.js Bootstrap | Sólo plan | `G01` | `369f3e60d6b5` |
| `.planning/phases/02-camera-fps-controller/02-01-PLAN.md` | plan | PhysicsWorld: Octree presente, Matter ausente | Sólo plan | `S02` | `ca652b86299b` |
| `.planning/phases/02-camera-fps-controller/02-02-PLAN.md` | plan | 02-02-PLAN | Sólo plan | `S02` | `b74ae1888e44` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-01-PLAN.md` | plan | 05.6-01-PLAN | Sólo plan | `H03` | `b3a5c585bd35` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-01-SUMMARY.md` | resumen histórico | Phase 5.6 Plan 01: PropFactory v3 Summary | Sólo plan | `H03` | `4e4f32c0b212` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-02-PLAN.md` | plan | 05.6-02-PLAN | Sólo plan | `H03` | `b0ab97fdd22b` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-02-SUMMARY.md` | resumen histórico | Phase 5.6 Plan 02: Textures Summary | Sólo plan | `H03` | `218de44d5218` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-03-PLAN.md` | plan | 05.6-03-PLAN | Sólo plan | `H03` | `e9ffdcf0918b` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-03-SUMMARY.md` | resumen histórico | Phase 5.6 Plan 03: Dormitorio Summary | Sólo plan | `H03` | `28641e5cee71` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-04-PLAN.md` | plan | 05.6-04-PLAN | Sólo plan | `H03` | `daec6059dd5b` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-04-SUMMARY.md` | resumen histórico | Phase 5.6 Plan 04: Baño Summary | Sólo plan | `H03` | `41986ae399be` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-05-PLAN.md` | plan | 05.6-05-PLAN | Sólo plan | `H03` | `d637ad8bfbb6` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-05-SUMMARY.md` | resumen histórico | Phase 5.6 Plan 05: Living Summary | Sólo plan | `H03` | `88039bef5ca9` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-06-PLAN.md` | plan | 05.6-06-PLAN | Sólo plan | `H03` | `2c45af3d0ea8` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-06-SUMMARY.md` | resumen histórico | Phase 5.6 Plan 06: Cocina Summary | Sólo plan | `H03` | `2d0bee9b8143` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-07-PLAN.md` | plan | 05.6-07-PLAN | Sólo plan | `H03` | `2e42a4a6674c` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-07-SUMMARY.md` | resumen histórico | Phase 5.6 Plan 07: Patio Summary | Sólo plan | `H03` | `1048962d213d` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-08-PLAN.md` | plan | 05.6-08-PLAN | Sólo plan | `H03` | `bcae9cc27730` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-CONTEXT.md` | contexto/QA | Phase 5.6: PropFactory v3 + Modelos detallados — Context | Sólo plan | `H03` | `9a24447b76c3` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-PROPFACTORY-NEXT.md` | contexto/QA | PropFactory Next Notes | Sólo plan | `H03` | `ed0e10c36946` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-RESEARCH.md` | requisito/idea | Phase 5.6 Research | Sólo plan | `H03` | `e4b7087d3833` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-VALIDATION.md` | contexto/QA | Phase 5.6 — Validation Strategy | Sólo plan | `H03` | `75d9a34c098d` |
| `.planning/phases/05.6-propfactory-v3-modelos-detallados/05.6-VISUAL-UAT.md` | contexto/QA | Phase 5.6 Visual UAT | Sólo plan | `H03` | `5cbbeb639bf4` |
| `.planning/phases/06-core-interactables/06-01-PLAN.md` | plan | 06-01-PLAN | Sólo plan | `K01` | `229c7df20827` |
| `.planning/phases/06-core-interactables/06-CONTEXT.md` | contexto/QA | Phase 6: Core Interactables - Context | Sólo plan | `K01` | `0f457a358619` |
| `.planning/phases/13.8-animation-system/13.8-01-PLAN.md` | plan | Plan 13.8-01 — Animation System | Sólo plan | `A02` | `df4a4c8a196c` |
| `.planning/phases/13.9-sceneeditor-pro/13.9-01-PLAN.md` | plan | Plan 13.9-01 — SceneEditor Pro | Sólo plan | `H01` | `2b26b43cab2a` |
| `.planning/phases/13.95-engine-observability/CONTEXT.md` | contexto/QA | Phase 13.95 — Engine Observability: Logs + Error Catching | Sólo plan | `Q01` | `a3b226c2886c` |
| `.planning/phases/13.95-engine-observability/PLAN.md` | plan | Phase 13.95 — Engine Observability: Logs + Error Catching | Sólo plan | `Q01` | `654d049d3d82` |
| `.planning/phases/13.95-engine-observability/VERIFICATION.md` | contexto/QA | Phase 13.95 — VERIFICATION | Sólo plan | `Q01` | `6d65903a24c8` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/.gitkeep` | contexto/QA | .gitkeep | Sólo plan | `X01` | `e3b0c44298fc` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-01-PLAN.md` | plan | 36-01-PLAN | Sólo plan | `X01` | `f0643de2509e` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-01-SUMMARY.md` | resumen histórico | Plan 01 Summary — QA Checklist Creation | Sólo plan | `X01` | `f7500e84c207` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-02-PLAN.md` | plan | 36-02-PLAN | Sólo plan | `X01` | `cd0bcee259c9` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-02-SUMMARY.md` | resumen histórico | Plan 02 Summary — UIFocusManager + Input Routing | Sólo plan | `X01` | `7cc6bbd74b59` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-03-PLAN.md` | plan | 36-03-PLAN | Sólo plan | `X01` | `664e51ed2793` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-04-PLAN.md` | plan | 36-04-PLAN | Sólo plan | `X01` | `2a5cad852518` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-04-SUMMARY.md` | resumen histórico | Plan 04 Summary — WaterSystem + VegetationSystem Hardening | Sólo plan | `X01` | `52f9f8071884` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-05-PLAN.md` | plan | 36-05-PLAN | Sólo plan | `X01` | `4a2d6448e453` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-06-PLAN.md` | plan | 36-06-PLAN | Sólo plan | `X01` | `a50581c59114` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-CONTEXT.md` | contexto/QA | Phase 36: Engine showcase guided QA and FPS horror systems polish - Context | Sólo plan | `X01` | `642627d75944` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-QA-CHECKLIST.md` | contexto/QA | Phase 36 QA Checklist — Engine Showcase & FPS Horror Systems Polish | Sólo plan | `X01` | `02ac515b714b` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-RESEARCH.md` | requisito/idea | Phase 36: Engine showcase guided QA and FPS horror systems polish - Research | Sólo plan | `X01` | `28192d4e844b` |
| `.planning/phases/36-engine-showcase-guided-qa-and-fps-horror-systems-polish/36-VALIDATION.md` | contexto/QA | Phase 36 Validation Strategy | Sólo plan | `X01` | `6a41c2aaf13b` |
| `.planning/phases/phase-39/color-audit.md` | contexto/QA | Phase 39 — Color Audit | Sólo plan | `H04` | `0b5e3a2b5edc` |
| `.planning/PROJECT.md` | contexto/QA | La Casa — Multi-Genre PSX Horror Engine | Sólo plan | `ZA1` | `17b96cdaa884` |
| `.planning/quick/260505-qrp-creo-que-hace-falta-trabajar-las-luces-a/260505-qrp-PLAN.md` | plan | 260505-qrp-PLAN | Sólo plan | `H04` | `643774d5e49a` |
| `.planning/quick/260505-qrp-creo-que-hace-falta-trabajar-las-luces-a/260505-qrp-SUMMARY.md` | resumen histórico | Quick Task 260505-qrp Summary | Sólo plan | `H04` | `92e65ccb587a` |
| `.planning/requirements/v6.0-REQUIREMENTS.md` | requisito/idea | Requirements — Milestone v6.0: Multi-Genre Horror Toolkit | Sólo plan | `ZA1` | `bda5444b1b67` |
| `.planning/research/ARCHITECTURE.md` | contexto/QA | Architecture Research | Sólo plan | `ZA1` | `fa7beec088f4` |
| `.planning/research/FEATURES.md` | requisito/idea | Feature Research: v6.0 Multi-Genre Horror Toolkit | Sólo plan | `ZA1` | `78927677a8b4` |
| `.planning/research/PITFALLS.md` | contexto/QA | Pitfalls Research | Sólo plan | `ZA1` | `236bf504ad50` |
| `.planning/research/STACK-RESEARCH.md` | requisito/idea | Stack Research: First-Person Horror Game — Web 2026 | Sólo plan | `ZA1` | `6f84cdf60236` |
| `.planning/research/STACK.md` | contexto/QA | Stack Research — v6.0 Multi-Genre Horror Toolkit | Sólo plan | `ZA1` | `98d7f151ec95` |
| `.planning/research/SUMMARY.md` | resumen histórico | Research Summary — v6.0 Multi-Genre Horror Toolkit | Sólo plan | `ZA1` | `7d13cd69ceaa` |
| `.planning/ROADMAP.md` | plan | Roadmap: La Casa | Sólo plan | `ZA1` | `22ee6f0eb7b3` |
| `.planning/STATE.md` | contexto/QA | Project State | Sólo plan | `ZA1` | `084c108da96f` |

## 105 modelos GLB

`En manifest` indica presencia en el catálogo JSON, no uso en una escena. `Referencia literal` busca ruta, nombre o clave del modelo en JS/JSON de motor y juego, excluyendo el manifest; una ruta construida dinámicamente puede no aparecer. Fuente, autor, licencia y permiso de redistribución siguen sin prueba por archivo. Ningún modelo se copia a VESTIGIO en esta etapa.

| GLB | Grupo | KiB | En manifest | Referencia literal | Procedencia | Ticket | SHA-256 |
|---|---|---:|---|---|---|---|---|
| `assets/models/cars/psx_low-poly_camper_van.glb` | cars | 51 | Sí | — | Sin verificar | `X02` | `cafe32828ddb` |
| `assets/models/characters/characters_psx.glb` | characters | 22285 | Sí | — | Sin verificar | `X02` | `1f407972e00d` |
| `assets/models/characters/giant_cockroach.glb` | characters | 533 | Sí | — | Sin verificar | `X02` | `b36decdfd285` |
| `assets/models/characters/lowpoly_male_base_mesh.glb` | characters | 255 | Sí | — | Sin verificar | `X02` | `a78c4bf38075` |
| `assets/models/doors and gates/gate.glb` | doors and gates | 2622 | Sí | — | Sin verificar | `X02` | `e478cc93400c` |
| `assets/models/doors and gates/low-poly_psx_style_essential_doors_pack.glb` | doors and gates | 457 | Sí | — | Sin verificar | `X02` | `5cc2b14b5b03` |
| `assets/models/doors and gates/low-poly_psx_style_front_doors_pack.glb` | doors and gates | 810 | Sí | — | Sin verificar | `X02` | `674f8ff49605` |
| `assets/models/doors and gates/low-poly_psx_style_industrial_metal_doors_pack.glb` | doors and gates | 540 | Sí | — | Sin verificar | `X02` | `bd6cdbc17833` |
| `assets/models/doors and gates/low-poly_psx_style_wooden_interior_doors_pack.glb` | doors and gates | 536 | Sí | — | Sin verificar | `X02` | `1d679dfadb8b` |
| `assets/models/doors and gates/low-poly_psx_style_worn_wooden_doors_pack.glb` | doors and gates | 381 | Sí | — | Sin verificar | `X02` | `0c71d08f201a` |
| `assets/models/doors and gates/psx_style_chain_fence_gate.glb` | doors and gates | 143 | Sí | — | Sin verificar | `X02` | `679b08fc937f` |
| `assets/models/food/foodpsx.glb` | food | 1024 | Sí | — | Sin verificar | `X02` | `84c8dbac152e` |
| `assets/models/food/low_poly_beer_bottle_-_psx_style.glb` | food | 23 | Sí | — | Sin verificar | `X02` | `03ebb1c16025` |
| `assets/models/food/low_poly_dirty_plastic_bottle_-_game_ready.glb` | food | 5689 | Sí | — | Sin verificar | `X02` | `2c1c59ba7cdf` |
| `assets/models/food/plate_shrimps.glb` | food | 816 | Sí | — | Sin verificar | `X02` | `421a22d8cce1` |
| `assets/models/food/psx_diet_coke_3d_sodas_model.glb` | food | 25 | Sí | — | Sin verificar | `X02` | `f454af5bc7de` |
| `assets/models/food/psx_low-poly_soup.glb` | food | 65 | Sí | — | Sin verificar | `X02` | `2573b6e65e1d` |
| `assets/models/food/psx_meal.glb` | food | 5304 | Sí | — | Sin verificar | `X02` | `7e2c35be9880` |
| `assets/models/food/psx_pepsi_3d_sodas_model.glb` | food | 55 | Sí | — | Sin verificar | `X02` | `15235daf723c` |
| `assets/models/food/psx_raw_meat_pack__low-poly_2_types.glb` | food | 327 | Sí | — | Sin verificar | `X02` | `61c92bef2803` |
| `assets/models/food/psx_soda_can.glb` | food | 124 | Sí | — | Sin verificar | `X02` | `2b9dfa08ac36` |
| `assets/models/furniture/ac_outdoor_unit_-_low_poly.glb` | furniture | 1845 | Sí | — | Sin verificar | `X02` | `ac103e36af88` |
| `assets/models/furniture/book_shelf.glb` | furniture | 3662 | Sí | — | Sin verificar | `X02` | `9c8429f2bf5a` |
| `assets/models/furniture/cardboard_box_psx.glb` | furniture | 30 | Sí | — | Sin verificar | `X02` | `71307f674e9e` |
| `assets/models/furniture/free_car_tire_set_-_ps2_style_urban_props.glb` | furniture | 93 | Sí | — | Sin verificar | `X02` | `8290c6782264` |
| `assets/models/furniture/gameready_psx_style_vcr__vhs_set.glb` | furniture | 2375 | Sí | — | Sin verificar | `X02` | `b55c6c985019` |
| `assets/models/furniture/low-poly_psx_style_park_benches_vol.1.glb` | furniture | 818 | Sí | — | Sin verificar | `X02` | `663c9d00455c` |
| `assets/models/furniture/low-poly_psx_style_vintage_rugs_pack.glb` | furniture | 1117 | Sí | — | Sin verificar | `X02` | `5ae5c2f2f1eb` |
| `assets/models/furniture/low-poly_psx_style_wardrobe_with_clothes.glb` | furniture | 2577 | Sí | — | Sin verificar | `X02` | `19602ff85ef2` |
| `assets/models/furniture/low_poly_psx_bicycle.glb` | furniture | 457 | Sí | — | Sin verificar | `X02` | `33848369e211` |
| `assets/models/furniture/low_poly_psx_street_lamp.glb` | furniture | 189 | Sí | — | Sin verificar | `X02` | `3bd7ea9cacaf` |
| `assets/models/furniture/low_poly_psx_wall_lamp_with_canopy.glb` | furniture | 238 | Sí | — | Sin verificar | `X02` | `48a5b9799958` |
| `assets/models/furniture/low_poly_psxps2_trash_filled_metal_dumpster.glb` | furniture | 321 | Sí | — | Sin verificar | `X02` | `2b88e4b50247` |
| `assets/models/furniture/low_polygons_shihos_bass.glb` | furniture | 300 | Sí | — | Sin verificar | `X02` | `bbaadb8ee70b` |
| `assets/models/furniture/lowpoly_psx_old_computer.glb` | furniture | 161 | Sí | — | Sin verificar | `X02` | `1e9556232948` |
| `assets/models/furniture/old_couch.glb` | furniture | 3716 | Sí | — | Sin verificar | `X02` | `07a7807f0fb5` |
| `assets/models/furniture/old_media_props_pack__game_ready_hq_assets.glb` | furniture | 28957 | Sí | — | Sin verificar | `X02` | `0e78885b4d3e` |
| `assets/models/furniture/old_soviet_stove.glb` | furniture | 2816 | Sí | — | Sin verificar | `X02` | `91c0790c5c5c` |
| `assets/models/furniture/playstation_1.glb` | furniture | 1105 | Sí | — | Sin verificar | `X02` | `4ff07a59eeb9` |
| `assets/models/furniture/ps1_style_lamp.glb` | furniture | 33 | Sí | — | Sin verificar | `X02` | `845e7bb2f89f` |
| `assets/models/furniture/ps2_books.glb` | furniture | 2283 | Sí | — | Sin verificar | `X02` | `459a62ee6de1` |
| `assets/models/furniture/ps2_bookshelf.glb` | furniture | 1130 | Sí | — | Sin verificar | `X02` | `c45e34855936` |
| `assets/models/furniture/ps2_style_rusty_trash_bin.glb` | furniture | 1368 | Sí | — | Sin verificar | `X02` | `43e30f08bce3` |
| `assets/models/furniture/ps2_table_lamp.glb` | furniture | 106 | Sí | — | Sin verificar | `X02` | `0f21c240ebe0` |
| `assets/models/furniture/psx-style_vintage_wall_calendars.glb` | furniture | 171 | Sí | — | Sin verificar | `X02` | `30a30a070491` |
| `assets/models/furniture/psx-style_vintage_wall_clocks.glb` | furniture | 352 | Sí | — | Sin verificar | `X02` | `8a2ae4fcb3da` |
| `assets/models/furniture/psx_-_pack.glb` | furniture | 259 | Sí | — | Sin verificar | `X02` | `4fc9f61eb51d` |
| `assets/models/furniture/psx_electric_pole.glb` | furniture | 115 | Sí | — | Sin verificar | `X02` | `4d7e80f7ae0f` |
| `assets/models/furniture/psx_fire_extinguisher.glb` | furniture | 78 | Sí | — | Sin verificar | `X02` | `82e2440766b4` |
| `assets/models/furniture/psx_low-poly_televisions.glb` | furniture | 49 | Sí | — | Sin verificar | `X02` | `ddc7cc95311e` |
| `assets/models/furniture/psx_stockpot.glb` | furniture | 94 | Sí | — | Sin verificar | `X02` | `8a22f6b2377b` |
| `assets/models/furniture/psx_style_barrel.glb` | furniture | 26 | Sí | — | Sin verificar | `X02` | `d0fdb26e04e6` |
| `assets/models/furniture/psx_style_satellite_radio.glb` | furniture | 128 | Sí | — | Sin verificar | `X02` | `75cd1f0012c1` |
| `assets/models/furniture/psx_style_wardrobe.glb` | furniture | 131 | Sí | — | Sin verificar | `X02` | `6664f6c6f392` |
| `assets/models/furniture/psx_toilet.glb` | furniture | 21 | Sí | — | Sin verificar | `X02` | `982e0083f805` |
| `assets/models/furniture/psx_trash_can.glb` | furniture | 84 | Sí | — | Sin verificar | `X02` | `c1bb2e8fcb62` |
| `assets/models/furniture/psx_vhs_player__tape.glb` | furniture | 69 | Sí | — | Sin verificar | `X02` | `347ee0ab396b` |
| `assets/models/furniture/psx_washing_machine.glb` | furniture | 211 | Sí | — | Sin verificar | `X02` | `5ef4f261e4e8` |
| `assets/models/furniture/psx_win95_pc._90s.glb` | furniture | 170 | Sí | — | Sin verificar | `X02` | `f97911758b2c` |
| `assets/models/furniture/psx_wooden_box.glb` | furniture | 45 | Sí | — | Sin verificar | `X02` | `36c2d657955a` |
| `assets/models/furniture/retro__psx-like_spooky_house_pieces.glb` | furniture | 7191 | Sí | — | Sin verificar | `X02` | `f46fa066d5b1` |
| `assets/models/furniture/retro_lowpoly_lamp.glb` | furniture | 37 | Sí | — | Sin verificar | `X02` | `c08fb065a8bc` |
| `assets/models/furniture/retro_lowpoly_toilet_ps1ps2_style.glb` | furniture | 105 | Sí | — | Sin verificar | `X02` | `951979aa117a` |
| `assets/models/furniture/trash_can_lowpoly_psx_style.glb` | furniture | 861 | Sí | — | Sin verificar | `X02` | `5d75a2beb05a` |
| `assets/models/furniture/trashy_backyard_sofa.glb` | furniture | 7424 | Sí | — | Sin verificar | `X02` | `5f33eb1c8169` |
| `assets/models/furniture/vintage_canadian_books_18_512x512_psx_retro.glb` | furniture | 1069 | Sí | — | Sin verificar | `X02` | `9be72e9eba5a` |
| `assets/models/furniture/wooden_book_shelf__low-poly__game-ready.glb` | furniture | 8157 | Sí | — | Sin verificar | `X02` | `fb03b9d0089a` |
| `assets/models/furniture/yamaha_cs01_keyboard.glb` | furniture | 1119 | Sí | — | Sin verificar | `X02` | `2044044d3488` |
| `assets/models/hands/psx_first_person_arms.glb` | hands | 1061 | Sí | — | Sin verificar | `X02` | `2b2fcdfd780e` |
| `assets/models/objects/320gb_sata_iii_35_hdd.glb` | objects | 90 | Sí | — | Sin verificar | `X02` | `6e7ecd6b928a` |
| `assets/models/objects/4gb_ddr3_1600mhz_ram_stick.glb` | objects | 21 | Sí | — | Sin verificar | `X02` | `b2056ff44023` |
| `assets/models/objects/black_and_white_floppy_disk.glb` | objects | 30 | Sí | — | Sin verificar | `X02` | `6537b45a01a5` |
| `assets/models/objects/cassette_tape_100x64x0_8.glb` | objects | 1590 | Sí | — | Sin verificar | `X02` | `71bab5f6c82d` |
| `assets/models/objects/disc_licensing_lowpoly_psx.glb` | objects | 1063 | Sí | — | Sin verificar | `X02` | `3f88bda27ec1` |
| `assets/models/objects/geforce_256_gpu.glb` | objects | 151 | Sí | — | Sin verificar | `X02` | `44189e7d59cc` |
| `assets/models/objects/geforce_gt710_gpu.glb` | objects | 123 | Sí | — | Sin verificar | `X02` | `4c3df54135a4` |
| `assets/models/objects/low_poly_psx_bolt_cutters.glb` | objects | 78 | Sí | — | Sin verificar | `X02` | `aabe558a676a` |
| `assets/models/objects/low_poly_radio.glb` | objects | 1458 | Sí | — | Sin verificar | `X02` | `c9fcb91e7525` |
| `assets/models/objects/motherboard.glb` | objects | 77 | Sí | — | Sin verificar | `X02` | `2a12fec0cf8d` |
| `assets/models/objects/music_cassette.glb` | objects | 153 | Sí | — | Sin verificar | `X02` | `25b0ade73f66` |
| `assets/models/objects/nvidia_gf210_low_poly_psx.glb` | objects | 1424 | Sí | — | Sin verificar | `X02` | `c7b8671034c5` |
| `assets/models/objects/old_tv_model_download.glb` | objects | 2191 | Sí | — | Sin verificar | `X02` | `bf070869bcf0` |
| `assets/models/objects/ps1__psx_cd.glb` | objects | 2468 | Sí | — | Sin verificar | `X02` | `ad4e53642af2` |
| `assets/models/objects/psu.glb` | objects | 113 | Sí | — | Sin verificar | `X02` | `660790def9fa` |
| `assets/models/objects/psx_nokia.glb` | objects | 71 | Sí | — | Sin verificar | `X02` | `0d645218f825` |
| `assets/models/objects/psx_rusted_knife.glb` | objects | 33 | Sí | — | Sin verificar | `X02` | `8aa4e1da2238` |
| `assets/models/objects/psx_style_cassette_tape.glb` | objects | 236 | Sí | — | Sin verificar | `X02` | `b1bf338ef0ae` |
| `assets/models/objects/psx_style_digital_watch.glb` | objects | 48 | Sí | — | Sin verificar | `X02` | `df8ae6e33998` |
| `assets/models/objects/psx_style_smoking_pack.glb` | objects | 79 | Sí | — | Sin verificar | `X02` | `7d1d0bf61738` |
| `assets/models/objects/psx_style_sony_walkman.glb` | objects | 81 | Sí | — | Sin verificar | `X02` | `3bdd76b01527` |
| `assets/models/objects/psx_style_walkie_talkie.glb` | objects | 25 | Sí | — | Sin verificar | `X02` | `48326f58e612` |
| `assets/models/objects/psx_survival_horror_healing_assets.glb` | objects | 241 | Sí | — | Sin verificar | `X02` | `b9a924d8881c` |
| `assets/models/objects/psx_vhs.glb` | objects | 32 | Sí | — | Sin verificar | `X02` | `fb3f0eb781c5` |
| `assets/models/objects/retro__psx-like_horror_melee_weapons_pack.glb` | objects | 596 | Sí | — | Sin verificar | `X02` | `d6058ad45a15` |
| `assets/models/objects/retropsx_analog_multimeter.glb` | objects | 77 | Sí | — | Sin verificar | `X02` | `45e039d7b954` |
| `assets/models/plants and trees/fantasy_tree_1.glb` | plants and trees | 8979 | Sí | — | Sin verificar | `X02` | `3455dfa36d7c` |
| `assets/models/plants and trees/geranium_flowering_plants_free.glb` | plants and trees | 6119 | Sí | — | Sin verificar | `X02` | `9f7314c17f04` |
| `assets/models/plants and trees/hill_top_tree.glb` | plants and trees | 4779 | Sí | — | Sin verificar | `X02` | `594486cada9e` |
| `assets/models/plants and trees/low_poly_rock_pack.glb` | plants and trees | 5216 | Sí | — | Sin verificar | `X02` | `24df4fe173e5` |
| `assets/models/plants and trees/lowpoly_flower_bushes.glb` | plants and trees | 3047 | Sí | — | Sin verificar | `X02` | `7495e57770c7` |
| `assets/models/plants and trees/lowpoly_tree.glb` | plants and trees | 1700 | Sí | — | Sin verificar | `X02` | `46ae11f15a0b` |
| `assets/models/plants and trees/psx_style_bush.glb` | plants and trees | 41 | Sí | — | Sin verificar | `X02` | `e2b4ac4e2694` |
| `assets/models/plants and trees/psx_style_house_plants.glb` | plants and trees | 182 | Sí | — | Sin verificar | `X02` | `59210e5345c5` |
| `assets/models/plants and trees/tree.glb` | plants and trees | 8595 | Sí | — | Sin verificar | `X02` | `5e940f70ca8a` |
| `assets/models/plants and trees/trees.glb` | plants and trees | 7987 | Sí | — | Sin verificar | `X02` | `7bc7299e6d60` |

Los índices `assets/models/doors/index.json` y `assets/models/windows/index.json` contienen `[]`; los GLB de `doors and gates/` son archivos presentes, no una colección de puertas o ventanas especializada y operativa.
