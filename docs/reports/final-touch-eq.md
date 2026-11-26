# Final Touch 0.2.0 · EQ por pareja

## Señal y compatibilidad

EARS, FRONT y FLOOR tienen tres biquads en cascada. Cada pareja recibe la salida de su convolución después del fundido de sala. El fader, mute/solo, el medidor y la suma wet reciben la salida de la EQ. Los filtros de izquierda y derecha comparten coeficientes y conservan integradores independientes. Los boosts pueden elevar el nivel por encima de 0 dBFS.

LOW es un estante de 100 Hz con S=1. MID es una campana de 1 kHz con Q=0,7. HIGH es un estante de 8 kHz con S=1. Los tres controles abarcan −12 a +12 dB en pasos de 0,1. La señal dry y el bypass global conservan sus rutas de 0.1. El filtrado no añade muestras de retardo. La latencia de convolución permanece en 128 muestras y la cola declarada en 4,5 s + 128/Fs.

El biquad usa dos integradores trapezoidales en doble precisión. Con A = 10^(dB/40) y un prewarping tan(πf/Fs), el estante bajo ajusta la frecuencia interna por 1/√A y el alto por √A. Ambos emplean amortiguamiento √2. La campana usa 1/(Q·A). Los pesos que combinan entrada, paso banda y paso bajo reproducen la función de transferencia RBJ. Las pruebas utilizan otra representación, los cinco coeficientes explícitos del filtro digital.

Las ganancias recorren una rampa lineal en dB de 20 ms, realizada con una rampa multiplicativa de √A. Cada muestra de la rampa recalcula coeficientes a partir de esa ganancia. Una nueva consigna inicia la rampa desde el valor alcanzado. EQ IN interpola durante 20 ms entre señal directa y filtrada. Los integradores siguen recibiendo audio en OUT y con faders muteados. En OUT estable la ruta devuelve el float original directamente. Cada banda plana transmite el valor de entrada directamente mientras sigue actualizando sus estados. El antidenormal del mezclador cubre las dos rutas.

Los catorce parámetros anteriores conservan ID, posición y versionHint 1. Tras ellos se registran doce nuevos, con versionHint 2 para el orden del AU. Cada prefijo ears/front/floor admite los sufijos EqLowDb, EqMidDb, EqHighDb y EqEnabled. Faltando campos en un proyecto antiguo, las ganancias se inicializan explícitamente a 0 dB y los enables a false. Los faders y las EQ son globales a las salas. Todos los parámetros nuevos participan en gesto, historial por instancia, reset único y guardado. El envelope v1 admite los campos aditivos. El atributo visual editorLayoutVersion=2 distingue las dimensiones nuevas. Para proyectos 0.1 se multiplica el ancho por 1280/1180 y se recalcula el alto proporcional, limitado al rango 1024×684 – 1586×1058.

## Pruebas

- Antes de implementar la EQ, las pruebas nuevas mostraron nueve fallos en curvas y rampas. El processor mostró trece fallos por los parámetros ausentes. La migración geométrica falló en los cuatro ensayos de tamaño antes del ajuste.
- La prueba DSP compara el espectro complejo del impulso con la transferencia analítica a 44,1, 48, 88,2, 96, 176,4 y 192 kHz. Usa once puntos de DC a Nyquist, incluidas las tres transiciones, cada banda aislada a ±12 dB y tres combinaciones extremas. En Fs distintas de 96 kHz se incorpora la respuesta medida del impulso de referencia sin EQ, pues ese impulso también atraviesa el remuestreador del kit. Máximo error complejo absoluto **1,817 × 10⁻⁷**, frente al umbral **2 × 10⁻⁵**. La relación entre canales 1 y −0,5 tuvo error máximo cero.
- Las comparaciones bit a bit de la fixture estéreo verificaron OUT con ajustes no planos, IN plana, EQ de otra pareja sola, dry y bypass. La prueba de referencia de las nueve matrices a 96 kHz conservó su tolerancia y alcanzó un error máximo de **2,98 × 10⁻⁸** frente a WAV v06/v05. El null interversiones con las nueve parejas y recursos reales pertenece al harness independiente de QA.
- En un nivel constante de 0,2 FS se verifican el punto medio y el final de rampas IN y de ganancia. Al alternar ajustes y enables cada 127 muestras, el máximo fue **0,2147896 FS**, y la mayor diferencia entre muestras **0,00031059 FS**. En una reflexión sintética que decae, la misma clase de automatización produjo un máximo de **0,4957809 FS** y una diferencia máxima entre muestras de **0,00058404 FS** después del ataque. El canal de entrada silencioso se mantuvo a cero. Se probó la recuperación tras NaN/Inf en los tres controles y el retorno a silencio.
- El preset Release mac-dsp pasó **3/3**. La categoría DSP pasó **236/236**. El test histórico de imágenes detectó el recurso antiguo ausente durante la sustitución. Tras actualizar el comprobador de píxeles a los tres fondos definitivos y el PNG transparente, la categoría Processor del harness Release pasó **421/421** en 22 secciones, incluidas las 211 comprobaciones nuevas de EQ, recall y geometría. La ejecución integrada final de todos los componentes corresponde al build con la UI terminada.

