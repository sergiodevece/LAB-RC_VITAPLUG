# LAB-RC 0.2.0 — Final Touch: UI y composición

Fecha: 26 de noviembre de 2026.

**Estado visual: aprobado explícitamente por el propietario.** El lead ha comunicado su confirmación «ok,validadads» inmediatamente después de presentar las tres capturas finales. Esta aprobación corresponde al acabado y la composición de 0.2.0; no convierte las capturas históricas de 0.1.0 en una referencia aprobada ni sustituye el smoke sonoro/funcional de 0.2.0 en Logic.

## Referencia y alcance

Se leyeron íntegros la especificación y el plan Final Touch y el documento original del paquete; se inspeccionaron visualmente la referencia única y los cuatro PNG suministrados. Referencia vinculante: `/Users/serx/Downloads/LAB-RC_Final_Touch_02/LAB-RC_REFERENCIA_VINTAGE_APROBADA.png`. El HTML v01 y la UI anterior se consideran históricos y no se usan como referencia estética.

El acabado combina metal verde oscuro envejecido, pátina, arañazos y pérdidas de pintura irregulares, biseles profundos, tornillos Phillips, serigrafía crema, knobs metálicos con cepillado y luz anisotrópica, faders con ranura profunda y cap estriado, y nueve knobs negros de baquelita estriada. Los controles y los medidores siguen siendo componentes dinámicos. No se utiliza la captura aprobada completa como fondo ni su escena ilustrativa.

La geometría y el material se dibujan nativamente. Las superficies estáticas se almacenan en cachés de pintura adaptadas al factor físico de píxel, incluidos los textos estáticos, sin congelar mandos, lecturas, estados ni medidores. La referencia completa no forma parte de BinaryData.

## Recursos y procedencia

`Assets/FinalTouch/manifest.json` registra rutas originales, SHA-256, dimensiones, modo, alfa y procedencia de los detalles recortados. Verificación final: cuatro copias byte a byte de los originales y dos detalles estáticos con sus hashes coincidentes.

| Archivo | Dimensiones | Uso |
|---|---:|---|
| `kentucky_clean.png` | 1448×1086, RGB | Fondo original sin batería, opaco e intacto |
| `center_field_clean.png` | 1671×941, RGB | Fondo original sin batería, opaco e intacto |
| `live_room_clean.png` | 1448×1086, RGB | Fondo original sin batería, opaco e intacto |
| `boost_this.png` | 1448×1086, RGBA | Única batería autorizada; alfa real 0…255 |
| `approved_metal.png` | 100×78 | Material vacío de la referencia, recorte (183,802)–(283,880) |
| `approved_screw.png` | 32×32 | Cabeza inerte de tornillo, recorte (7,7)–(39,39), máscara circular |

Los dos recortes se autorizaron expresamente como detalles estáticos. No contienen texto, lectura, medidor ni mando operativo congelado. Ningún original se sobrescribió ni se regeneró una sala o batería. El loader de matrices IR permanece sin cambios; Resources añade exclusivamente los nuevos recursos gráficos.

## Escenas por capas

`RoomScene` dibuja el fondo original mediante contain. El espacio lateral pertenece al marco oscuro integrado y no simula una extensión física de la sala. La imagen no se recorta, estira ni recolorea.

`DrumKitLayer` es un componente independiente, sin interacción. Usa únicamente el PNG BOOST THIS autorizado. Posición y escala aproximan la batería del kit anterior: Kentucky ocupa un 31% del ancho de la foto; Center Field, 19.5%; Live Room, 29%. Las alturas mantienen la relación de aspecto de la batería. Un shear afín de aproximadamente 1–2%, una sombra de contacto suave y luz cálida/neutra/fría por sala integran la capa en el suelo existente. Center Field conserva su alfombra y suelo originales. La iluminación solo modifica copias de render de la batería en memoria; el PNG fuente y los fondos quedan intactos.

Se conservan **exactamente** las seis coordenadas normalizadas anteriores de EARS/FRONT/FLOOR L/R. Los overlays permanecen relativos al rectángulo efectivo contain del fondo, no al marco completo. No se arrastran ni cambian la geometría acústica. MICS solo afecta a esos seis marcadores. La capa de batería y sus copias de iluminación son recursos visuales, no procesamiento de audio.

## EQ, controles y tamaños

Cada módulo inferior contiene identificación/M/S, LOW/MID/HIGH, fader y medidor. EQ IN tiene estado real. Los IDs son los doce de la especificación: `earsEqLowDb/earsEqMidDb/earsEqHighDb/earsEqEnabled`, y equivalentes front/floor. Cada ganancia tiene lectura editable en dB, rango del parámetro, doble clic al default, drag fino, teclado, rueda con foco y centro 0 visible. Los nueve knobs son de baquelita, sin mandos secundarios duplicados.

