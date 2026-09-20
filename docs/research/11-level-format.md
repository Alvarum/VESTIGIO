# 11 — Proyecto, niveles y serialización

Propuesta. No cambia loaders existentes. Baseline: `ReProject` embebe un `ReWorld`; `re_world_load/save_v4` y `re_project_commit` ya validan/migran/guardan transaccionalmente. La arqueología encuentra export/import divergente en `SceneEditor._export` y `SceneLoader.load`; por eso **un codec canónico** es requisito central.

## Formato evaluado

| Opción | Ventajas | Coste / decisión |
|---|---|---|
| Texto propio actual | Parser pequeño, líneas/errores y compatibilidad existente | Se complica con jerarquía, arrays, componentes y metadatos extensibles; preservar como legacy |
| JSON UTF-8 | Herramientas maduras, legible, validable y fácil de generar desde C/herramientas | Verboso y sin comentarios; **autoría nueva elegida** |
| Binario único | Carga/espacio eficientes | Diff y migraciones difíciles; no autoridad de edición |
| Híbrido | Texto fuente + blobs binarios de modelos/texturas y caché cocinada | **Elegido**, binario derivado sólo cuando carga/packaging lo justifique |

No serializar `sizeof(struct)` del runtime: padding, punteros, endianness y nuevas versiones lo romperían. Meshes/texturas grandes van en assets, no arrays enormes en JSON de nivel. Esquemas de propiedades alimentan inspector y codecs, sin introducir reflection universal.

## Proyecto externo al engine

```text
MyGame/
  project.vg.json
  assets/
    models/        # fuentes y .vgmeta.json
    textures/
    audio/
    shaders/
    fonts/
    materials/
  levels/
    entrance.vlevel.json
  definitions/
    actors/
    interactions/
    prefabs/
  src/             # opcional para juego sólo visual
    game.c
    doors.c
    enemies.c
  config/
    defaults.json
  .vestigio/       # caché, estado editor/autosave; excluido del paquete
  CMakeLists.txt    # opcional en juego sólo visual
```

Proyecto referencia engine/SDK por versión requerida y capacidades. SDK instalado vive fuera del proyecto. Preferencias del usuario y partidas van a directorio de usuario por ProjectId, no a `assets` ni a la carpeta de instalación. Fuente y assets propios sí se versionan; caché, binarios y estado del layout editorial no.

Manifiesto conceptual:

```json
{
  "format": "vestigio.project",
  "version": 1,
  "id": "992986ab-056a-4dbd-94b1-2b50c42e3067",
  "name": "MyGame",
  "engine": { "api_major": 1, "min_minor": 0 },
  "entry_level": "levels/entrance.vlevel.json",
  "game": { "kind": "builtin", "module": "retro_exploration" },
  "defaults": "config/defaults.json",
  "asset_roots": ["assets"],
  "definition_roots": ["definitions"]
}
```

Son nombres y versión futuros ilustrativos. No representan un proyecto ya cargable. Un juego C selecciona módulo compilado en su host, sin depender de localizar una DLL por nombre en esta primera versión.

## Contenido de un nivel

- Metadata: LevelId, nombre, versión del formato, required capabilities, dependencias.
- Environment: sky/background, ambient, fog y referencia a render profile con overrides; no configuración física de ventana.
- Geometry: receta sectorial legacy o mesh/recetas nuevas; geometría derivada no es segunda autoridad mutable.
- Entities: UUID, nombre, transform local/padre opcional y componentes tipados/versionados.
- Gameplay: referencias a tipos/definiciones/acciones y propiedades de juego en namespace propio.
- Editor: capas/grupos, bookmarks y organización opcional; no runtime handles ni selección persistida como dato de juego.

Luces, audio, triggers, spawn points, puertas, interactuables, enemigos, decals y cámaras son entidades/componentes; evitar tablas paralelas que dupliquen su posición/ID. Una tabla de tipos conocida indica cuáles son de engine y cuáles de gamekit/game module.

Ejemplo mínimo de estructura (los AssetIds deben resolverse en catálogo; omite geometría y contenido adicional para mostrar contratos):

```json
{
  "format": "vestigio.level",
  "version": 1,
  "id": "a615a968-fb84-4c24-9a47-6e81bf4373c5",
  "name": "Entrada",
  "coordinates": "right-handed-z-up-meters",
  "required": ["mesh3d", "kinematic-door"],
  "environment": {
    "profile": "psx",
    "fog": { "mode": "linear", "color": [0.12, 0.14, 0.16], "start": 4, "end": 25 }
  },
  "entities": [
    {
      "id": "d169b28b-e1cd-4380-b990-df5cfa7d6db4",
      "name": "Puerta de entrada",
      "transform": { "position": [2, 0, 0], "rotation": [0, 0, 0, 1], "scale": [1, 1, 1] },
      "components": {
        "engine.mesh": { "version": 1, "asset": "7d7a572a-3785-48f9-a346-13ba22e9a3c1" },
        "engine.collider": { "version": 1, "shape": "box", "half_extents": [0.45, 0.03, 1.05], "center": [0.45, 0, 1.05], "motion": "kinematic" },
        "gamekit.door": { "version": 1, "hinge": [0, 0, 0], "axis": [0, 0, 1], "closed_angle": 0, "open_angle": 1.5707963, "speed": 1.5, "locked": false, "auto_close_seconds": 3 }
      }
    }
  ]
}
```

