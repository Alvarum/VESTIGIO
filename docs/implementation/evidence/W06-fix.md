# Corrección visual W06 — luces y niebla perceptibles

Estado: corrección implementada; revisión visual del usuario pendiente.

El Atrium declaraba dos luces y niebla, pero el efecto en la demo era demasiado sutil. El modelo glTF compartía solo ocho vértices y no aportaba normales: el importador calculaba normales promediadas en las esquinas de cada cubo. La niebla comenzaba a 8 m y terminaba a 24 m, lejos de buena parte de la geometría visible. Ahora el glTF usa 24 vértices con normales planas por cara, regenerables mediante `python tools/generate-atrium-gltf.py`. El nivel baja la iluminación ambiental, acerca la niebla a 3–14 m y aumenta el alcance/intensidad de las luces cálida y fría. Player y Studio cargan el mismo asset y nivel; no se cambió el shader ni se añadió trabajo de compatibilidad.

## Comparación controlada en GPU real

Player Release de W07 (antes de la animación W08), con el glTF y nivel corregidos copiados a `bin/assets/demo`; NVIDIA GeForce RTX 5060 Ti, OpenGL 3.3. Capturas de 480×270 tras ocho frames, misma posición de cámara `(0.01, -6.57, 1.70)`, mismo perfil Limpio y misma geometría. Sólo cambia el parámetro indicado. Las tres capturas registran 14 draw calls y 168 triángulos:

| Variante | Captura | Diferencia frente a todo activo |
|---|---|---|
| Dos luces + niebla | [Activo](W06-fix-on.png) | Referencia |
| Niebla apagada | [Sin niebla](W06-fix-fog-off.png) | 49.966 píxeles con diferencia RGB máxima >4; diferencias absolutas medias R/G/B 5,65 / 3,39 / 6,04 |
| Intensidad de ambas luces en cero | [Sin luces](W06-fix-lights-off.png) | 57.620 píxeles con diferencia RGB máxima >4; medias 9,98 / 10,38 / 7,63 |

Cada captura solicitada produjo un readback; el render ordinario permanece en GPU. Las imágenes muestran un foco cálido sobre el suelo y los pilares cercanos a la derecha, una contribución azul en el centro y atenuación de los pilares lejanos por niebla. La niebla mezcla el color de la geometría según distancia; **no** es niebla volumétrica ni dibuja haces en el aire.

Reproducción desde la raíz, después de `tools/bootstrap.ps1`:

```powershell
@'
import copy, json, pathlib
source = json.loads(pathlib.Path('assets/demo/atrium.level.json').read_text(encoding='utf-8'))
pathlib.Path('build').mkdir(exist_ok=True)
no_fog = copy.deepcopy(source)
no_fog['environment']['fog']['mode'] = 'none'
pathlib.Path('build/atrium-no-fog.level.json').write_text(json.dumps(no_fog), encoding='utf-8')
no_lights = copy.deepcopy(source)
for entity in no_lights['entities']:
    light = entity.get('components', {}).get('engine.light')
    if light is not None:
        light['intensity'] = 0
pathlib.Path('build/atrium-no-lights.level.json').write_text(json.dumps(no_lights), encoding='utf-8')
'@ | python -
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1 -Smoke 8 -NoAudio -Capture build/atrium-on.png
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1 -Smoke 8 -NoAudio -Level build/atrium-no-fog.level.json -Capture build/atrium-no-fog.png
powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-3d-demo.ps1 -Smoke 8 -NoAudio -Level build/atrium-no-lights.level.json -Capture build/atrium-no-lights.png
```

Las imágenes de evidencia se midieron con Pillow mediante `ImageChops.difference` y `ImageStat.Stat`; se contó cada píxel cuya diferencia máxima entre canales supera 4. La comparación entre variantes separa el efecto de luces del de niebla, que el test anterior sólo comprobaba como parámetros y no como resultado perceptible. Si se repite con W08, las dos piezas animadas elevan draws/triángulos y cambian ligeramente los recuentos de píxeles; la comparación controlada sigue siendo válida.
