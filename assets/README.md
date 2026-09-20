# Recursos

- `foundry.map`: geometría y marcadores de Foundry, editables sin recompilar C.
- `lab.map`: geometría del consumidor independiente Retro Lab.
- `studio/haunted.retro`: proyecto de aprendizaje para Studio.
- `studio/logic/haunted.rules`: reglas CUANDO/SI/HACER, triggers, objetivos y luces.
- `studio/dialogues/haunted.dialogue`: conversación ramificada del showcase.
- `studio/art/haunted-title.png`: portada original de Haunted. Generada para
  RetroForge con OpenAI ImageGen el 2026-09-20 y conservada como PNG fuente;
  no contiene material de terceros ni texto incrustado.
- `studio/levels/house.map`: casa v2, escalera, plantas y barrera dinámica.
- `studio/actors/*.actor`: perseguidora configurable y jefe de dos fases.

Las texturas, sprites, fuente y sonidos son originales y se generan una vez
desde código. Así puedes estudiar su construcción y modificar su paleta:

- `src/fps/art.c`: metal, paneles, guardia, pickups, pistola y PCM.
- `src/engine/canvas.c`: fuente bitmap 5×7.
- `src/lab/main.c`: materiales didácticos del laboratorio.

No se descargan recursos artísticos durante la ejecución. La plataforma también
ofrece carga de imágenes externas para juegos futuros. No hay assets de Doom.

Ver `docs/05-formato-mapas.md` para reglas y ejemplos completos.