En este ejemplo el asset del panel debe estar definido en reposo desde bisagra x=0, extendiéndose hacia +x. La geometría de asset/collider comparten convención. No inferir origen automáticamente del bounding box.

Rotación serializada quaternion `[x,y,z,w]`, unidades radianes para ángulos de comportamiento, colores con espacio declarado por esquema (fog/lighting lineal; texturas baseColor sRGB). Nunca alternar entre arrays y objetos según el consumidor. Editor muestra grados si resulta cómodo, pero convierte al contrato canónico.

## Identidad y referencias

UUID estable para entidad, nivel, asset y definición. Runtime handle se resuelve al instanciar; no se escribe al disco. Duplicar un grupo crea nuevos UUIDs y remapea referencias internas en bloque. Borrar entidad referenciada ofrece error o operación de reparación explícita; no deja referencias silenciosamente rotas.

Referencias a otro nivel se expresan mediante LevelId + destino SpawnId; loader valida al empaquetar. Un nivel puede contener varias cámaras/spawns; el proyecto/juego elige el inicial. Reglas apuntan a EntityId/Tag/DefinitionId con semántica explícita; no confundir nombre visible con ID.

Definiciones (actor, material, prefab) son compartidas; instancia contiene overrides. Primer prefab: expansión por receta con fuente/overrides simples. Prefabs anidados y propagación compleja se posponen; export no puede depender de IDs efímeros del editor.

## Validación y migración

Pipeline: parse límites→validación estructural/tipos→versiones→IDs y referencias→transforms/ciclos→capacidades y assets→reglas/colisión→instanciar candidato. Diagnósticos incluyen archivo, JSON path, EntityId, severidad y remedio; en legacy conservar número de línea.

Versiones independientes: project, level, componente, importer y savegame. `version` futura requerida se rechaza sin sobrescribir; campos de extensión opcionales desconocidos se preservan en documento editor para round-trip, pero no se ejecutan. Componente requerido desconocido bloquea jugar/exportar. No presentar como compatible un archivo cuyo comportamiento se descartó.

Migraciones puras y secuenciales sobre datos, de n→n+1; fuente respaldada, reporte y nuevo archivo sólo tras validar. Igual entrada genera misma salida semántica; orden estable de claves/arrays por ID para diffs, precisión float de round-trip, números no finitos rechazados y locale independiente.

Legacy `retro_map 1..4`, project/actor/rules/dialogue actuales continúan leyéndose. Import a nuevo proyecto genera IDs determinísticos con namespace de ProjectId/ID legacy y conserva etiquetas/reglas. Sectores se envuelven en componente/geometry legacy, sin perder portales. Escribir versión nueva no debe destruir automáticamente la antigua; migración se ejecuta sobre copia con reporte. No subir simplemente `retro_map 5` y fingir que sectores ahora son mallas.

## Guardado, recuperación y colaboración con herramientas

Documento mutable y assets compartidos separados. Guardar prepara candidatos, valida referencias, realiza transacción recuperable siguiendo `re_project_commit`. Múltiples archivos con journal/backups no equivalen a atomicidad instantánea de todo filesystem, pero permiten recuperación; mantener esa distinción. Autosave guarda snapshot de cambios no confirmados en `.vestigio/autosave` con revisión; jamás se anuncia como guardado manual.

CLI/Tool API requiere expected revision y un lote de operaciones. Si usuario editó mientras agente preparaba cambios, devolver CONFLICT + IDs afectados; rebase/merge sólo con validación. Escritura externa detectada invalida reload automático destructivo. `ProjectLock` actual es antecedente local, no sistema de edición colaborativa distribuida.

## Build y partidas

Paquete de juego: runtime/host + game module incorporado/compilado + proyecto validado + cierre transitivo de assets + avisos. Excluir src, caché, autosaves, temporales y recursos no referenciados salvo inclusión explícita. IDs/rutas se resuelven independientemente del cwd; probar ruta con espacios y Unicode. No depender de carpetas de herramientas de desarrollo.

Savegame distinto de nivel: ProjectId, LevelId/content revision, versión y estado mutable por entidad/componente (puerta, actor, inventario, RNG, variables, diálogos, jugador). Definir qué proyectiles/voces/timers se persisten o reinician. No asumir snapshot completo porque `re_save_write` guarda interacción y actores; los proyectiles actuales son de sesión. Restaurar contra contenido incompatible falla con diagnóstico o migración, no aplica arrays a otras entidades.

Binario cooked posterior contiene header/version/endianness/chunks/checksums, índices remapeados y dependencias. Es derivado regenerable, no reemplaza `.vlevel.json` como archivo de autoría.

## Contratos de aceptación

Round-trip completo y equivalente de todos los componentes editables; save/load tras duplicar/reparentar; fuentes de modelos renombradas; campos opcionales desconocidos preservados; required desconocidos rechazados; migraciones legacy con portales/diálogos intactos; fallo a mitad de commit recuperable; error de JSON/recurso deja documento anterior; generación por C/Tool API produce el mismo contenido que el editor. Ningún widget se considera entregado si su dato no sobrevive este recorrido.
