# Atrium 3D — primera escena nueva de VESTIGIO

Desde la raíz del repositorio, después de preparar las herramientas con
`./tools/bootstrap.ps1`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1
```

La opción de ejecución afecta sólo a ese proceso de PowerShell.

WASD mueve la cámara; el ratón gira la vista; Espacio salta y Escape o cerrar la
ventana termina la demo. F3 alterna los colliders reales. El jugador tiene un
volumen con colisión de suelo, objetos, techo y escalón. Es una escena de prueba
**sin armas ni editor**.
El suelo, monumento, escalón y pilares usan instancias del modelo glTF
`atrium.gltf`, creado para este ejemplo y cargado mediante el SDK C. El frame
final se dibuja en GPU. El archivo glTF contiene geometría y materiales
procedurales originales, sin recursos artísticos de terceros.

Prueba reproducible y captura puntual:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1 -Smoke 8 -Capture build/atrium.png
```

Para capturar las formas de colisión visibles:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1 -Smoke 8 -ShowColliders -Capture build/atrium-colliders.png
```

El smoke inyecta avance y un giro pequeño de cámara, cambia el tamaño de la
ventana a mitad del recorrido y exige draw calls, upload GPU y desplazamiento.
La captura es el único readback de la ruta. Se puede ejecutar el binario desde
otro directorio: resuelve `assets/demo/atrium.gltf` junto al ejecutable. Si falta
ese asset, imprime su ruta y termina con código distinto de cero.

El volumen del jugador es una unión de esferas muy solapadas que aproxima una
cápsula; los límites admitidos están en `include/vestigio/controller.h`.
Las plataformas de esta escena son estáticas. No hay simulación de cuerpos
rígidos ni navegación con navmesh.
