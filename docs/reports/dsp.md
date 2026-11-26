# Motor DSP de LAB-RC 0.1.0

Verificación local del 25 de noviembre de 2026. Se probaron el mezclador float32 y los nueve WAV v06 del kit. Los WAV v05 sirven como referencia independiente para la entrada mono.

## Señal y tiempo

Cada entrada conserva sus dos destinos con el orden LL, LR, RL y RR del manifiesto. Cada pareja recibe la suma de sus dos contribuciones. La entrada mono alimenta ambas rutas a ganancia uno. El mezclador aplica los faders después de la convolución. Los estados mute, solo y bypass mantienen los historiales de entrada. Las señales pueden superar 0 dBFS.

Se fijó la latencia de cálculo en 128 muestras a todas las tasas y tamaños de bloque. Las cuatro líneas de retardo conservan el estéreo original para bypass y el estéreo con input trim para dry. La cola declarada es 4,5 s más 128/Fs. El primer bloque adopta los parámetros de la instancia. Los cambios posteriores de trims, ganancias efectivas de parejas, mezcla y bypass se interpolan en amplitud durante 20 ms.

A 96 kHz el motor utiliza las muestras originales. En otras tasas interpola mediante sinc centrada con ventana Blackman, extensión cero y rechazo por encima del menor Nyquist. Multiplica por 96000/Fs para conservar la ganancia de convolución. No se realinean picos y se conservan los silencios acústicos iniciales.

La convolución overlap-add comparte los espectros de ambas entradas entre las doce rutas. Las particiones son de 128, 512, 2048 y 8192 muestras. La FFT de Apple usa Accelerate. Las demás plataformas disponen de FFT radix-2 con tablas y memoria privada reservadas durante la preparación. Los productos de las particiones largas y las FFT inversas se distribuyen en tramos de 128 muestras. Los bloques mayores que la capacidad de preparación se dividen internamente.

## Salas y medidores

La primera sala se prepara sincrónicamente con el audio detenido. Un trabajador por instancia consulta la última selección y construye el banco siguiente sin tocar el activo. El callback consume únicamente bancos completos. El banco nuevo recibe 128 muestras de entrada antes del fundido para cubrir su latencia. El fundido complementario ocupa 50 ms. La cola antigua se atenúa durante ese intervalo. Si hay automatización rápida, termina el fundido en curso y atiende la última selección. Un proveedor que lance una excepción en el trabajador conserva la sala anterior y permite elegir otra. Una excepción en la preparación inicial llega al llamante.

Dos casillas con publicación acquire/release limitan el procesamiento a dos bancos. El callback devuelve la casilla retirada al trabajador. La destrucción de espectros y planes FFT se hace en el trabajador o durante la liberación con el audio parado. El callback carece de mutex, acceso a archivos y reconstrucción de IR. Los medidores publican máximos lineales por canal con atomics lock-free. Una lectura desde la GUI devuelve el máximo de cada canal desde la lectura anterior y lo pone a cero. Esto captura un transitorio aunque varios callbacks silenciosos lo sigan antes del refresco visual.

## Pruebas y tolerancias

El primer ensayo usó una implementación vacía con paso directo. Se observaron 29 fallos de encaminamiento, cola y referencia WAV, incluido un error absoluto estéreo de 1,13483787. Las pruebas añadidas antes del motor también fallaron en seis tasas, en los controles y en la transición de sala. La prueba específica del medidor falló tres veces al esperar un pico de 1,75 y recibir cero antes de incorporar la retención.

Se recorrieron todas las muestras de las nueve matrices a 96 kHz con impulso exclusivamente izquierdo, exclusivamente derecho y mono a unidad. La comparación incluye el silencio anterior a la latencia y las 256 muestras posteriores a la cola. Error máximo observado **2,98 × 10⁻⁸**. El límite del test es 2 × 10⁻⁵.

Los dos oráculos de remuestreo se calculan sin usar las tablas sinc ni la FFT de producción.

- El primero calcula en doble precisión una convolución directa de un burst estéreo pseudoaleatorio de 73 muestras con cuatro FIR analíticas. Las bandas gaussianas se centran en 1250, 17500 y 32000 Hz. A 44,1 y 48 kHz se espera rechazo de la componente de 32 kHz. También hay una prueba temporal específica del origen y ganancia a 7300 Hz. Tolerancias absolutas de 8 × 10⁻⁵ y 2 × 10⁻⁵, respectivamente.
- El segundo compara la respuesta compleja del render con la DFT directa en doble precisión de los cuatro canales de cada WAV v06 y de las dos entradas pseudoaleatorias. Evalúa 51, 411, 1753, 7300 y 16000 Hz en ambas salidas de las nueve parejas. La tolerancia absoluta es 1,5 × 10⁻⁴. Son cinco frecuencias de control por respuesta.

| Fs (Hz) | Error máximo temporal del burst | Error máximo complejo de los WAV |
|---:|---:|---:|
| 44100 | 0,000011709 | 0,000017582 |
| 48000 | 0,000008410 | 0,000005998 |
| 88200 | 0,000004461 | 0,000001076 |
| 96000 | 0,000000124 | 0,000001036 |
| 176400 | 0,000002636 | 0,000002535 |
| 192000 | 0,000001273 | 0,000002936 |

