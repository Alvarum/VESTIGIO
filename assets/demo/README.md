# Atrium 3D — primera escena nueva de VESTIGIO

Desde la raíz del repositorio, después de preparar las herramientas con
`./tools/bootstrap.ps1`:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1
```

La opción de ejecución afecta sólo a ese proceso de PowerShell.

WASD mueve la cámara sobre el plano; el ratón gira la vista; Escape o cerrar la
ventana termina la demo. Es recorrido libre **sin colisiones, salto, armas ni
editor**. El suelo, monumento y pilares usan instancias del modelo glTF
`atrium.gltf`, creado para este ejemplo y cargado mediante el SDK C. El frame
final se dibuja en GPU. El archivo glTF contiene geometría y materiales
procedurales originales, sin recursos artísticos de terceros.

Prueba reproducible y captura puntual:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1 -Smoke 8 -Capture build/atrium.png
```

El smoke inyecta avance y un giro pequeño de cámara, cambia el tamaño de la
ventana a mitad del recorrido y exige draw calls, upload GPU y desplazamiento.
La captura es el único readback de la ruta. Se puede ejecutar el binario desde
otro directorio: resuelve `assets/demo/atrium.gltf` junto al ejecutable. Si falta
ese asset, imprime su ruta y termina con código distinto de cero.
