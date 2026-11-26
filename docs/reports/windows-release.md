# LAB-RC 0.2.0 · preparación del VST3 Windows x64

El checkpoint de 0.2.0 guarda el AU instalado, el historial de Git y la comprobación de integridad. El ejecutable del AU y su copia comparten el SHA-256 `5bf36ca80b635aea63e25f9d1e28cabe68774437231922a286ee6c16553e4541`. La copia del AU pasó la verificación de firma en ambas arquitecturas. Las 18 IR originales y los recursos Final Touch pasaron los hashes de sus manifiestos.

En macOS arm64 pasaron las cinco suites de CTest Release y las cinco con ASan/UBSan. Pasaron también las siete pruebas del empaquetador y la comprobación estática del workflow y del manifiesto que lee el instalador. La comparación local de la FFT portable con Accelerate cubrió 234 tramos, con pico residual de `3,5763 × 10⁻⁷ FS` y RMS de `4,5325 × 10⁻⁹ FS`. [Método y evidencia de paridad](windows-parity.md)

## Pruebas preparadas para el runner

| Requisito | Comprobación |
|---|---|
| Release x64 e instalación | CMake con MSVC 2022. Inspección del DLL PE32+ AMD64, export, dependencias, identidad, instalador y SHA-256 del ZIP |
| Formato VST3 | pluginval nivel 5. El workflow intenta compilar el Validator oficial de Steinberg desde la revisión fijada del SDK y admite un ejecutable local verificado |
| Audio y mezcla | CTest DSP y Processor con matrices originales, rutas mono a estéreo y estéreo a estéreo, latencia de 128 muestras, fundidos, automatización, EQ y valores finitos |
| Estado y editor | CTest Processor y UI con identidad de parámetros, recall legado, undo, redo, reset, tamaño, recreación del editor y aislamiento entre instancias |
| Paridad entre plataformas | Render independiente con EQ OFF desde el procesador compilado con MSVC y comparación con el procesador Release de la versión AU 0.2.0. El informe registra diferencias de bits, pico, RMS y latencia |
| CPU y memoria | Benchmark Release nativo con tiempos de callback, excesos de plazo y memoria del proceso. Los valores corresponden al equipo de prueba |

La comparación usa el procesador y los estados internos compartidos por AU y VST3. La carga de proyectos de Logic y el comportamiento de un DAW Windows se verifican en sus respectivos hosts. El smoke en Windows 10/11, HiDPI real y la escucha quedan separados del resultado del runner.

## Resultados del runner Windows

- Release AMD64 compilado con MSVC 19.44.35230.0 en Windows Server 2022. El bundle pasó la inspección PE32+ y la comprobación de dependencias. El SHA-256 del DLL es `a4a116e794d5b30393b5a8a9fc4deb48309daf975400ec67cbb8b5f5b88eaedc`.
- CTest pasó las tres suites. pluginval pasó a nivel 5. El VST3 Validator oficial de Steinberg pasó 47 pruebas. La simulación `-WhatIf` sobre el DLL empaquetado pasó en Windows PowerShell 5.1.
- El wrapper VST3 de Windows etiqueta `Default` al programa existente y rechaza frecuencias fuera del rango de preparación del motor. Steinberg confirmó el rechazo de 1.234.567,8 Hz y terminó su suite con cero fallos. El código del procesador y del DSP coincide con el checkpoint.

Mediciones del benchmark en el runner virtual con EQ apagada y bloques de 64 muestras. El tiempo es de callback y la memoria corresponde al proceso completo.

| Frecuencia | p99 | Excesos de plazo | Memoria residente |
|---|---:|---:|---:|
| 48 kHz | 403,70 µs | 0 de 4.500 | 34,5 MiB |
| 96 kHz | 464,30 µs | 16 de 9.000 | 57,3 MiB |
| 192 kHz | 568,50 µs | 2.139 de 18.000 | 103,8 MiB |

La validación de carga y el benchmark corresponden al binario nativo. El smoke en el DAW y el escalado HiDPI en Windows 10/11 se comprueban en el equipo del estudio. [Ejecución privada](https://github.com/sergiodevece/LAB-RC_VITAPLUG/actions/runs/45823374424)

## Paridad y paquete final

La comparación Windows x64 frente a la referencia del AU pasó en 234 tramos y 213.520.746 muestras con EQ apagada. El residual fue de `3,5762786865234375e-7 FS` de pico y `4,801214036803412e-9 FS` RMS, con 128 muestras de latencia en ambas plataformas. El archivo final incluye las mediciones y los registros nativos. El DLL conserva su hash del runner. [Paridad nativa](windows-parity.md#resultado-nativo-windows)

El ZIP final `LAB-RC-Windows-x64-VST3.zip` tiene SHA-256 `e529be4e3ec45c7620f591cdf831669e22de9088b8c300ad5d95d922aad826d1`. Se verificaron el CRC y los hashes internos y externos. Se dejó una copia en Descargas junto a su archivo `.sha256`.
