# Formatos de contenido VESTIGIO

Esta carpeta describe el contrato realmente aceptado por `vestigio_content`.
Los documentos son JSON UTF-8 y tienen dos discriminadores obligatorios:
`format` y `version`. D01 implementa `vestigio.project` versión 1 y
`vestigio.level` versión 1. Un número de versión futuro se rechaza antes de
crear un documento.

El codec es independiente del runtime, GPU, ventana y editor. Analiza y valida
un candidato completo antes de publicar el puntero de salida. Un error de
lectura, parseo o validación deja intacto el documento que el llamador ya tenga.
La aplicación de documentos a un mundo y el guardado transaccional en disco
pertenecen a tickets posteriores.

## Reglas comunes

- El archivo mide entre 1 byte y 1 MiB, contiene como máximo 32 768 tokens,
  anida como máximo 64 niveles y cada string o clave ocupa como máximo 65 535
  bytes decodificados.
- Sólo se admite JSON estricto: UTF-8 válido, claves únicas y números finitos.
  No se admiten comentarios, `NaN`, `Infinity`, NUL embebido ni comas finales.
- Los UUID usan la forma minúscula `8-4-4-4-12`; el UUID cero se rechaza.
- Los campos desconocidos son opcionales y permanecen en el DOM para el
  round-trip. Una capacidad, extensión o componente desconocido incluido en
  una lista `required*` se rechaza.
- Cada diagnóstico incluye código, archivo, JSON path, UUID relacionado cuando
  se conoce, byte de parseo cuando corresponde y mensaje.

La serialización canónica ordena las claves de cada objeto por sus bytes UTF-8,
usa dos espacios, saltos LF, escapes JSON mínimos y un LF final. Conserva el
orden de **todos** los arrays porque un array dentro de una extensión desconocida
puede tener semántica posicional. También conserva el lexema decimal válido de
cada número, lo que evita perder precisión o depender del locale. Por ello,
parsear, serializar, volver a parsear y serializar produce exactamente los
mismos bytes; la canonicalización no reordena entidades ni sectores.

## `vestigio.project` versión 1

Miembros requeridos:

| Miembro | Contrato |
| --- | --- |
| `format` | string exacto `vestigio.project` |
| `version` | entero `1` |
| `id` | UUID estable del proyecto |
| `name` | string UTF-8 no vacío, máximo 128 bytes |
| `engine` | objeto con `api_major: 0` y `min_minor` entre 0 y 1 |
| `entry_level` | ruta relativa con `/`, sin unidad, `.` ni `..` |

Miembros implementados opcionales:

- `game`: `{ "kind": "builtin", "module": "..." }`.
- `defaults`: ruta relativa.
- `asset_roots` y `definition_roots`: hasta 16 rutas relativas, no duplicadas.
- `required`: hasta 32 capacidades. En project v1 sólo se implementa `core`.
- `extensions`: objeto conservado sin ejecutarse.
- `required_extensions`: debe estar vacío en v1.

## `vestigio.level` versión 1

Miembros requeridos:

| Miembro | Contrato |
| --- | --- |
| `format` | string exacto `vestigio.level` |
| `version` | entero `1` |
| `id` | UUID estable del nivel |
| `name` | string UTF-8 no vacío, máximo 128 bytes |
| `coordinates` | `right-handed-z-up-meters` |
| `entities` | array de hasta 1 023 entidades |

`required` puede declarar `core`, `transform`, `environment`,
`legacy-geometry`, `component.engine.camera.v1`,
`component.engine.mesh.v1` y `component.engine.collider.v1`.
`extensions` se conserva. `required_extensions` debe estar vacío.

### Transform e identidad

Cada entidad requiere un UUID único y un objeto `transform` con:

- `position`: tres números finitos en metros.
- `rotation`: quaternion normalizado `[x,y,z,w]`.
- `scale`: tres números estrictamente positivos.

`parent` es `null` o el UUID de otra entidad del mismo nivel. Se rechazan
referencias ausentes y ciclos. El handle generacional del runtime nunca se
serializa.

### Environment

`ambient_linear` y `clear_linear` son colores RGB lineales en `[0,1]`. `fog`
acepta `none`, `linear` (`0 <= start < end`) o `exponential` (`density > 0`) y
requiere `color_linear` RGB. Los nombres declaran el espacio de color para no
confundirlos con texturas sRGB.

### Geometry legacy

`geometry` opcional implementa `{ "kind": "legacy.sectors", "version": 1 }`.
Contiene hasta 256 sectores. Cada sector requiere UUID único, `floor < ceiling`
y entre 3 y 64 vértices `[x,y]`. Un portal usa un índice de arista único dentro
del sector, referencia el UUID de un sector existente y declara una apertura
normalizada `[min,max]` con `0 <= min < max <= 1`. Esto preserva portales
parciales; el formato no convierte silenciosamente sectores en mallas.

### Componentes iniciales

- `engine.camera` v1: `0 < fov_y_radians < pi` y `0 < near < far`.
- `engine.mesh` v1: `asset` es un UUID. D01 valida la forma; resolver el catálogo
  de assets ocurre en la capa que dispone de ese catálogo.
- `engine.collider` v1: shape `box`, `center`, `half_extents` positivos y motion
  `static` o `kinematic`.

Un componente desconocido se conserva sin ejecutarse. Si aparece en
`required_components`, el documento se rechaza. Todo componente requerido
conocido debe existir en `components`.

## Procedencia del parser

`src/content/third_party/jsmn.h` es la copia JSMN incluida en cgltf 1.15 dentro
del source pinneado de raylib 6.0 del repositorio. Se extrajo localmente, sin
descarga ni dependencia remota nueva, conserva el aviso MIT completo y se usa
en modo estricto con parent links. `src/content/json.c` añade el DOM, límites,
UTF-8/Unicode, números independientes del locale, detección de claves duplicadas
y escritura canónica.
