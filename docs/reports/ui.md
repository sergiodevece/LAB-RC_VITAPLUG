# LAB-RC — informe UI

Fecha: 2026-11-25. Fase UI local. La aprobación estética del propietario sigue **PENDIENTE**; las pruebas de software y las capturas no la sustituyen.

## Fuentes y recursos

Leídos íntegros MASTER_PROMPT y prototype_v01.html, omitiendo únicamente la impresión del base64. Inspección visual de approved_vintage.png y de las tres fotografías originales. Referencia 1586×992; tamaños 800×500, 1180×738 y 1586×992. Kentucky y Live Room: 1448×1086. Center Field: 1671×941.

El editor no incluye ni carga la referencia completa ni los JPEG recodificados del HTML. Resources entrega únicamente las fotografías PNG originales empaquetadas. Se conservan batería BOOST THIS, geometría y encuadre completos, sin personas, sin recoloración, sin recorte y sin estiramiento. `fitContain` calcula un rectángulo centrado; la llamada de dibujo utiliza ese rectángulo con la misma proporción que el original.

Seis componentes independientes, EARS L/R, FRONT L/R, FLOOR L/R, se colocan sobre el rectángulo efectivo de fotografía. Se mantienen exactamente las coordenadas normalizadas del HTML por sala. No se dibujan micrófonos. Pulsar un marcador solo resalta visualmente su pareja; MICS solo cambia la preferencia de overlays. Los componentes no se destruyen al cambiar sala.

## Implementación

- `Source/UI/EditorGeometry.h`: layout proporcional, contain y coordenadas normalizadas puras, sin dependencia de DSP.
- `Source/UI/ConsoleFaderScale.h`: interpolación monótona y su inversa; anclas t=0/.15/.36/.65/.86/1 para −60/−43/−27/−10/0/+6 dB.
- `Source/UI/VintageComponents.*`: chasis y paneles de metal verde oliva oscuro, pátina determinista, desgaste en bordes, biseles y tornillos; knobs con estrías concéntricas e indicador oscuro; faders con ranura profunda y cap metálico estriado; botones con estados normal/hover/pressed/activo/desactivado y foco.
- `Source/PluginEditor.*`: composición, selector, parámetros, reset, undo/redo, persistencia de tamaño, teclas y refresco a 30 Hz.

Todos los controles son componentes nativos móviles; no hay controles congelados en una captura ni sliders secundarios que dupliquen knobs. Fuentes del sistema, sin empaquetar archivos de fuente. El tema sigue verde oliva oscuro, metal cálido y tipografía crema de la referencia. Es una interpretación nativa revisable, no una afirmación de igualdad de píxel con la imagen artística.

INPUT/OUTPUT/MIX y faders admiten drag, Shift para movimiento fino, teclado, rueda solo con foco y doble clic al default. Valores numéricos editables con clamp al rango real; números malformados/no finitos se rechazan conservando el último valor. Las cifras usan fuente monoespaciada y bounds fijos. Hit areas pequeñas se amplían a un mínimo práctico de 28 px cuando cabe, sin reordenar el layout. Se conserva el foco de los marcadores al cambiar sala.

## Parámetros y undo

IDs APVTS exactos. La UI no registra `ParameterAttachment` ni listeners de parámetro. Se leen los valores reales mediante punteros atómicos APVTS desde el timer de GUI a 30 Hz; los controles se actualizan con `dontSendNotification`. Los gestos del usuario invocan exclusivamente `beginParameterGesture(id,name)`, `setParameterFromUser(id,actual)` y `endParameterGesture(id)`. La automatización externa no entra en ese camino ni añade acciones de undo.

Room conserva los faders globales. Mute prevalece sobre solo; varios solos simultáneos. M/S solo se iluminan con sus parámetros activos. RESET llama a la transacción única del processor, incluida MICS=true, sin restablecer el tamaño. UNDO/REDO y Command/Ctrl-Z/Shift-Z usan las APIs protegidas `canUndoUser/canRedoUser/undoUser/redoUser`, sin acceso directo del editor al UndoManager. Preferencias de tamaño y mics permanecen por instancia.

El timer obtiene una sola lectura de `meterSnapshot` por tick. Cinco medidores estéreo muestran los picos acumulados entregados por DSP, decay visual y clip hold a ≥1.0 lineal. Los segmentos están apagados en silencio. No hay lectura bloqueante ni repaint desde el callback de audio. Rótulo neutro `PEAK - dBFS`, sin presentar un estado IDLE inventado con audio activo.

## Evidencia TDD y comprobaciones

