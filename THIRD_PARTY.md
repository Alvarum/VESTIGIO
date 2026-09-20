# Dependencias y procedencia

- raylib 6.0: https://github.com/raysan5/raylib/tree/dbc56a87da87d973a9c5baa4e7438a9d20121d28
  Licencia zlib/libpng. Se conserva su LICENSE en las distribuciones.
  También se incluye el aviso de GLFW en el paquete. El código se descarga sin modificaciones; sus dependencias incluidas conservan
  sus avisos dentro del árbol fuente de raylib.
  `licenses/raylib-external` conserva las fuentes originales de sus dependencias
  embebidas para acompañar sus avisos íntegros; no se compilan al abrir el juego.
- raygui 5.0: https://github.com/raysan5/raygui/tree/020a61bebcbe288b4414de3416e219ef40af847a
  Licencia zlib/libpng. Sólo implementa los controles de Studio y permanece
  aislada de las APIs públicas del motor.
- GCC, MinGW-w64, CMake, Ninja, GDB y Clang: herramientas de desarrollo de MSYS2,
  instaladas aisladamente. Sus licencias están en `.tools/msys64/ucrt64/share/licenses`.
  El paquete incluye los avisos de GCC runtime y MinGW-w64 CRT enlazados estáticamente.
- Arte, fuente bitmap y ondas de sonido: definidos para este proyecto en C.
  No se utilizan sprites, sonidos, texturas, niveles ni código de Doom.
- `assets/studio/art/haunted-title.png`: imagen original creada para este
  repositorio con OpenAI ImageGen el 2026-09-20. No contiene texto, logos ni
  material de terceros; el prompt y el método se registran en la entrega.
- La investigación aportada por el usuario se conserva sin modificaciones.

El proyecto no presupone una licencia de publicación para el trabajo del usuario.
Antes de distribuir públicamente el código propio, el titular puede elegirla.
