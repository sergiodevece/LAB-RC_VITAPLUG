# Regresión acústica entre LAB-RC 0.1.0 y Final Touch

**Resultado acústico.** A las seis tasas, 0.2.0 con EQ OUT y con EQ IN plana produjo exactamente el mismo stream que el checkpoint 0.1.0. En cada comparación, los 213 520 746 floats coincidieron bit por bit. La escucha y el smoke de 0.2.0 en Logic requieren al propietario.

## Referencia local

La referencia se construye desde el contenido de `git archive 31cf43a0d4b2f04b57952639d34449209d025912`, extraído en `build/baseline/source`. Usa la copia fijada de JUCE, Apple clang 21, Release arm64, macOS 26.5, deployment 11.0 y Accelerate. El árbol histórico, la biblioteca de código compartido y el ejecutable de medida tienen rutas distintas de las de 0.2.0. El AU aprobado por el propietario se conserva en `checkpoints/0.1.0/`; este ensayo controla el AudioProcessor real fuera del host.

En `build/baseline/legacy-state.bin` se conserva un estado serializado por 0.1.0 con sala, trims, mezcla, tres faders, solo, mute, MICS y tamaño. SHA-256 `281e19d58ea27403ba22d89a8b4212c5c14dbd5429a3d6e33fb9788286d8c7fa`. Se restaura literalmente en ambas versiones después de la preparación y antes del primer callback. El harness comprueba los valores esperados. En la candidata comprueba también los defaults de las doce entradas EQ antes de probar el modo elegido.

## Protocolo de comparación

- Tasas de 44,1, 48, 88,2, 96, 176,4 y 192 kHz. Preparación para 256 muestras y secuencia de bloques de 1, 32, 64, 127, 128, 256, 512, 1024, 2048 y 2801. Se comprueba una latencia declarada de 128 muestras en cada preparación.
- Cada pareja de las tres salas se mide a 0 dB, sola, MIX 100 %, con cuatro entradas independientes. Se utilizan impulsos L y R, un impulso de entrada mono real cuyo segundo canal de buffer se rellena deliberadamente con una señal espuria, y un burst estéreo determinista de 4096 muestras. Se guardan 4,5 s de cola y 256 muestras adicionales tras la latencia. No se aportó un archivo de batería autorizado.
- Otra secuencia por tasa mide el recall antiguo seguido de mezcla parcial, dry, wet, dos solos, mute prioritario, bypass de parámetro, bypass de host y reactivación. Usa ruido estéreo continuo reproducible mediante hash entero del índice de muestra y canal. Los cambios de control se hacen en posiciones de muestra fijas.
- La secuencia de salas Kentucky → Center Field → Live Room → Kentucky se ejecuta dos veces por tasa. Entre peticiones se estaciona la llamada de prueba durante 750 ms sin avanzar muestras, para que el trabajador publique el banco antes del siguiente bloque. Se exigen hashes iguales entre repeticiones. Este experimento compara las transiciones y la cola con bancos listos en los mismos bloques. Durante audio continuo, el instante en que puede comenzar una transición sigue dependiendo de la preparación asíncrona.
- `samples.f32` guarda estéreo float32 intercalado en little endian. `segments.tsv` identifica tasa, estímulo, duración, latencia y offset de los 234 segmentos. `manifest.json` contiene SHA-256 del stream, de cada segmento, del harness y del estado, junto con el tamaño. Se compara el patrón de bits de cada muestra. Si difiere, se reportan cantidad, residual máximo y RMS en escala lineal sin reajustar niveles ni compensar temporalmente las señales. Se realizaron dos comparaciones de 0.2.0, con EQ OFF y con las tres EQ IN planas en régimen estable.
- El benchmark arranca un proceso por configuración. Una instancia en Live Room procesa seis segundos de ruido estéreo continuo a 48/96/192 kHz con bloques de 64 y 256. Se cronometra solo `processBlock`. `performance.tsv` contiene media/p95/p99/máximo, callbacks fuera de plazo y RSS final del proceso. El RSS incluye arnés, recursos y asignador. El tiempo de pared refleja carga vecina y planificación; una muestra de seis segundos no establece un presupuesto garantizado en Logic.

## Reproducción

