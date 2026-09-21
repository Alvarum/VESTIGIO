# Contratos de integración que F01 debe fijar

Estado: **contrato v0.1 materializado por F01** para vocabulario mínimo, ownership y fronteras; implementación funcional se añade en sus tickets. `include/vestigio/vestigio.h` existe y se compila como C11/C++11. `src/render/render_contract.h` es privado y tampoco implica que exista todavía un renderer GPU. Los agentes implementan contra esta versión y registran cualquier revisión necesaria.

## C01 — Frontera pública y distribución

Runtime interno C23; header público consumible C11/C++ sin tipos privados/raylib/WPF. Contextos opacos, handles tipados y descriptores POD con tamaño/versión. `create`/`get` por punteros de salida y `Result` explícito; enums/flags de ABI con ancho fijo. UTF-8 con ownership y unidades documentados. Librería estática primero; DLL de tooling con exports explícitos. SDK no incluye una Game DLL como requisito.

F01 fija Windows x64 como primera ABI, C calling convention por defecto, `VG_API_VERSION=0.1`, `VgResult`, `VgContext` opaco, handles de 64 bits, descriptores con `struct_size/api_version`, target `Vestigio::Headers` y `#include <vestigio/vestigio.h>`. R03 añade paquete CMake y lifecycle; G03 incorpora draw de modelo al consumidor. No someter headers de consumidor C11/C++ a la función CMake interna que fuerza flags C23 y avisos sólo válidos para C.

## C02 — Identidad, Transform y memoria

Entity/World/Asset handles no son UUID persistentes. Validación considera tipo, contexto, mundo y generación; cero inválido. Entidad destruida durante iteración se retira en frontera definida. Componentes retienen assets; contexto posee servicios y cleanup ordenado. Operación inválida/OOM no publica estado parcial.

Mundo diestro Z-up, metros, radianes, segundos; quaternion `[x,y,z,w]`. F01 fija layout/multiplicación de matrices y local→world. Cambio glTF: `(x,y,z) → (x,-z,y)` y matrices `C M C^-1`, no recenter arbitrario. Reparent declara conservar local o world; escala cero, no finitos, ciclos y shear no representable se rechazan o se soportan explícitamente. Una rotación de padre no admite descomposición TRS falsa.

## C03 — Plataforma, superficie y backend GPU

Un dueño de ventana/contexto y un hilo gráfico inicial. El contrato interno de superficie es distinto de captura CPU. Debe informar lifecycle, tamaño físico/DPI, resize/minimize/focus, presentación y quién destruye cada objeto. La API de juego no expone HWND; el adaptador de host nativo sí puede conocerlo sin contaminar SDK.

G01 implementa GPU en ventana propia. G02 fija Windows/WPF en un `HwndHost`: raylib crea una única ventana/contexto OpenGL en el hilo UI, el adaptador Win32 la convierte en hija con `SetParent` y mantiene tamaño físico, foco y destrucción. `GetWindowHandle` sólo identifica la ventana creada; no se adopta un HWND ajeno. La superficie vive mientras el `HwndHost` está conectado y `Dispose` cierra renderer/contexto antes de permitir otro host. Edit/Play comparte esa superficie inicial; no se permiten dos `InitWindow` simultáneos. Frames normales conservan color/depth/postprocess en GPU; readback se etiqueta sólo como captura/thumbnail. Sin GPU apta: error/capacidad declarada, no etiqueta GPU falsa.

F01 fija `VgSurfaceInfo`, `VgDrawPacket`, `VgFrameStats` y `VgRenderContract` como contrato **interno** inicial en `src/render/render_contract.h`; no es API instalada. G01/G02 pueden proponer una revisión si su prueba demuestra límites. R03 no promete ABI de embedding definitiva antes de ese resultado.

## C04 — Assets e importación

AssetId estable + ruta/meta/fingerprint/versiones de importer; AssetHandle efímero para residente. IR CPU propia entre parser y renderer, con submeshes/nodos/materiales/texturas/dependencias. Decode/validación no invoca API gráfica. GPU upload/release ocurre en su hilo y publica candidato completo. Mesh/textura compartidas; transform y pose por instancia.

