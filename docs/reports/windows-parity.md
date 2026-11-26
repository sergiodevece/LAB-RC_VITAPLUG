# LAB-RC 0.2.0 · paridad de audio para Windows x64

## Referencia y criterio

La referencia `build/final-touch-zero-off` contiene el render Release arm64 del procesador compartido por el AU 0.2.0 instalado. El [informe Final Touch](final-touch-regression.md#correspondencia-con-el-au-universal-y-veredicto) documenta la correspondencia de las fuentes, el banco DSP y el SHA-256 del AU. El nuevo objetivo `labrc_final_touch_regression` enlaza el mismo arnés de regresión al procesador que se construye con MSVC. La carga del wrapper VST3 se verifica por separado con los validadores.

El arnés reproduce 234 segmentos estéreo a 44,1, 48, 88,2, 96, 176,4 y 192 kHz con EQ OUT en las tres parejas. Recorre los impulsos izquierdo, derecho y mono reales, un burst estéreo, recall de un estado 0.1.0, bypass y cambios de sala. El arnés comprueba 128 muestras de latencia, muestras finitas y dos renders deterministas de cada secuencia de cambios. La referencia usa el estado con SHA-256 `281e19d58ea27403ba22d89a8b4212c5c14dbd5429a3d6e33fb9788286d8c7fa`.

El comparador conserva un umbral de identidad bit a bit por defecto. Para comparar la FFT portable con Accelerate admite `--peak-tolerance 2e-5`, el límite del Gate 1 original en amplitud lineal. Comprueba disposición, número de muestras, latencia, versión del arnés y del estado. Registra diferencias de bits y residual pico/RMS de cada tramo, sin compensar tiempo, ganancia ni fase. [Gate 1 del proyecto](../../Assets/ImplementationKit/MASTER_PROMPT.md)

```sh
python3 -B scripts/final_touch_regression.py compare \
  build/final-touch-zero-off WINDOWS_PARITY_RENDER_DIR windows-parity.json \
  --peak-tolerance 2e-5
```

## Evidencia local

- Release macOS arm64 pasó DSP, Processor y UI. La respuesta compleja de los nueve controles EQ se comprobó en ambos extremos, por pareja y a las seis tasas, contra las fórmulas RBJ. Error máximo observado de `1,817e-7` y error de enlace L/R de cero en el estímulo medido.
- Los renders previos de macOS arm64 0.2.0 con EQ OUT y con EQ IN plana coincidieron bit a bit en las 213.520.746 muestras al volver a ejecutar el comparador. Se comprobó con datos pequeños que el modo tolerante acepta el residual dentro del límite, rechaza el superior y rechaza valores no finitos.
- El procesador Release arm64 enlazado con `LABRC_USE_ACCELERATE=0` reprodujo los 234 tramos completos con EQ OUT, incluidos los cambios de sala repetidos. Frente a Accelerate, hubo 157.198.058 patrones de bits diferentes de 213.520.746. El residual pico fue `3,5763e-7` y el RMS `4,5325e-9`, en amplitud lineal. El pico se mantuvo por debajo de `2e-5` y los dos renders declararon 128 muestras de latencia. Esta prueba ejercita el backend portable que usa Windows, compilado y ejecutado en macOS. Su suite DSP separada pasó 241 comprobaciones en 20 secciones.
- La regresión concurrente nueva ejecutó dos procesadores preparados en hilos distintos. El silencio de una instancia se mantuvo exacto mientras la otra emitía audio finito.

## Verificación nativa y smoke pendientes

| Área | Evidencia prevista en Windows x64 |
|---|---|
| Compilación, carga y formatos de canal | MSVC Release, CTest y pluginval. Steinberg VST3 Validator si se aporta un ejecutable verificado. El DAW confirma layouts mono a estéreo y estéreo a estéreo según su propio enrutamiento |
| EQ, convolución, latencia, cambios de sala y null test | Suite DSP y comparación de los 234 tramos MSVC EQ OUT con la referencia Release arm64. Una prueba de continuidad durante reproducción del DAW complementa la medición de transiciones |
| Automatización, undo/redo/reset, estados, tamaño, recreación y varias instancias | Suites Processor y UI con parámetros reales y recall. La automatización grabada y la restauración del proyecto se confirman en el DAW |
| CPU, memoria y estabilidad | Benchmark de callback con tiempos, excesos de plazo y memoria residente del proceso, mediante el working set de Windows. Las comprobaciones Release no sustituyen la instrumentación de memoria en Windows |
| Pantallas y sistema | Verificación física de HiDPI/escalado y smoke en Windows 10 y 11. Un runner Windows Server cubre la construcción y las herramientas, con el smoke en ambos sistemas objetivo pendiente |