Se necesitan los builds locales arm64 con `BUILD_TESTING=ON`, CMake Unix Makefiles y target `LabRC` ya compilado. El script usa las opciones del compilador de ese árbol y su biblioteca compartida estática. Los comandos de captura exigen directorios nuevos para preservar la referencia.

```sh
python3 -B scripts/final_touch_regression.py capture build/baseline/arm64 build/baseline/renders build/baseline/legacy-state.bin --label 0.1.0-31cf43a --make-legacy
python3 -B scripts/final_touch_regression.py capture build/mac-arm64 build/final-touch-zero-off build/baseline/legacy-state.bin --label 0.2.0-eq-off
python3 -B scripts/final_touch_regression.py capture build/mac-arm64 build/final-touch-zero-flat build/baseline/legacy-state.bin --label 0.2.0-eq-flat --mode flat
python3 -B scripts/final_touch_regression.py compare build/baseline/renders build/final-touch-zero-off docs/reports/final-touch-null-off.json
python3 -B scripts/final_touch_regression.py compare build/baseline/renders build/final-touch-zero-flat docs/reports/final-touch-null-flat.json
```

El directorio `build/baseline` contiene `configure.log`, `compile.log` y `render.log`. Los resultados de las dos comparaciones figuran más abajo. `final-touch-eq.md` documenta los ensayos de EQ activa.

### Resultado de la referencia 0.1.0

Captura terminada con código 0: **234 segmentos**, 106 760 373 pares de muestras, **854 082 984 bytes**. SHA-256 del stream completo `878abb021bae7000a770d69ff0bf68845c19120c95a6e8d754f475a3961d1fe4`. Los cambios de sala repetidos dieron el mismo hash en las seis tasas. El harness usado tiene SHA-256 `09a97114033f80498090d2404ce21b4f4ff405c7354b9721b6d6a1a4cca3713a`. El fichero de muestras y los hashes individuales permanecen en `build/baseline/renders/`. Latencia de 128 muestras en todas las preparaciones.

| Fs | Bloque | Media µs | p95 µs | p99 µs | Máximo µs | Excesos/callbacks | RSS MiB |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 48000 | 64 | 11,09 | 28,63 | 38,04 | 97,58 | 0/4500 | 74,95 |
| 48000 | 256 | 46,01 | 69,54 | 79,33 | 105,33 | 0/1125 | 74,92 |
| 96000 | 64 | 17,17 | 41,54 | 54,58 | 104,08 | 0/9000 | 100,94 |
| 96000 | 256 | 67,91 | 92,71 | 115,79 | 229,75 | 0/2250 | 101,03 |
| 192000 | 64 | 34,17 | 82,92 | 112,67 | 602,88 | 1/18000 | 173,91 |
| 192000 | 256 | 126,54 | 155,17 | 197,08 | 386,96 | 0/4500 | 173,84 |

### Primera candidata integrada: defecto de cero en APVTS

Se comprobó que la biblioteca enlazada identificaba versión 0.2.0, el ID `earsEqLowDb` y los recursos `*_clean_png` y `boost_this_png`. Las dos capturas se detuvieron al verificar el primer default. La biblioteca JUCE local, con el rango decimal original `[-12, +12]` y paso `0.1f`, devolvía **+1,78813934326 × 10⁻⁷ dB** al construir el processor, al recibir el estado antiguo y después de automatizar +12 dB → 0 dB. El integrador de la EQ emplea una ruta de identidad cuando el valor es exactamente cero.

Una sonda separada del processor aisló EARS, aplicó el mismo impulso en 96 kHz a dos instancias y comparó EQ OUT con EQ IN tras devolver las tres bandas a cero mediante automatización. En 256 000 muestras escalares hubo **41 172 patrones de bits diferentes**, residual máximo **3,72529029846 × 10⁻⁹**. Este resultado explica el rechazo del candidato por el control de defaults e identidad matemática. No se modificó la tolerancia del ensayo. Registros originales: `build/evidence/final-touch/qa-capture-off.log`, `qa-capture-flat.log` y `build/final-touch-probes/processor/result.log`.

### Verificación del arreglo de cero e integración DSP