1. Tests portables escritos antes de la implementación de geometría. RED observado: 45 fallos frente a stubs y escala lineal, incluidos anclas, contain y bounds. GREEN: `clang++ -std=c++17 -Wall -Wextra -pedantic -DLABRC_UI_STANDALONE_TEST Tests/UITests.cpp -o /private/tmp/labrc-ui-tests && /private/tmp/labrc-ui-tests` → 0 fallos.
2. Suite UI JUCE: anclas/inversa en 1001 posiciones, fixtures literales de contain en ambos ejes, todos los paneles dentro de min/default/max, marcadores relativos a fotografía; hit-test real de INPUT/OUTPUT/MIX; edición numérica y undo único; sala con faders globales y seis componentes estables; MICS sin efectos de parámetro DSP; actualización host sin undo.
3. Primer build completo de las fuentes UI: exit 0, sin warnings/errores en `build/evidence/ui-build.log`.
4. Tras Processor GREEN, RED específico de entrada malformada: 2180 checks PASS y 1 FAIL, el texto `3-2 dB` se interpretaba como 3 dB. Evidencia `build/evidence/ui-red-numeric.log`. Corregido con validación del número completo, sufijo apropiado y valor finito. GREEN posterior de la categoría UI.
5. QA fix 1, observación de parámetros: la revisión de JUCE confirmó `ParameterAttachment::parameterValueChanged` → `triggerAsyncUpdate` desde el hilo de automatización. RED observado: 2197 PASS/9 FAIL en `build/evidence/ui-polling-red.log`. Se retiraron los siete attachments y sus listeners, sustituidos por polling atómico desde GUI. Ningún cambio del host llama un callback de componente UI. Regresión con worker que automatiza los catorce parámetros, espera de un frame GUI real, actualización de seis valores/sala/M/S/bypass y ausencia de undo de usuario. No se afirma disponer de un detector portable de posts a la cola del sistema ni de eliminar los locks internos de la implementación de parámetros de JUCE; el arreglo retira el fan-out de listeners UI añadido por la aplicación.
6. QA edición: confirmado que `Label::setText` llama `hideEditor(true)` incluso con el mismo texto. Ahora no se toca el readout mientras está en edición y solo se escribe cuando cambia su texto. RED de edición perdida observado; GREEN de edición durante timer/automatización, cancelación hacia el valor del host y confirmación como una sola acción deshacible.
7. QA recall y marcadores: RED extendido 2216 PASS/14 FAIL en `build/evidence/ui-recall-red.log`, incluyendo bounds de un marcador, tamaño de editor abierto y undo inmediato de una sesión anterior. El timer aplica el tamaño restaurado con `restoringSize`, y una posterior acción manual de resize prevalece. Botones/teclas de undo usan la API segura del processor. FRONT/FLOOR tienen ancho adaptativo y fuente ajustada, sin elipsis, conservando L/R y centros normalizados exactos; sus hit areas se limitan al rectángulo de fotografía. No cambia la imagen ni su geometría.
8. GREEN integrado: `build/mac-arm64/tests/labrc_tests --category UI` → **11 secciones, 2231 checks PASS, 0 FAIL**, `build/evidence/ui-polling-green.log`. Se reemplazó un sleep de fixture por un FrameProbe Timer con espera acotada a 750 ms para comprobar un frame despachado realmente.
9. Suite completa posterior: `ctest --test-dir build/mac-arm64 --output-on-failure` → **5/5 PASS**, DSP/Processor/UI/BuildTools/Bootstrap, `build/evidence/ui-final-ctest.log`.

## Capturas y revisión visual

El renderer nativo genera doce PNG reales en `build/evidence/screenshots`: cada sala a 800×500, 1180×738, 1586×992 y 2360×1476 (default @2x). No utiliza un host ni device de audio. Comando: `build/mac-arm64/tools/labrc_render_ui --output <ruta absoluta>/build/evidence/screenshots`. Log `build/evidence/ui-render-final.log`.

Se han inspeccionado visualmente las doce capturas: tres salas en mínimo/default/máximo y @2x. Fotografías completas contain, batería BOOST THIS conservada, seis etiquetas con L/R completos, metering oscuro en silencio, M/S apagados inicialmente y ningún texto/control cortado observado. A mínimo, la etiqueta FLOOR L de Center Field usa ajuste horizontal para respetar su posición cercana al borde sin salir de la fotografía. No se altera el centro normalizado ni el encuadre.

Tras revisión del lead se añadió pitting/desgaste de bordes y se sustituyeron anillos concéntricos gruesos por cepillado radial fino con luz diagonal en los knobs. El resizer adopta metal oscuro/dorado del chasis. Se conservan el layout y los recursos; no hay un estilo alternativo ni una escena recreada. Compilación final `build/evidence/ui-final-build.log` sin warnings/errores; suite completa final 5/5 PASS en 15.03 s. Las capturas son evidencia del renderer, no validación de un host/monitor físico HiDPI.

## Pendiente

Decisión posterior del propietario: distribución funcional correcta, acabado gráfico **no aprobado** por conservar rasgos del prototipo descartado. UI congelada sin más modificaciones estéticas en esta fase; se prioriza el AU instalable y su smoke en Logic. Diseño industrial definitivo y EQ de tres bandas por pareja se reservan para una segunda iteración posterior a la aprobación sonora/funcional.

- Aprobación estética del propietario en la segunda iteración; no se declara aprobada esta implementación gráfica.
- Prueba humana de interacción, tipografía/foco, accesibilidad, resize/recall en Logic y aceptación estética por el propietario.

No se declara cerrado Gate 2 ni validación humana por disponer de tests o imágenes. No se han modificado WAV/PNG, processor, DSP, CMake ni otros archivos fuera de la propiedad UI; no se han hecho commits, publicación o instalación.