## Tiempo y memoria

Medición local Release arm64 en macOS 26.5, JUCE 8.0.17 y Apple clang 21. Se enlazó el test y la biblioteca DSP de la revisión 0.1 del checkpoint con el mismo runner y los módulos JUCE locales. El benchmark actualizado repite la señal pseudoaleatoria, las tres parejas de Live Room, el número de callbacks y la temporización de pared del anterior. En 0.2 además ensaya EQ fija y automatización de las nueve ganancias y tres enables en cada callback. El detector de heap se validó con asignación y liberación deliberadas. En las rutas instrumentadas se registraron **cero llamadas** interpuestas. Pasaron **29/29** comprobaciones del benchmark anterior y **37/37** del nuevo.

Cada fila corresponde a una instancia y seis segundos de audio. Se ejecutaron en procesos separados consecutivos, con otros trabajos de desarrollo en el equipo. El RSS representa el proceso completo y depende del asignador. En este build arm64, sizeof(PairEqualiser) es 480 bytes. Las tres EQ ocupan 1440 bytes fijos por instancia.

| Fs y bloque | Versión y modo | Media µs | p95 µs | p99 µs | Máximo µs | Fuera de plazo | RSS MiB |
|---|---|---:|---:|---:|---:|---:|---:|
| 96 kHz / 64 | 0.1 | 27,72 | 66,29 | 81,42 | 178,88 | 0/9000 | 111,8 |
| 96 kHz / 64 | 0.2 OUT | 27,78 | 65,88 | 81,42 | 144,04 | 0/9000 | 111,7 |
| 96 kHz / 64 | 0.2 IN fija | 28,81 | 66,21 | 83,62 | 166,38 | 0/9000 | 111,8 |
| 96 kHz / 64 | 0.2 automatizada | 47,83 | 120,75 | 160,83 | 617,17 | 0/9000 | 84,3 |
| 192 kHz / 64 | 0.1 | 36,62 | 83,58 | 99,04 | 167,00 | 0/18000 | 229,6 |
| 192 kHz / 64 | 0.2 OUT | 56,87 | 147,38 | 226,58 | 497,25 | 21/18000 | 139,5 |
| 192 kHz / 64 | 0.2 IN fija | 49,69 | 111,46 | 134,58 | 1081,79 | 3/18000 | 127,3 |
| 192 kHz / 64 | 0.2 automatizada | 54,99 | 138,00 | 190,75 | 456,38 | 3/18000 | 142,1 |

Para cuatro fundidos a 192 kHz/64, 0.1 registró 4 excesos en 600 callbacks y 0.2 OUT registró 0 en 600. Los resultados varían con la planificación del sistema y el espectro de carga de una sesión. Continúan pendientes escucha, presupuesto multinstancia y smoke físico de 0.2 en Logic.

Desde la raíz, con JUCE local preparado:

```sh
cmake --build build/mac-dsp --target labrc_tests -j 4
ctest --preset mac-dsp --verbose
clang -O2 -dynamiclib scripts/dsp_heap_audit.c -o /tmp/labrc_heap_audit.dylib
env DYLD_INSERT_LIBRARIES=/tmp/labrc_heap_audit.dylib build/mac-dsp/tests/labrc_tests --benchmark
```
