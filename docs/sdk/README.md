# SDK C de VESTIGIO 0.1

R03 distribuye el runtime inicial como biblioteca estática, el header público
`<vestigio/vestigio.h>` y un paquete CMake. El SDK no necesita raylib, Studio,
.NET ni headers de `src/`.

```cmake
find_package(Vestigio CONFIG REQUIRED)
add_executable(my_game main.c)
target_link_libraries(my_game PRIVATE Vestigio::Runtime)
set_target_properties(my_game PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES)
```

Después de instalar el proyecto en un prefijo, configurar el consumidor con
`-DCMAKE_PREFIX_PATH=<prefijo>`. El proyecto verificable en
`tests/sdk/external-consumer` sólo usa ese paquete instalado. Su build y su
ejecución se realizan desde directorios distintos y con un `cwd` que no contiene
recursos del repositorio.

```powershell
cmake --install build/release --prefix C:/sdk/vestigio --component SDK
cmake -S my-game -B my-game/build -DCMAKE_PREFIX_PATH=C:/sdk/vestigio
cmake --build my-game/build
```

## Lifecycle

Una instancia de juego copia `VgGameCallbacks` y conserva el puntero `user`
prestado hasta `shutdown`. El orden observable es:

1. `vg_game_create` entra a `init`.
2. `vg_game_set_world` valida el handle y llama `world_ready`.
3. Cada tick fijo ejecuta `fixed_update` y después consume eventos FIFO hasta el
   presupuesto configurado.
4. El host llama `vg_game_draw_ui` cuando dispone de un frame de UI.
5. `vg_game_destroy`, o el cleanup del contexto, llama `shutdown` exactamente
   una vez si `init` llegó a comenzar.

Si `init` falla, `shutdown` se ejecuta antes de devolver el error y no se publica
la instancia. Si `world_ready` falla, el mundo anterior sigue enlazado. El cambio
de mundo exige que la cola esté vacía; los eventos emitidos por el callback
fallido se descartan. Un mundo enlazado no se puede destruir hasta llamar
`vg_game_clear_world` o destruir el juego.

`vg_game_step` usa segundos, acumula tiempo y ejecuta un `dt` fijo. Limita el
delta aceptado y el número de pasos por llamada para evitar una espiral de
actualizaciones; `VgStepInfo` separa tiempo simulado, pasos descartados, eventos
pendientes e interpolación. Los eventos se copian en una cola acotada. Los que
un callback emite conservan FIFO y también consumen el presupuesto del tick, de
modo que un ciclo no puede bloquear indefinidamente al host.

Los callbacks son síncronos. No se permite reentrar step, draw, cambio de mundo,
destrucción de juego o destrucción de contexto desde ellos. Sí se permite emitir
otro evento. La implementación inicial no carga Game DLLs ni hace hot reload.

## Cámara mínima

`VgCameraDesc` es un componente POD sobre una entidad. Su pose procede del
Transform; sólo declara proyección, clipping, FOV o altura ortográfica. No expone
una superficie, ventana ni objeto OpenGL. La conexión con el renderer GPU se
incorpora en G03.

## Toolchains y ABI

El header público es C11 y también compila como C++11. Las reglas CMake evitan
flags GCC cuando se configura con MSVC, por lo que el SDK puede construirse con
MinGW-w64 o MSVC. Una biblioteca estática debe enlazarse con un toolchain y
runtime C/C++ compatibles con los usados para construirla. Esta entrega no
promete que un `.a` de MinGW y un `.lib` de MSVC sean ABI intercambiables; para
cambiar de familia de compilador se recompila VESTIGIO y el juego.

Windows x64 es la primera ABI publicada. `struct_size`, `api_version`, enteros de
ancho fijo y handles opacos permiten evolucionar la API, pero no sustituyen la
compatibilidad de arquitectura, compilador y runtime del paquete binario.
