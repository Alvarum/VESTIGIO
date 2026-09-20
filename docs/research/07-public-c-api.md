# 07 — Public C API / SDK y Game API

Propuesta de contrato, **ninguno de los símbolos `vg_*` de este documento está implementado**. El prefijo es provisional; no obliga a renombrar internals `re_*`. Evidencia de partida: `include/retro/editor.h` ya tiene documento opaco; `session.h` tiene sesión opaca pero recibe `ReProject`; `gameplay.h` expone pools/punteros. Se necesita una frontera pública nueva, no declarar públicas todas las cabeceras actuales.

## Distribución evaluada

| Alternativa | Ventajas | Costes | Decisión |
|---|---|---|---|
| Librería estática | Debug sencillo, sin búsqueda de DLL, optimización y empaquetado predecibles | Recompilar/relink; toolchain compatible | **SDK inicial** con target CMake importado |
| Librería dinámica | Binarios pequeños de consumidores, frontera real útil para C# | Exports, ABI, runtime C, búsqueda de dependencias | DLL de tooling/Studio; SDK dinámico después del contrato estático |
| Engine + game module estático | Separa comportamiento de host, prueba dos juegos con mismo motor | Requiere callbacks/lifecycle | **Elegido**, game module puede ser funciones compiladas con host |
| Game DLL/shared library | Iteración sin relink del host | Estado entre versiones, punteros/callbacks vivos, locks y código arbitrario | Opcional posterior, sin hot reload inicial |
| Callbacks | Inversión de control pequeña; no requiere VM | Reentrancia/orden/errores necesitan contrato | `init`, `world_ready`, `fixed_update`, `event`, `draw_ui`, `shutdown` |
| Plugin API genérica | Terceros pueden ampliar herramientas | Muy fácil comprometer estabilidad con registry universal | Primero registro limitado de tipos/acciones de juego |
| Structs públicos del engine | Acceso directo y rendimiento | ABI/layout/ownership acoplados, corrupción fácil | No exponer mundo/asset internals |
| Handles opacos | Lifetime validable, bindings sencillos | Lookup y errores deben manejarse | Para objetos con vida propia |
| Híbrida | Handles para identidad, POD para descripciones/copias | Algo más de código que punteros directos | **Elegida** |

`vestigio_runtime` se instala con headers, biblioteca, `VestigioConfig.cmake`, versión y licencias. El usuario debe poder hacer `find_package(Vestigio CONFIG REQUIRED)` y enlazar `Vestigio::Runtime` sin incluir carpetas del repo, abrir Studio ni instalar .NET. El header público debe ser consumible desde C11 y C++ con `extern "C"`; internals pueden seguir C23. No introducir `nullptr`, atributos C23 o `math.h` interno en esa frontera.

## Tipos y reglas de ABI

Diseño ilustrativo:

```c
typedef struct VgContext VgContext;
typedef struct { uint64_t value; } VgWorld;
typedef struct { uint64_t value; } VgEntity;
typedef struct { uint64_t value; } VgAsset;
typedef struct { uint64_t value; } VgVoice;
typedef int32_t VgResult;
typedef struct { float x, y, z; } VgVec3;
typedef struct { float x, y, z, w; } VgQuat;
typedef struct { VgVec3 position; VgQuat rotation; VgVec3 scale; } VgTransform;
typedef struct {
    uint32_t struct_size;
    uint32_t api_version;
    uint32_t backend;
    uint32_t flags;
    void *user;
    void (*log)(void *user, uint32_t severity, const char *utf8);
} VgContextDesc;
```

ABI fija por plataforma/arquitectura soportada, inicialmente Windows x64. No prometer que structs por valor sean intercambiables entre arquitecturas; en DLL los descriptores cruzan por puntero const y las salidas por buffer del llamador. Convención de llamada y `VG_API` explícitas, exports mínimos. Usar enteros de ancho fijo para enums/flags/bools de ABI y longitudes; no exponer `size_t`, `FILE*`, `Texture2D`, `Model`, HWND ni punteros a pools internos.

Cada descriptor tiene `struct_size`; API tiene major/minor. Compatibilidad minor: añadir campos al final con defaults documentados, respetar tamaño recibido y capacidad de salida. Major incompatible se rechaza antes de crear recursos. Tamaño de estructura **no sustituye** versionado semántico ni metadatos del backend. Cero representa handle inválido; valores no persistibles, sólo válidos en contexto/mundo/tipo/generación de origen.

## Semántica pública

