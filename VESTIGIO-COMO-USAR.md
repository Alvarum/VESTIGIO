# VESTIGIO: abrir, compilar y crear un nivel 3D

Ejecuta los comandos desde la raíz de este repositorio en PowerShell. VESTIGIO
es el motor 3D; sus fuentes están en `engines/vestigio/`.

## Abrir Studio

Si ya está compilado, abre con doble clic
`build/vestigio/debug/bin/vestigio_studio.exe`, o ejecuta:

```powershell
./build/vestigio/debug/bin/vestigio_studio.exe
```

La pantalla inicial ofrece **Nuevo nivel**, **Abrir nivel** y **Ver Atrium de
ejemplo**. Nuevo crea un documento con un inicio del jugador y sin objetos del
Atrium. El ejemplo está protegido: para conservar sus ediciones, guarda una copia.

Para compilar y abrir con un solo comando:

```powershell
./tools/run-studio-3d.ps1
```

También puedes abrir directamente un archivo:

```powershell
./build/vestigio/debug/bin/vestigio_studio.exe --level "C:\mis-juegos\nivel.level.json"
```

## Crear tu primer nivel

1. Elige **Nuevo nivel**, escribe su nombre y pulsa **Crear nivel**.
2. Pulsa **＋ Habitación**. En **Construir**, establece origen X/Y, ancho y
   largo, y pulsa **Trazar rectángulo**. Planta Z, alto y grosor se expresan en metros.
3. Para una abertura, elige arista, tipo, inicio, ancho y alto, y pulsa
   **Añadir abertura**. La arista 0 une los primeros dos vértices del contorno;
   el inicio se mide desde el primero. El contorno avanzado permite otras formas.
   Las aberturas son huecos de la geometría; no crean una puerta interactiva.
4. Usa **Vista previa**, **Cancelar** o **Crear habitación**, siempre al pie del
   constructor. La vista previa no guarda ni añade pasos al historial.
5. Pulsa **Importar modelo…** y elige un GLB/glTF. En **Recursos**, selecciona
   el modelo y pulsa **Colocar**. **Vista previa** es opcional; **Volver al nivel**
   cierra esa vista temporal.
6. Selecciona el objeto en **Escena** o en el viewport. Cambia posición en el
   **Inspector** y pulsa **Aplicar transformación**. **Rotación y escala**
   despliega los demás campos. Las unidades son metros y grados.
7. Pulsa **Guardar** y elige una ubicación. Studio guarda el `.level.json` y
   copia los modelos importados a los recursos del destino. Conserva ambos
   juntos si mueves o compartes el nivel.
8. Cierra Studio, ábrelo otra vez y usa **Abrir nivel**. Selecciona tu archivo.
   Comprueba habitación, abertura y modelo, y pulsa **Probar**.
9. Pulsa **Detener** para volver a editar. **Deshacer/Rehacer** también funciona
   después de la prueba; las acciones realizadas al jugar no alteran el documento.

**＋ Nueva habitación** abre una receta nueva. Seleccionar una habitación y
**Editar habitación seleccionada** permite modificar su receta. El **Inicio del
jugador** de la jerarquía fija dónde apareces al jugar: puedes editar posición y
rotación. Mover la cámara de edición no cambia ese inicio.

## Orientarte en el editor

- **Escena**, a la izquierda: jerarquía y selección. Ctrl/Mayús permite varios objetos.
- **Viewport GPU**, al centro: vista libre, órbita u ortográfica; Encuadrar selección.
- **Inspector**, a la derecha: propiedades de lo seleccionado; sin selección,
  ofrece acciones para empezar. **Construir** contiene habitaciones; **Nivel**
  contiene el perfil Limpio/Retro y volumen.
- **Recursos / Problemas**, abajo: modelos del nivel y diagnósticos. Importar
  no coloca automáticamente el modelo. Seleccionarlo tampoco cambia la escena.
- Arrastra los separadores para ajustar los paneles. **Ver → Restablecer paneles**
  recupera sus tamaños. En ventanas pequeñas algunos grupos se pliegan y los
  inspectores tienen scroll; Crear/Cancelar y Probar/Detener mantienen su sitio.

| Control | Acción |
|---|---|
| Clic en viewport / jerarquía | Seleccionar |
| Botón derecho + ratón | Mirar; enfoca la vista |
| WASD con viewport enfocado | Mover la cámara o el jugador |
| Espacio / Ctrl en Editar | Subir / bajar |
| Espacio en Probar | Saltar |
| F | Encuadrar selección |
| F5 | Probar / Detener |
| Escape | Cancelar gesto o vista previa de habitación; detener si estás en Probar |
| Ctrl+N / Ctrl+O | Nuevo / Abrir |
| Ctrl+S / Ctrl+Mayús+S | Guardar / Guardar como |
| Ctrl+Z / Ctrl+Y | Deshacer / Rehacer; dentro de un campo, edición del texto |
| Ctrl+D / Supr | Duplicar / Borrar selección |

**Guardar** actualiza tu archivo; **Archivo → Guardar como** elige otro destino.
Nuevo/Abrir/Cerrar pregunta ante cambios pendientes: **Sí** guarda, **No** descarta
y **Cancelar** mantiene el documento. Cancelar la elección de ruta también
cancela el cambio de documento. Un archivo inválido informa el problema y
conserva el nivel activo. Los niveles aún sin ubicación usan una carpeta de
respaldo en `%LOCALAPPDATA%\VESTIGIO\Unsaved`.

## Abrir Player o la demo

```powershell
./tools/run-3d-demo.ps1
./tools/run-3d-demo.ps1 -Level "C:\mis-juegos\nivel.level.json"
```

Player usa WASD, ratón, Espacio para saltar y Escape para cerrar. En el Atrium,
E abre/cierra la puerta. Su animación y ambiente son efectos del ejemplo; no
se añaden a los niveles nuevos. Para una captura que se cierra sola:

```powershell
./tools/run-3d-demo.ps1 -Smoke 8 -Capture build/atrium.png
```

## Compilar y ejecutar las pruebas

Requisitos: Windows, .NET SDK 10 y controlador con OpenGL 3.3. La preparación
descarga el toolchain MSYS2 y las dependencias en las carpetas del repositorio.
Si ya existen `.tools/` y las dependencias restauradas, no repitas bootstrap.

```powershell
./tools/bootstrap.ps1
./tools/build.ps1 -Preset debug -Test
./tools/build.ps1 -Preset release -Test
```

Los binarios quedan en `build/vestigio/debug/bin/` y
`build/vestigio/release/bin/`. Los archivos personales de imagen y volumen están
en `%LOCALAPPDATA%\VESTIGIO\visual.settings`. Usa `--settings "ruta.settings"`
y `--no-audio` si necesitas preferencias separadas o desactivar el sonido.

La evidencia de UX01 está en [docs/implementation/evidence/UX01.md](docs/implementation/evidence/UX01.md).
La comprobación automática no sustituye la revisión visual e interactiva del usuario.
