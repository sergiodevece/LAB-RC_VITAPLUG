# Correcciones I1–I3 de restauración de estado

Verificación local Release arm64 del 25 de noviembre de 2026.

## Sala del primer impulso

La restauración antes de preparar el processor suministra la sala inicial a `prepareToPlay`. Si el host ya preparó el processor pero todavía no procesó muestras, la restauración instala sincrónicamente el banco de esa sala en el hilo de control. Una selección idéntica conserva el banco preparado. Se conserva la latencia de 128 muestras y la cola de 4,5 s + 128/Fs.

La prueba integrada compara dos órdenes de reapertura a 192 kHz con bloques de 64 muestras. Excita ambos canales con el mismo impulso estéreo en ambas instancias, aísla cada pareja y compara las primeras 19200 muestras de ambos canales. **Las nueve parejas produjeron salidas idénticas muestra por muestra.** La sonda offline original de QA también volvió a ejecutarse con la nueva biblioteca. El error máximo respecto a Live Room preparado de antemano pasó de 0,052534 a **0**. La diferencia con Kentucky fue **0,052534**. La comparación del resto de la cola a 96 kHz permanece cubierta por las pruebas de las IR completas.

El parámetro de sala conserva su ID, rango y versión. Su método `AudioParameterChoice::valueChanged` envía una petición atómica al trabajador incluso con el transporte detenido. El trabajador puede recuperar con compare-and-swap una casilla preparada que el callback aún no ha reclamado, para obedecer una selección más reciente durante ese intervalo. Un test pide dos salas sucesivas sin ningún callback y verifica la carga de las tres parejas de cada una.

Tras el primer bloque con muestras, las restauraciones utilizan la carga asíncrona y el fundido existente de 50 ms. La disponibilidad de una sala nueva durante automatización activa depende del tiempo de preparación del trabajador. La salida anterior sigue sonando hasta que se dispone del banco siguiente.

## Sincronización

La preparación, la liberación y la instalación de un banco inicial utilizan el `getCallbackLock()` que los wrappers JUCE ya emplean alrededor del procesamiento. La ruta de control comprueba de nuevo si el audio empezó después de adquirir ese bloqueo. Una restauración con audio activo aplica el estado y delega el cambio acústico en el trabajador. Las gestiones del historial y la sustitución de estado se serializan entre GUI y control mediante una sección crítica propia. La aplicación nunca adquiere esa sección en `processBlock`, `RoomMixer::process` o la notificación de sala. Un llamante que invoque simultáneamente el processor de forma directa debe respetar el mismo contrato de ciclo de vida que el wrapper.

Los wrappers, el APVTS y las notificaciones de parámetros de JUCE tienen sincronización propia. El contador de heap instrumenta únicamente las llamadas a `RoomMixer::process`. En esta versión pasó el control positivo y registró cero llamadas de heap en todos los escenarios estables y fundidos del benchmark, con **29/29 comprobaciones**.

## Undo, gestos y validación

La GUI dispone de `canUndoUser`, `canRedoUser`, `undoUser` y `redoUser`. Un restore válido desde otro hilo invalida el historial bajo el mismo bloqueo que las operaciones de usuario. La siguiente consulta, acción o edición GUI reconcilia esa invalidación antes de actuar. La corrección funciona antes de que se despache cualquier mensaje pendiente. `undoManager` sigue expuesto por compatibilidad del código existente. Los accesos de producción de la GUI usan las cuatro API del processor.

Al recibir un restore durante un drag, se cierra una sola vez el gesto comunicado al host. Los cambios tardíos del drag quedan descartados hasta su liberación o el comienzo de un gesto nuevo. El siguiente gesto crea historial desde el valor restaurado. El test registra las notificaciones de inicio y fin del host, ejecuta el restore desde otro hilo e intenta mover el control antes de cualquier despacho GUI.

El envelope admite el esquema entero canónico `version="1"` con exactamente un hijo `PARAMETERS`. Sus entradas deben ser del tipo `PARAM`. Se rechazan raíz ajena, hijo de tipo incorrecto, versión omitida en el envelope, desconocida, no numérica y fraccionaria. La compatibilidad histórica se conserva para un árbol `PARAMETERS` directo sin envelope. Dentro de una estructura válida, los campos ausentes o no finitos conservan los defaults especificados y los numéricos fuera de rango se acotan. Las entradas de identificador desconocido se ignoran. La validación precede a todo cambio de DSP, preferencias o historial.

La sonda de QA adaptada para llamar a las API de usuario recuperó −12 dB y un undo inmediato dejó el valor en −12 dB. El envelope malformado posterior conservó +10 dB y el historial. El tamaño visual del editor se sincroniza en el siguiente refresco de la GUI, objeto de I4.

## Evidencia y reproducción

Las pruebas nuevas se escribieron primero y se ejecutaron con la biblioteca anterior y métodos provisionales equivalentes al comportamiento previo. Se observaron **34 fallos**. Seis correspondían a impulsos de salas incorrectas, seis al historial obsoleto, tres al drag durante recall, dieciocho al envelope inválido y uno a la solicitud de sala sin callbacks.

Con el código de esta corrección, la categoría **Processor pasó 206/206** y la categoría **DSP pasó 206/206**. BuildTools también pasó. En la primera ejecución integrada quedaron dos fallos de refresco en UI, comunicados a su propietario para su corrección y revalidación. La revisión independiente y el smoke del DAW corresponden a las siguientes fases.