| Área / funciones propuestas | Contrato esencial | Necesidad y antecedente |
|---|---|---|
| `vg_context_create/destroy`, `vg_get_version`, `vg_get_capabilities` | Contexto creado antes de recursos; destroy termina mundos/voces/GPU con orden definido | `re_platform_open/close`, API de sesión actual |
| `vg_world_create/load/destroy`, `vg_request_world_change` | Carga candidata; cambio diferido, error deja mundo anterior intacto | `re_world_load` transaccional; fallo de `SceneManager.goto` |
| `vg_entity_create/destroy/is_alive` | ID generacional; destruir durante iteración difiere final del tick | `ReEntityId`; thinkers DOOM como idea |
| `vg_transform_get/set`, `vg_entity_set_parent` | Copias POD, validación finite/quaternion/escala/ciclos; parent mode explícito | `ReMarker` insuficiente; Node3D |
| `vg_mesh_set`, `vg_sprite_set`, `vg_camera_set`, `vg_light_set` | Descriptor tipado; retiene assets; no depende de raylib | Modelos/sprites/luces con mismo runtime |
| `vg_collider_set`, `vg_raycast`, `vg_sweep`, `vg_overlap` | World-space Z-up, máscaras, ignored entity y resultados con normal/distancia/fracción | Quake `SV_Move`; canales actuales de `re_world_trace` |
| `vg_trigger_set`, `vg_interactable_set`, `vg_event_emit/poll` | Payload acotado copiado; source/target estables; overflow explícito | Reglas/eventos existentes |
| `vg_asset_acquire/release/get_info`, `vg_material_create/set_params` | Assets compartidos; retención explícita; parámetros validados contra esquema | Caché omitido de js-game |
| `vg_shader_load`, `vg_render_profile_set`, `vg_post_effect_set` | Requires capabilities; compile error conserva programa anterior; GLSL GPU | Petición GPU y shaders custom |
| `vg_audio_play/stop`, `vg_audio_bus_set`, `vg_music_play`, `vg_audio_emitter_set` | Sound immutable vs Voice mutable; música streaming | Límite actual de 16 PCM y sesión sin audio conectado |
| `vg_input_action_get`, `vg_input_bind`, `vg_settings_get/set/save` | Input lógico común a hosts; settings tipados | Bindings fijos duplicados |
| `vg_ui_submit`, `vg_debug_draw`, `vg_stats_get` | Comandos de presentación copiados para frame; sin mutar simulación | Canvas actual y debugger futuro |
| `vg_game_register_type/action`, `vg_gamekit_player_configure` | IDs/versión/propiedades y callbacks; gamekit opt-in | Crear enemigos/puertas/jugador sin cambiar core |

Enemigos y armas no son tipos obligatorios de la API nuclear. El juego registra `mygame.guard` o usa gamekit; inspector, loader y creación por C resuelven el mismo TypeId. Propiedades serializables se describen con un esquema pequeño (nombre, tipo, default, rango, referencia); estado C privado puede tener callbacks de save/load/migrate si se desea persistencia. No serializar memoria arbitraria de `user`.

Para geometría code-first: `vg_mesh_create` acepta arrays de vértices/índices y los copia o consume según una variante explícita; no un bool ambiguo de ownership. `vg_room_create` puede ser helper de contenido/gamekit que produce receta y mesh, sin exigir sectores a todo usuario.

## Game API recomendada

```c
typedef struct {
    uint32_t struct_size, api_version;
    void *user;
    VgResult (*init)(VgContext *, void *user);
    VgResult (*world_ready)(VgContext *, VgWorld, void *user);
    void (*fixed_update)(VgContext *, VgWorld, float dt, void *user);
    void (*event)(VgContext *, const VgEvent *, void *user);
    void (*draw_ui)(VgContext *, VgUiFrame *, void *user);
    void (*shutdown)(VgContext *, void *user);
} VgGameCallbacks;
```

`VgEvent` y `VgUiFrame` son tipos que el header final deberá definir; este bloque explica lifecycle, no se entrega como header compilable. Se usa `fixed_update` en vez de `Game_Update` ambiguo. Draw es entrega de UI/comandos, no acceso implícito al estado OpenGL. El motor dibuja sus componentes. Pasos gráficos custom de bajo nivel serían extensión explícita del backend con capability, fuera del SDK inicial portable.

Orden: create→register callbacks/types→init→load/create world→world_ready→ticks/events/render→world unload→shutdown→destroy. `shutdown` se llama una vez si se entró a init, incluso ante init parcialmente fallido; el juego debe tolerar ese estado. Recursos del engine se liberan por contexto aun si el juego falló. El mundo sigue válido durante su callback de unload si éste se añade; después todos sus handles quedan inválidos.

Host normal usa `vg_run` para loop incorporado; host embebido usa `vg_step(elapsed, input)` y `vg_render(surface, camera)` separados. No mezclar `vg_run` con stepping manual sobre el mismo contexto. El host embebido permite cámara de edición sin simular gameplay: editar no requiere crear una sesión de Player y controlar su personaje.

