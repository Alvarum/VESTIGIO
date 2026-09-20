# 02 · C23, compilación y memoria

## Qué significa C23 aquí

C23 es la revisión publicada como ISO/IEC 9899:2024. La versión del lenguaje y la
versión del compilador son cosas distintas: GCC 16.2 implementa un conjunto de
características del estándar; la biblioteca C del sistema implementa otro.

Seleccionamos `C_STANDARD 23`, `C_STANDARD_REQUIRED YES` y `C_EXTENSIONS NO` en
nuestros targets. Una comprobación de `__STDC_VERSION__ >= 202311L` evita aceptar
el modo preliminar C2x de un compilador antiguo. raylib conserva su configuración
de compilación propia. No es necesario convertir una dependencia C99 a C23.

Características usadas:

- `bool`, `true` y `false`: palabras clave del lenguaje.
- `nullptr`: constante de puntero nulo; no apunta a un objeto.
- `static_assert`: comprueba un contrato al compilar, por ejemplo RGBA de 4 bytes.
- `[[nodiscard]]`: pide un diagnóstico si se ignora un resultado importante.

No usamos una característica sólo por ser nueva. Tipos explícitos facilitan
estudiar dimensiones, unidades y conversiones. `constexpr`, `typeof`, `_BitInt`
y `#embed` son temas de ampliación, no requisitos de este renderer.