```sh
cmake --build build/mac-arm64 --target labrc_tests -j 4
ctest --preset mac-arm64 --verbose
clang -O2 -dynamiclib scripts/dsp_heap_audit.c -o /tmp/labrc_heap_audit.dylib
env DYLD_INSERT_LIBRARIES=/tmp/labrc_heap_audit.dylib build/mac-arm64/tests/labrc_tests --benchmark
```

## Segunda ronda, I9. Última petición de sala al detener el transporte

En el processor, `AudioParameterChoice::valueChanged` publica la selección de sala en un único valor atómico. El mixer se prepara con `externallyControlledRoom=true` tanto durante `prepareToPlay` como al restaurar en frío. Bajo ese modo el callback consulta directamente la selección publicada para aceptar un banco y omite la escritura desde el snapshot del bloque. Un bloque capturado antes de una automatización ya no puede anular la petición mientras el transporte queda parado. Los clientes independientes que omiten el nuevo argumento siguen seleccionando la sala con `Parameters::room` en cada bloque.

La prueba retiene al trabajador en una carga anterior, solicita la sala 2, ejecuta el último callback con sala 0 en su snapshot y detiene el procesamiento. Las condiciones de prueba fijan ese orden fuera del callback. El trabajador debe preparar la sala 2 durante la pausa. Al reanudar con el mismo snapshot obsoleto, la salida debe llegar a la ganancia 3 de la sala nueva. Antes de aplicar la corrección fallaron ambas aserciones y la ganancia quedó en 1.

Para que el primer impulso se renderice en la sala recuperada sin arriesgar una espera del wrapper, el host debe finalizar la restauración en frío antes de comenzar la reproducción. Si llama al primer render simultáneamente con esa preparación, el bloqueo de ciclo de vida de JUCE puede aplazar la entrada en el callback. El callback propio conserva sus operaciones de acceso atómico y procesamiento preasignado.

El preset arm64 integrado pasó **5/5** en 14,08 s. DSP pasó 209/209, Processor 206/206 y UI 2231/2231. La prueba aislada pasó sus tres aserciones, incluido el inicio de la carga previa que fija la intercalación. Las pruebas clásicas de matrices, transiciones y las nueve parejas recuperadas siguen en el mismo suite.

### Medición final con control externo de sala

Mismo Mac arm64, Release, una instancia y ruido estéreo. Se usó el binario integrado con la instrumentación y el control positivo descritos en `dsp.md`. Se detuvieron las compilaciones del proyecto hasta terminar esta medición. Las duraciones corresponden al tiempo de pared del proceso de usuario, con la planificación ordinaria de macOS. Pasaron **29/29** comprobaciones funcionales y de heap. Las comprobaciones de tiempo quedan cuantificadas en la tabla. Todos los escenarios contabilizaron cero operaciones de heap interpuestas en el callback.

| Fs | Bloque | Escenario | Media µs | p95 µs | p99 µs | Máximo µs | Excesos / callbacks | RSS proceso MiB |
|---:|---:|---|---:|---:|---:|---:|---:|---:|
| 48000 | 1 | Live Room | 0,43 | 0,33 | 0,46 | 134,33 | 999 / 288000 | 54,9 |
| 48000 | 64 | Live Room | 11,55 | 29,71 | 43,67 | 100,08 | 0 / 4500 | 54,0 |
| 48000 | 256 | Live Room | 45,32 | 65,42 | 76,75 | 119,46 | 0 / 1125 | 54,1 |
| 96000 | 1 | Live Room | 0,61 | 0,42 | 1,33 | 660,42 | 4682 / 576000 | 105,3 |
| 96000 | 64 | Live Room | 24,81 | 74,83 | 131,83 | 5524,08 | 5 / 9000 | 80,5 |
| 96000 | 256 | Live Room | 67,37 | 85,08 | 102,00 | 147,79 | 0 / 2250 | 80,6 |
| 192000 | 1 | Live Room | 0,77 | 0,33 | 0,50 | 327,54 | 9416 / 1152000 | 113,4 |
| 192000 | 64 | Live Room | 29,76 | 67,21 | 79,21 | 234,92 | 0 / 18000 | 153,5 |
| 192000 | 256 | Live Room | 161,29 | 311,71 | 465,29 | 2217,58 | 2 / 4500 | 136,9 |
| 96000 | 64 | Fundido | 26,02 | 59,04 | 178,17 | 201,79 | 0 / 300 | 218,9 |
| 192000 | 64 | Fundido | 50,08 | 130,62 | 309,50 | 839,00 | 5 / 600 | 314,1 |

A 192 kHz y 64 muestras el plazo es 333,33 µs. Hubo excesos durante cinco bloques de los cuatro fundidos reunidos. También hubo excesos en régimen estable en otros tamaños de bloque y tasas. La prueba deja pendiente medir y escuchar en el host con interfaz, audio musical y varias instancias.

El detalle de cada prueba y cada medición queda en los logs adjuntos. [Test de regresión antes de la corrección](recall-i9-red.log) · [CTest arm64 final](recall-i9-ctest.log) · [Benchmark final y control de heap](recall-i9-heap-benchmark.log).