El cambio se limita al redondeo a décimas alrededor de cero en los nueve parámetros de ganancia EQ. El paso declarado al host continúa en 0,1 dB. La misma sonda, recompilada y enlazada al processor corregido, obtuvo **0 dB exactos** al construir, restaurar el estado 0.1 y automatizar +12 → 0. El render aislado de control pasó de 41 172 diferencias a **0/256 000**, pico residual 0. La sonda y resultado de la revisión inicial se conservaron. La regresión integrada incluye comprobaciones de cero exacto desde el host y la API de edición.

La comparación de fuentes contra `31cf43a` conserva `Source/DSP/MatrixBank.{h,cpp}` y los WAV de `Assets/ImplementationKit/assets/ir` sin cambios. En el mezclador, cada pareja combina los bancos de sala antes de filtrar. Las salidas L/R usan estados integradores distintos y los mismos coeficientes, actualizados una vez por muestra. Después se aplican ganancia/mute/solo, medidor de pareja, suma wet, MIX y OUTPUT. Dry y bypass siguen usando la ruta anterior. El callback no crea geometrías, hilos ni buffers de filtros. El reporte de pruebas DSP conserva la instrumentación de heap y la referencia analítica RBJ independiente. Los 14 IDs de 0.1 mantienen su orden y versionHint 1; los doce parámetros nuevos se añaden tras ellos con versionHint 2. La suite adapta las comparaciones visuales a las imágenes nuevas y al rango de tamaños aprobado para esta iteración. Las pruebas matemáticas de las IR mantienen sus tolerancias.

## Resultado de los dos null tests finales

| Referencia frente a 0.2.0 | Segmentos | Muestras escalares | Patrones diferentes | Residual peak / RMS | Latencia |
|---|---:|---:|---:|---:|---:|
| EQ OUT | 234 | 213 520 746 | 0 | 0 / 0 | 128 muestras |
| EQ IN plana, tres parejas | 234 | 213 520 746 | 0 | 0 / 0 | 128 muestras |

Los tres streams tienen el mismo SHA-256 `878abb021bae7000a770d69ff0bf68845c19120c95a6e8d754f475a3961d1fe4`. Los dos JSON pequeños `docs/reports/final-touch-null-off.json` y `final-touch-null-flat.json` registran los resultados completos. Los PCM y manifiestos detallados se mantienen en `build/baseline/renders`, `build/final-touch-zero-off` y `build/final-touch-zero-flat`. La biblioteca DSP candidata del ensayo tiene SHA-256 `7468055bee2f9c9d391a096edb78f776c4fc53ec9bca0b0adcd2a81596731503`. La prueba se enlazó con el código compartido corregido `b97115f15e02f6833e7ed49e68c7bdbedcb8bfbe824b70f925fbccd261329fab`, antes de las últimas modificaciones del editor. El harness no crea ventanas de plugin ni invoca componentes gráficos.

## Tiempo y memoria del processor en condiciones emparejadas

Se reservaron los árboles de build y se ejecutaron los dos binarios de regresión una vez terminadas las capturas. Se midieron 54 procesos independientes, sin compilaciones, renders, CTest ni empaquetado simultáneos del proyecto. Por cada tasa/bloque se hicieron tres rondas, rotando el orden de 0.1 / 0.2 OUT / 0.2 plana. Cada celda de tiempo es la mediana de la estadística de sus tres rondas. El archivo `final-touch-paired-performance.tsv` conserva los tiempos individuales, incluidos p95, máximo y RSS. El experimento mide `processBlock` sin editor abierto, en régimen estable.

| Fs y bloque | Media µs 0.1 / OUT / plana | p99 µs 0.1 / OUT / plana | Excesos 0.1 / OUT / plana | Callbacks por modo |
|---|---|---|---|---:|
| 48 kHz / 64 | 11,1 / 12,2 / 12,3 | 38,7 / 39,2 / 39,5 | 0 / 0 / 0 | 13 500 |
| 48 kHz / 256 | 43,6 / 47,2 / 48,2 | 74,9 / 80,8 / 76,9 | 0 / 0 / 0 | 3375 |
| 96 kHz / 64 | 17,7 / 18,2 / 18,2 | 55,0 / 53,6 / 54,8 | 0 / 0 / 0 | 27 000 |
| 96 kHz / 256 | 69,7 / 73,2 / 71,5 | 114,1 / 125,8 / 116,8 | 0 / 0 / 0 | 6750 |
| 192 kHz / 64 | 30,9 / 32,5 / 32,5 | 86,2 / 93,7 / 97,6 | 2 / 6 / 4 | 54 000 |
| 192 kHz / 256 | 123,9 / 135,6 / 141,0 | 182,1 / 259,1 / 289,3 | 1 / 0 / 0 | 13 500 |