Fuentes: [WG14](https://open-std.org/jtc1/sc22/wg14/),
[soporte C en GCC](https://gcc.gnu.org/projects/c-status.html) y
[C_STANDARD en CMake](https://cmake.org/cmake/help/latest/prop_tgt/C_STANDARD.html).

## De texto a programa

Cada `.c` se preprocesa y compila como una **unidad de traducción**. `#include`
incorpora declaraciones; no importa mágicamente un módulo en tiempo de ejecución.
Los include guards evitan declarar dos veces los mismos tipos dentro de la unidad.

Un header anuncia qué funciones existen. El enlazador resuelve esas funciones
con los símbolos producidos al compilar los `.c`. Una declaración incompatible
con la definición es un error de interfaz, aunque algún enlazador no lo detecte.

`static` en una función de `.c` da linkage interno: sólo esa unidad la ve.
`static inline` en `math.h` permite definiciones pequeñas privadas en cada unidad;
`inline` no es una orden irrevocable al optimizador. Las funciones públicas tienen
prefijo `re_` para evitar colisiones con otras bibliotecas.

## Tres formas de duración de almacenamiento

```c
ReVec2 direction = {0, 1};        // Objeto automático de esta llamada.
static const int limit = 16;     // Duración de todo el programa.
ReWorld *world = calloc(1, sizeof(*world)); // Objeto asignado dinámicamente.
```

La pila es una implementación habitual del almacenamiento automático. El
estándar describe duración y alcance, no exige una pila física concreta.
No devuelvas `&direction`: su vida útil termina al salir de la función.

`calloc` reserva un bloque y pone sus bytes a cero. Las estructuras que se
inicializan así deben usar representaciones compatibles con su uso posterior;
en esta plataforma los campos de estado numéricos y punteros se inicializan
antes de usarse. Para reiniciar objetos locales usamos inicializadores de C,
por ejemplo `(ReInput){0}`, que expresan inicialización semántica.

El proyecto mantiene los objetos grandes, como `FpsGame`, en el heap. Los vectores,
triángulos y scratch buffers pequeños viven durante una llamada. Ninguna rutina
del núcleo asigna memoria por píxel, entidad o tick.

## Un puntero no incluye tamaño ni propiedad

`RePixel *pixels` contiene una dirección. No dice cuántos píxeles hay, quién
puede modificarlos ni quién debe liberarlos. `ReTexture` agrupa dirección y
dimensiones; los contratos explican el propietario.

```c
size_t index = (size_t)y * (size_t)width + (size_t)x;
RePixel color = pixels[index];
```

La multiplicación debe realizarse en un tipo capaz de representar el tamaño y
estar precedida por límites válidos. Aquí las texturas se limitan a 4096 por eje;
no se convierten dimensiones negativas a `size_t`. Un número negativo convertido
a unsigned se convierte modularmente en un número grande, no sigue siendo negativo.

`sizeof(*pixels)` expresa el tamaño del elemento. `sizeof(pixels)` mide el puntero.
Dentro de una función que recibe `T array[]`, ese parámetro es realmente `T *`:
no intentes recuperar su longitud con `sizeof(array)`.

## `const` y aliasing

`const ReWorld *world` impide modificar el objeto mediante ese puntero. No
garantiza por sí solo que otro alias no lo modifique. `ReWorld *const world`
impediría cambiar la dirección almacenada en el puntero, no el mundo.

El renderer recibe vistas de sólo lectura y escribe únicamente sus propios
buffers. No se reinterpretan objetos de raylib como objetos del motor mediante
casts incompatibles. La plataforma copia colores campo a campo. La excepción
del PCM usa una API de raylib cuyo parámetro `void *` no expresa el préstamo
de sólo lectura; el contrato de nuestra función sí lo expresa.

Evita leer un `float` haciendo cast de su dirección a `int *`: viola las reglas
de acceso mediante tipos incompatibles. Para inspeccionar representación usa
bytes o `memcpy`, y distingue representar bits de convertir un valor.

## Alineación y padding

El compilador puede insertar huecos entre campos de una estructura para alinear
sus miembros. Por eso `sizeof(ReBody)` no tiene que ser la suma ingenua de sus
campos. Tampoco el layout de una estructura es un formato de archivo portable.

No guardamos un mundo con `fwrite(&world, sizeof world, 1, file)`. El mapa usa
campos textuales e identificadores. Los píxeles sí tienen una representación
de cuatro canales byte, comprobada con `static_assert` y `CHAR_BIT == 8`.

Experimento: imprime `sizeof(ReBody)`, `alignof(ReBody)` y `offsetof(ReBody, sector)`.
Consulta `<stddef.h>` para `offsetof`. No añadas `packed` para ahorrar unos bytes
sin comprender sus consecuencias de acceso y ABI.

## Errores y comportamiento indefinido

El compilador puede asumir que no ocurre comportamiento indefinido. No es una
excepción recuperable ni una promesa de crash. Casos frecuentes:

- Leer después de `free` o acceder fuera de un array.
- Desbordar un entero con signo.
- Usar un valor no inicializado.
- Pasar un argumento de tipo incorrecto a `printf`.
- Pasar un `char` negativo a `isspace` sin convertirlo a `unsigned char`.
- Liberar un préstamo que pertenece a otro objeto.

De ahí `-Wconversion`, `-Wformat=2`, capacidades explícitas y validación antes de
dibujar. Los valores de archivo usan `strtol`/`strtof`, comprobando fin de token,
`errno`, finitud y rango. `atoi("error")` no serviría: aparenta un cero válido.

Las tolerancias de flotantes son parte del algoritmo. No compares todos los
resultados con igualdad exacta; tampoco uses un epsilon arbitrariamente grande
para ocultar errores. Los tests usan tolerancias acordes a metros y profundidad.

## Limpieza y carga transaccional

La aplicación inicializa recursos a estados vacíos. Si una fase falla, el bloque
`cleanup` destruye los recursos adquiridos. Un `goto` local de limpieza evita
repetir cinco liberaciones en cada rama; no salta entre funciones ni representa
la lógica del juego.

La carga del mapa crea un candidato temporal. Sólo al validarlo por completo
copia su valor al destino. Un error deja intacto el mundo anterior. Es una
propiedad observable y probada, no simplemente una convención de estilo.

No copies por valor un `ReTexture` propietario y destruyas ambas copias: sus
punteros apuntarían a la misma reserva. `ReWorld`, en cambio, contiene arrays
internos y ningún puntero, por lo que su copia por valor es independiente.

## Qué significa “profesional” en este proyecto

Contratos claros, errores visibles, nombres estables, algoritmos justificables y
pruebas independientes de la pantalla. No significa que el programa sea inmune
a todos los errores ni que disponer de tests sustituya revisar el código.
Las limitaciones y el alcance están escritos para que puedas ampliarlo con criterio.

`tools/check.ps1` comprueba formato, compila con `-fanalyzer` y repite los tests
del núcleo/juego con Clang y UBSan en modo trap. Ese modo detiene el proceso al
detectar una operación instrumentada inválida, sin necesitar un runtime UBSan
externo. No es AddressSanitizer ni demuestra ausencia de todas las fugas o accesos
inválidos; cada herramienta cubre clases de errores diferentes.