Documentar acquire/release/asignación a componente, estados loading/ready/failed, caché evictable, purge y error de reload. Contadores separan RAM fuente/derivada y VRAM propia estimada. Ruta y alias no generan copias por cada instancia. Material tiene shader/params/textures tipados; perfil no modifica el asset fuente.

## C05 — Documento, mundo y sesiones

Documento nativo es autoridad editable; World es instancia runtime. Studio mantiene selección/UI, no un serializer alternativo ni copia editable paralela. Constructores/validadores comunes producen el mismo componente desde C y archivo. Assets pesados/GPU fuera del historial.

Tool API: begin(expected revision)→preview→validate→commit/cancel; salida de IDs/diagnósticos. Error y cancelación conservan undo/redo. Save manual, autosave y partida son distintos. Preview de juego crea copia; Stop no persiste mutaciones del juego automáticamente.

Formato con versiones project/level/componente/importer/save separadas. Optional desconocido se retiene en documento; required desconocido impide jugar/exportar. UUIDs estables y refs tipadas; no índices de arrays persistidos. Migración sobre copia y recuperación de commit mantienen originales.

## C06 — Juego, input y eventos

Host produce acciones pressed/held/released, no lógica de personaje. Simulación fija; dt/pacing/render distintos. Callbacks init/world_ready/fixed_update/event/draw_ui/shutdown con orden, errores y reentrancia definida. Input acumulado se consume por tick y limpia en pérdida de foco.

Eventos son datos copiados con límites/source/target; spawns/destrucciones/transiciones se difieren si iteración lo exige. Núcleo no requiere armas/enemigos: juego registra tipos/acciones y esquema; gamekit es opcional. Cambiar nivel carga candidato y conserva anterior si falla.

## C07 — Colisión, puertas y audio

Collider y renderable comparten Transform. API spatial define world-space, máscaras, ignored entity, normal y fracción/distancia. Bisagra mantiene collider sólido incluso abierta; cierre obstruido bloquea/revierte, no aplasta por defecto. Sweep angular tiene substeps/límites verificables. Puerta emite evento semántico; Audio lo convierte en Voice, sin replay por cada frame.

Audio separa Sound, Voice y Music streaming; listener/buses/settings compartidos. Estado puerta/trigger/actor es serializable con IDs, y P01 decide transitorios explícitamente. Colisión general rígida y navmesh universal quedan fuera.

## C08 — Contrato de integración y cambios

Cada frontera tiene owner y versión; cada worker entrega lista real de escritura. Cambiar firma/layout/schema implica revisar consumidores/tickets afectados. Los ejemplos de la investigación son ilustrativos, no nombres obligatorios ni un header ya implementado.

Registro que debe completar F01:

| Frontera | Decisión/archivo definitivo | Owner | Versión | Consumidores verificados |
|---|---|---|---|---|
| SDK/context/world | `include/vestigio/vestigio.h`, `Vestigio::Headers` | integrator/runtime | v0.1 | Tests C11/C++11; lifecycle pendiente R01/R03 |
| Surface/backend/draw packets | `src/render/render_contract.h`, `src/platform/gpu_host.*` privados | integrator/gpu | v0.1 interno | G01/G02 verificados en OpenGL 3.3 y WPF/HwndHost; conexión a World pendiente G03 |
| Asset/IR/retenciones | `VgAsset` público + C04 | runtime/content/gpu | v0.1 vocabulario | Implementación pendiente R02/M01/G03 |
| Documento/schema/Tool API | C05; sin declaraciones públicas prematuras | integrator/content/editor | v0.1 semántico | Implementación pendiente D01/D02 |
| Input/callbacks/eventos | C06; sin tabla incompleta en header | runtime | v0.1 semántico | Implementación pendiente R03/I01 |

El header declara sólo `vg_get_version`; su definición llega con el target de runtime de R01/R03. Esta ausencia deliberada evita stubs que aparenten lifecycle funcional. Los demás conceptos se agregan cuando exista su primer consumidor y pruebas, manteniendo las reglas C01–C08.
