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

El workflow se conserva localmente. El build nativo, los validadores Windows y la comparación Windows contra macOS siguen pendientes de acceso a un runner autorizado. El kit de compilación contiene fuentes y herramientas. El ZIP de Release se genera a partir del DLL compilado y verificado.