El RSS mediano de los tres modos estuvo entre 74,9 y 75,0 MiB a 48 kHz; 101,0 y 101,1 MiB a 96 kHz, y 173,9 y 174,0 MiB a 192 kHz. Estos valores corresponden al proceso completo y no miden por sí solos el coste de memoria de los filtros. A 192 kHz/64, el máximo observado fue de 1936,92 µs para 0.1, 1230,08 µs en 0.2 OUT y 535,13 µs en 0.2 IN plana, frente a 333,33 µs de plazo. Ver `final-touch-eq.md` para el coste con EQ no plana fija, con automatización y con dos bancos activos. El presupuesto musical sostenido de la instancia en Logic permanece pendiente.

```sh
python3 -B scripts/final_touch_regression.py paired build/baseline/renders build/final-touch-zero-off docs/reports/final-touch-paired-performance.tsv
```

El subcomando `paired` exige un fichero de salida nuevo. Para repetir, elegir otra ruta.

## Revisión independiente del editor compilado

Se inspeccionaron las doce capturas de Kentucky, Center Field y Live Room a 1024 × 684, 1280 × 854, 1586 × 1058 y 2560 × 1708 (escala 2×), junto con el detalle de controles de 1006 × 740. En el mínimo, las nueve lecturas neutras muestran `0.0 dB`, las escalas numéricas de los tres medidores se ven completas y cada pareja conserva sus tres controles, fader, mute, solo y EQ IN. Los seis rótulos de micrófonos se sitúan dentro de la foto en las tres salas. El material del panel, tornillos, escala de EQ y estrías del fader son visibles en el detalle. La suite UI del mismo editor pasó 2366 comprobaciones en 15 secciones. Incluye alcance real de los hitboxes de EQ, edición numérica, actualización desde automatización e historial. El propietario respondió «ok, validadads» al recibir las tres capturas finales de las salas. Queda registrada la aprobación visual de esa terna. La escucha de la EQ activa y el smoke del AU 0.2.0 en Logic siguen pendientes.

En `RoomScene`, el fondo limpio usa el rectángulo de contención de la fotografía. Un componente hijo dibuja BOOST THIS desde el único PNG con alfa, con transformación, luz y sombra de contacto por sala. Los seis marcadores independientes conservan las posiciones relativas a la foto y reaccionan al botón MICS. El manifiesto verifica los tres fondos, el PNG, y los recortes de material y tornillo de la referencia aprobada. Las cuatro imágenes originales nuevas conservan sus hashes.

## Correspondencia con el AU universal y veredicto

Se verificó directamente el SHA-256 `5bf36ca80b635aea63e25f9d1e28cabe68774437231922a286ee6c16553e4541` del ejecutable en `build/universal-0.2.0/AU/LAB-RC.component`. El recibo `final-touch-build-candidate.json` identifica el mismo ejecutable. Coinciden las siete fuentes auditadas de DSP y processor con `final-touch-build-audio-sources.json`. La biblioteca estática DSP arm64 del build definitivo conserva exactamente el SHA-256 del null test. Los 13 PNG documentados de `docs/screenshots/final-touch` coinciden por SHA-256 con los renders revisados y con `final-touch-screenshots.json`. El build fusionó los dos slices Release con mínimos arm64 macOS 11.0 y x86_64 macOS 10.15. Los resultados de validación de formato e instalación se registran en `final-touch-build.md`; el null se midió con el processor arm64.

**Conformidad acústica comprobada** para EQ OUT y EQ IN plana, con el recall legado, las rutas y los cambios de sala cubiertos. **Calidad de integración verificada** en las curvas y controles ensayados. En esta revisión queda resuelto el defecto de cero APVTS. El propietario aprobó las tres vistas finales. Siguen abiertos la escucha de EQ activa, el comportamiento de una sesión real de 0.2.0 en Logic y su margen bajo carga a 192 kHz. El trabajador de salas mantiene su dependencia de la planificación durante audio continuo.
