# Dependencias y procedencia

- raylib 6.0: https://github.com/raysan5/raylib/tree/dbc56a87da87d973a9c5baa4e7438a9d20121d28
  Licencia zlib/libpng. Se conserva su LICENSE en las distribuciones. También
  se incluye el aviso de GLFW y las fuentes originales de las dependencias
  embebidas por raylib dentro de `licenses/raylib-external`.
- Dirkster.AvalonDock 5.0.0, Serializer.Xml 5.0.0 y Themes.Arc 5.0.0:
  https://github.com/Dirkster99/AvalonDock/tree/408dc2896e2f41f3bb79a15207f160edee8a6792
  Licencias MS-PL / Apache-2.0. Implementan el layout desacoplable de Studio;
  las versiones se fijan mediante `packages.lock.json`.
- .NET 10 y WPF: https://github.com/dotnet/wpf
  Licencia MIT. Sólo se requieren para construir y ejecutar Studio; el motor,
  Player y los juegos permanecen en C23.
- GCC, MinGW-w64, CMake, Ninja, GDB y Clang: herramientas de desarrollo de
  MSYS2, instaladas aisladamente. Sus licencias están en
  `.tools/msys64/ucrt64/share/licenses`. El paquete incluye los avisos de GCC
  runtime y MinGW-w64 CRT enlazados estáticamente.
- Arte, fuente bitmap y ondas de sonido: definidos para este proyecto. No se
  utilizan sprites, sonidos, texturas, niveles ni código de Doom.
- `assets/studio/art/haunted-title.png`: imagen original creada para este
  repositorio con OpenAI ImageGen el 2026-09-20. No contiene texto, logos ni
  material de terceros; el prompt y el método se registran en la entrega.
- La investigación aportada por el usuario se conserva sin modificaciones.

## cgltf

- Versión 1.15, vendorizada como dependencia privada del importador.
- Origen: `third_party/cgltf/cgltf.h`, copia byte por byte de
  `src/external/cgltf.h` en el archivo fuente fijado de raylib, commit
  `dbc56a87da87d973a9c5baa4e7438a9d20121d28`.
- Upstream: https://github.com/jkuhlmann/cgltf
- Licencia MIT; aviso completo en `third_party/cgltf/LICENSE` y en el header.
- SHA-256: `efb169dee911696b5d35fc8e3f7ea0c56d679debc529eba9ca6aa6443ba9d5e9`.
- El build compila el header vendorizado y no necesita acceso a red.

El proyecto no presupone una licencia de publicación para el trabajo del
usuario. Antes de distribuir públicamente el código propio, el titular puede
elegirla.