Se mantiene polling de los valores atómicos a 30 Hz, sin listeners GUI ni ParameterAttachment que publiquen mensajes desde automatización. Los cambios de usuario conservan begin/set/end gesture y la API segura de undo/redo. La edición numérica abierta se respeta durante el refresco; números inválidos no cambian parámetros. El cero se canonicaliza a las décimas visibles antes de formar el signo, por lo que el roundoff del intervalo del slider no produce `+0.0 dB`. No se cambia el valor DSP para arreglar su representación.

| Tamaño | Dimensiones |
|---|---:|
| Mínimo | 1024×684 |
| Inicial | 1280×854 |
| Máximo/base | 1586×1058 |
| Captura inicial HiDPI | 2560×1708 |

La proporción es 1586/1058. La escena tiene 350 px de alto de referencia y los módulos inferiores 370 px, para colocar las tres bandas completas sin microtexto. Al mínimo, los knobs EQ tienen hit area de al menos 36 px y los valores al menos 60×22 px; las aserciones verifican que los extremos ±12.0 dB caben. El mínimo cabe en una pantalla 1366×768. DSP/Processor implementa la migración acordada mediante `editorLayoutVersion=2`: tamaño antiguo sin ese atributo → ancho escalado por 1280/1180, alto recalculado y clamp; el tamaño de 0.2 se conserva tras recall. El editor aplica el tamaño restaurado con guard para no reescribirlo, y un resize manual posterior prevalece.

Los VUs reservan el ancho medido de las etiquetas completas. Se corrigió la elipsis de -12/-24/-48 en EARS/FRONT, se aplica el mismo cálculo a todos los pares y medidores exteriores, y OUTPUT dispone de altura suficiente. Las marcas coinciden con la escala dBFS. Meters en silencio, SOLO/MUTE y EQ IN reflejan sus estados reales; las luces ilustrativas de la referencia no determinan el DSP.

## Evidencia de verificación

- Geometría/composición portable: **6 fallos RED → 0 GREEN**, con fixtures de contain, límites del módulo EQ y colocación contextual de la batería. También se conservan las anclas/inversa de fader y las coordenadas de marcadores.
- UI inicial real: **15 fallos RED**, doce controles EQ aún ausentes y tres expectativas geométricas de 0.1 retiradas por los nuevos tamaños aprobados. Log: `docs/reports/final-touch-ui-red.log`. No se relajan tolerancias de audio.
- Cero visible: **9 fallos RED → GREEN**, uno por lectura EQ. Evidencia: `build/evidence/final-touch-zero-red.log`.
- Suite UI final: **15 secciones, 2366 checks PASS, 0 FAIL**. Comando: `build/mac-arm64/tests/labrc_tests --category UI`. Log: `build/evidence/final-touch-ui-final.log`.
- Cobertura nueva: nueve valores editables y tres EQ IN reales; undo; los doce parámetros EQ desde worker y sincronización por GUI sin acciones de usuario; hit-testing y extremos numéricos al mínimo; cuatro reaperturas consecutivas con EQ, MICS y tamaño conservados. Se mantienen los tests de faders, controles globales, M/S, room, edición, automatización previa, recall y safe undo.
- Compilación final de pruebas y renderer: exit 0, sin warnings/errores. Log: `build/evidence/final-touch-ui-final-build.log`.
- Suite completa arm64: **5/5 PASS**, DSP, Processor, UI, BuildTools y Bootstrap; 12.90 s. Comando: `ctest --test-dir build/mac-arm64 --output-on-failure`. Log: `build/evidence/final-touch-ui-ctest.log`.

## Capturas y revisión

El editor compilado produjo trece PNG reales en `build/evidence/final-touch-screenshots`: cada sala en mínimo/inicial/máximo e inicial @2x, más `controls-detail.png`. Log: `build/evidence/final-touch-render.log`. Se inspeccionaron las tres escenas, los mínimos, las etiquetas VU y el detalle; la terna final se presentó al propietario y recibió aprobación explícita.

El lead solicitó corregir las elipsis VU y el cero visible y suavizar los desconchados demasiado rectangulares. Los tres ajustes se aplicaron y las capturas se regeneraron; no se alteraron fondo, posiciones de micro ni sonido. Las capturas proceden del renderer JUCE, no equivalen a una prueba física de monitor HiDPI o de host.

Las fuentes UI/Resources quedaron congeladas y el árbol arm64 se devolvió a Build para wrappers universales y paquete. Sin commits, publicación, instalación ni cambios de DSP por este agente. **Pendiente por separado:** smoke humano de la nueva versión 0.2.0 en Logic y validación del AU/paquete por Build; la aprobación visual ya está cerrada.