El render intercala bloques de 1, 32, 64, 127, 128, 256, 512, 1024, 2048 y 2801 muestras. Se verificaron 64 combinaciones mute/solo, mute prioritario, fader −60 dB, puntos intermedios de las rampas, ganancias por encima de unidad, dry/bypass, recuperación de NaN/Inf, continuidad de una reflexión tras mutear y bypass, cambios rápidos durante una transición y recuperación tras un fallo de carga. La FFT portable se compiló también en arm64 con LABRC_USE_ACCELERATE=0. Las primeras nueve secciones de la suite pasaron 183 comprobaciones con ese motor. La ejecución Windows y la escucha en Logic corresponden a los gates de plataforma y escucha.

## Reproducción

Desde la raíz y con JUCE local preparado según la documentación de compilación:

```sh
cmake --preset mac-dsp
cmake --build --preset mac-dsp
ctest --preset mac-dsp --verbose
clang -O2 -dynamiclib scripts/dsp_heap_audit.c -o /tmp/labrc_heap_audit.dylib
env DYLD_INSERT_LIBRARIES=/tmp/labrc_heap_audit.dylib build/mac-dsp/tests/labrc_tests --benchmark
```

El contador opcional usa interposición macOS en malloc, calloc, realloc, free, posix_memalign y malloc_zone_malloc/free. Se arma solo en el hilo que llama a process. Una asignación y liberación deliberadas comprueban primero que el detector está activo. La prueba recorre el primer callback, el estado estable, la carga concurrente, el fundido y la devolución del banco retirado. El contador cubre las rutas interpuestas del proceso ensayado.

## Resultados Release y presupuesto por callback

Con el código final, CTest pasó DSP y BuildTools. La categoría DSP pasó **204 comprobaciones en 13 secciones**, y el benchmark instrumentado pasó sus **29 comprobaciones en 12 secciones**. Se observaron **cero llamadas de heap** interpuestas dentro de process en todas las combinaciones medidas.

MacBook Air arm64 con macOS 26.5, Apple clang 21, JUCE 8.0.17 y preset mac-dsp Release. Una instancia, Live Room con tres parejas, entrada estéreo pseudoaleatoria continua, seis segundos de audio por fila. Medición de tiempo de pared en microsegundos. La compilación de otros formatos compartía la máquina durante la última ejecución. El RSS corresponde al proceso de prueba en ese momento e incluye el asignador, código y buffers del banco de pruebas.

| Fs | Bloque | Media µs | p95 µs | p99 µs | Máximo µs | Plazo µs | Excesos / callbacks | RSS MiB |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 48000 | 1 | 0,63 | 0,46 | 0,83 | 157,92 | 20,83 | 2205 / 288000 | 53,5 |
| 48000 | 64 | 14,39 | 38,96 | 40,88 | 55,42 | 1333,33 | 0 / 4500 | 55,0 |
| 48000 | 256 | 64,38 | 102,04 | 120,79 | 386,25 | 5333,33 | 0 / 1125 | 55,7 |
| 96000 | 1 | 0,75 | 0,46 | 0,62 | 494,46 | 10,42 | 4558 / 576000 | 113,3 |
| 96000 | 64 | 30,37 | 92,96 | 140,71 | 482,08 | 666,67 | 0 / 9000 | 112,3 |
| 96000 | 256 | 96,72 | 151,96 | 178,79 | 248,62 | 2666,67 | 0 / 2250 | 106,1 |
| 192000 | 1 | 1,16 | 0,50 | 1,21 | 4170,21 | 5,21 | 10199 / 1152000 | 182,4 |
| 192000 | 64 | 47,63 | 138,62 | 183,83 | 1101,12 | 333,33 | 14 / 18000 | 153,4 |
| 192000 | 256 | 156,24 | 234,00 | 281,29 | 565,88 | 1333,33 | 0 / 4500 | 142,3 |

Para medir la transición se renderizó primero el mismo material con la sala antigua. La primera diferencia en una segunda pasada localiza el inicio del fundido. Se reunieron cuatro transiciones Live Room → Center Field con ambos bancos, bloque de 64, y se excluyó el tiempo de preparación de las estadísticas de fundido.

| Fs | Callbacks | Media µs | p95 µs | p99 µs | Máximo µs | Excesos | Mayor RSS muestreado, MiB |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 96000 | 300 | 28,62 | 66,42 | 78,04 | 93,83 | 0 | 254,2 |
| 192000 | 600 | 55,97 | 160,08 | 243,25 | 321,54 | 0 | 377,5 |

Los bloques de una muestra son matemáticamente correctos pero exceden su plazo estricto cuando toca una FFT. A 192 kHz y 64 muestras hubo 14 excesos durante el estado estable y margen de solo 11,79 µs para el peor fundido observado. Como primera configuración para la prueba de estrés a 192 kHz, un bloque de 256 permite medir más margen sin alterar las IR ni el sonido. Quedan pendientes la validación sostenida con varias instancias y material musical en el DAW, la escucha A/B y las mediciones nativas Windows.