Callbacks son síncronos en hilo dueño de contexto. No pueden destruir el contexto ni reentrar `vg_step`; transiciones/spawns/destrucciones durante iteraciones estructurales se encolan. Eventos se drenan con presupuesto por tick y reportan overflow/ciclo, heredando el valor de `cycle_limited` actual. Una excepción C++ nunca cruza la ABI C.

## Ownership y errores

- Entradas `const char*` son UTF-8 prestado durante llamada; si runtime necesita conservar, copia. Nombres con longitud máxima/error explícito, no truncamiento silencioso.
- Descriptores/arrays se copian salvo funciones cuyo nombre documente préstamo. Evitar préstamos de larga duración en v1.
- `acquire` devuelve una retención del llamador. Asignar asset a componente agrega retención independiente. Tras asignar, el llamador puede `release`; el componente mantiene el recurso. Destruir entidad libera sus retenciones, no invalida otros usuarios.
- `get` devuelve copia o rellena buffer de usuario. Enumeración permite consultar capacidad necesaria y repetir; error de buffer no produce salida parcialmente válida.
- `VgResult`: OK, INVALID_ARGUMENT, INVALID_HANDLE, WRONG_CONTEXT, NOT_FOUND, CAPACITY, OUT_OF_MEMORY, IO, FORMAT_VERSION, UNSUPPORTED, GPU_ERROR, CONFLICT. Error detallado por operación/buffer, no singleton global mutable.
- Operación fallida no cambia mundo/documento salvo estados de diagnóstico definidos. Carga/upload se publica sólo al terminar; `vg_asset_get_info` distingue loading/ready/failed.
- Handles sólo se manipulan en hilo dueño; futuros workers envían comandos. `release` de GPU se agenda y ejecuta con contexto válido.
- Todas las cantidades documentan unidades: metros, radianes, segundos, lineal/sRGB, bytes, frames o samples. No convertir grados según el nombre de un campo sin declararlo.

## Un juego enteramente en C

Recorrido de aceptación: proyecto C externo registra callbacks; crea world vacío; crea mesh de habitación o carga GLB; crea cámara/controlador; crea diez entidades de un mismo Model handle, cinco luces, trigger, bisagra y emisores; define callbacks de interacción; modifica fog/material; dibuja HUD; solicita otro nivel. No hay edición de `src/engine`, `src/session` ni dependencia de WPF.

El mismo contenido puede venir de `.vlevel.json`: loader llama constructores/validadores equivalentes a la API. El juego no necesita serializar todos sus niveles si prefiere generarlos en init. Si quiere editarlos después, usa Tool API para producir documento con IDs persistentes; `vg_entity_create` en runtime por sí solo no implica guardado al proyecto.

## Tool API y game DLL futura

Tool API: `vg_document_open/create/get_revision`, `vg_edit_begin(expected_revision)`, comandos tipados, `vg_edit_validate/commit/cancel`, `vg_document_save`. Devuelve mapping de IDs nuevos y diagnósticos por ruta/propiedad. Es la capa usada por Studio y futura CLI; no usar `set_property` textual como único contrato de rendimiento para el juego.

Game DLL posterior: función de entrada que negocia versión y devuelve tabla de callbacks; engine proporciona tabla de servicios. Módulo conserva estado privado, pero tiene que destruirlo con su propio código. Antes de unload: detener callbacks, cancelar jobs/timers/voces referenciadas y garantizar que no queden function pointers. Hot reload necesita export/import de estado versionado; no está desbloqueado sólo por usar DLL.

No cargar módulos C de origen no confiable como si fueran scripts aislados. Automation sobre documentos no necesita ejecutar C arbitrario ni un proceso MCP.

## Validación del SDK antes de declararlo estable

1. Dos consumidores fuera del árbol: FPS pequeño y exploración sin armas, ambos con mismo SDK y sin headers internos.
2. Header compila en C11/C23 y C++; pruebas de ABI x64/DLL con C# según plataforma realmente soportada.
3. Handle destruido, doble release, tipo/contexto incorrecto y generaciones obsoletas se rechazan sin UB.
4. Fallos de parse, allocation y GPU upload dejan estado anterior utilizable.
5. Misma escena desde archivo y C produce componentes equivalentes y mismo resultado de interacción.
6. Destruir mundo/contexto libera CPU, voces y recursos GPU; repetir cargar/descargar no crece sin límite.
7. Versión/tamaño incompatibles fallan con mensaje claro. SDK instalable y ejemplo construido desde ruta con espacios, sin buscar archivos en el cwd.
