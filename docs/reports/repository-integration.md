# Integración del repositorio oficial de LAB-RC

## Historial y documentación

La fusión `8b88a66c085571f11aac34f4e08161121a37c699` conserva como padres el `main` oficial `6a9988e9b78c4b0210503af5dae7a05f802a00a6` y el historial de desarrollo de LAB-RC hasta `c83f107da593833860683e11c40296e3121cb393`. Se verificó la identidad de los cuatro documentos preexistentes mediante sus objetos Git. El README técnico del desarrollo se incorporó como [README-LAB-RC.md](../../README-LAB-RC.md).

## Archivos incorporados

- Código fuente C++ de LAB-RC 0.2.0, proyecto CMake/JUCE, pruebas, scripts de compilación y empaquetado, instaladores y flujo de Windows.
- Seis PNG definitivos de Final Touch con su manifiesto. Las 13 capturas del editor se cotejaron por tamaño y SHA-256 con el [informe de capturas](final-touch-screenshots.json).
- Las 18 respuestas originales del kit. Se conservan nueve WAV estéreo v05 y nueve matrices true stereo v06 de cuatro canales.
- El [master prompt inicial](../../Assets/ImplementationKit/MASTER_PROMPT.md) y el [brief Final Touch 02](../prompts/LAB-RC_FINAL_TOUCH_02.md) coinciden byte a byte con los originales. El [brief de Windows](../prompts/LAB-RC_WINDOWS_VST3_RELEASE.md) archiva la especificación de la conversación.
- Informes históricos de DSP, EQ, regresión, interfaz y AU, más los [registros nativos de Windows](windows-native-0.2.0/INFORME.md) y la [comparación con AU](windows-native-0.2.0/INFORME-PARIDAD-WINDOWS.md). Se comprobaron el SHA-256 y la integridad del ZIP privado de procedencia antes de extraer los informes.
- Ocho [archivos originales de investigación AcousticLab](../../Assets/ImplementationKit/research), con la geometría v02 y documentación, configuraciones, resultados y registros de v05 y v06. La incorporación de los ZIP completos de AcousticLab v0.3–v0.6 queda pendiente. El kit identifica esos ZIP como archivos separados y no se localizaron entre los materiales disponibles.

Se revisaron `.gitignore` y el historial importado para excluir credenciales, copias locales, temporales, dependencias de compilación descargadas, paquetes de distribución y binarios de plugin. Los informes y manifiestos se conservan en Git. Los renders PCM y los paquetes de compilación permanecen fuera del árbol de `main`.

## Pruebas ejecutadas sobre el árbol fusionado

| Comprobación | Resultado |
| --- | --- |
| Integridad de recursos mediante `scripts/verify_assets.py` | 32 hashes del kit original y seis PNG de Final Touch verificados. Validación independiente adicional de las 18 IR y su suma mono. |
| Compilación de las pruebas Release en macOS arm64 | `labrc_tests` y `labrc_final_touch_regression` compilados. |
| `ctest --preset mac-arm64 --output-on-failure` | 5/5 grupos correctos. DSP, procesador, interfaz, herramientas de compilación y preparación de JUCE. |
| `scripts/test_build_windows.py` en macOS | Siete pruebas correctas. Una omitida porque requiere Windows PowerShell 5.1. El instalador sí se comprobó con `-WhatIf` en la ejecución nativa archivada. |
| Documentación e historial | Los cuatro archivos previos de `main` conservan sus hashes Git y el historial de desarrollo sigue alcanzable. |

## Validación nativa de Windows archivada

La [ejecución privada 45823374424](https://github.com/sergiodevece/LAB-RC_VITAPLUG/actions/runs/45823374424) compiló el VST3 Release x64 con Visual Studio 2022 y MSVC. Superó CTest (3/3), pluginval (nivel 5), Steinberg VST3 Validator (47/47) y la comprobación `-WhatIf` del instalador. Se reutilizó esta evidencia ya registrada para el mismo código de producto.

La regresión con las EQ desactivadas comparó 234 segmentos y 213.520.746 muestras contra la referencia arm64 de la versión AU. El pico residual fue de `3,57627869 × 10⁻⁷ FS` y el RMS de `4,80121404 × 10⁻⁹ FS`, con 128 muestras de latencia. El smoke en un DAW de Windows queda pendiente.
