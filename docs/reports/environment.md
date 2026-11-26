# Entorno local inspeccionado — LAB-RC 0.1.0

Fecha: 2026-11-25. Auditoría de rutas, versiones y archivos locales; sin descargar ni instalar herramientas. No acredita licencia comercial ni validación del producto.

| Elemento | Evidencia local |
|---|---|
| macOS | 26.5, build 25F71; máquina arm64 |
| CMake / CTest usados | `/opt/homebrew/bin/cmake`, `/opt/homebrew/bin/ctest`, 4.4.3 |
| Compilador | Apple clang 21.0.0 (clang-2100.1.1.101), `/Library/Developer/CommandLineTools/usr/bin` |
| Developer directory | `/Library/Developer/CommandLineTools`; `xcodebuild -version` falla porque no hay Xcode completo activo |
| CLTools | paquete `com.apple.pkg.CLTools_Executables` 26.6.0.0.1781586589 |
| SDK elegido | `/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk` |
| SDK adicional disponible | `MacOSX15.4.sdk`; ambos anuncian x86_64/arm64 y mínimo 10.13 en `SDKSettings.json` |
| Herramientas macOS | `/usr/bin/make`, `lipo`, `otool`, `codesign`, `auval` |
| AU validator | 1.10.0; binario universal x86_64/arm64e |
| Rosetta | paquete instalado; `arch -x86_64 /usr/bin/uname -m` devolvió x86_64 |
| pluginval | `/Users/serx/Documents/ChatGPT/LAB-T/build/tools/pluginval.app`, 1.0.4, universal arm64/x86_64; licencia GPLv3 en documentación del proyecto de origen |
| Hosts instalados | Logic Pro 13.0, GarageBand 10.4.13; no se han abierto ni alterado |
| Python PATH | `/opt/homebrew/opt/python@3.14/bin/python3.14`, 3.14.6; sin NumPy/SciPy |
| Python CLTools | `/Library/Developer/CommandLineTools/usr/bin/python3`, 3.9.6; sin NumPy/SciPy |
| Entornos Python examinados | Dos venv detectados en Developer, sin NumPy/SciPy; tampoco wheels relevantes encontrados en cachés uv/pip |
| Python auxiliar | uv 0.11.24 disponible; no utilizado para instalar paquetes |

## JUCE reproducible y licencias declaradas en la copia local

Archivo de origen ya existente:

`/Users/serx/Documents/ChatGPT/COMPRESOR EXPERIMENTAL/build/dependencies/juce-8.0.17-security.tar.gz`

SHA-256 verificado:

`d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9`

Se extrae dentro de este workspace a `build/dependencies/juce`. La fuente permanece separada de proyectos ajenos. `scripts/prepare_juce.py` verifica hash y versión antes de extraer y CMake exige el marcador de procedencia. No hay FetchContent ni fallback de red.

`modules/juce_core/juce_core.h` y `CMakeLists.txt` declaran versión 8.0.17. README exige C++17 y CMake ≥3.22. `LICENSE.md` declara los módulos AGPLv3/comercial; el AudioUnitSDK incluido declara Apache-2.0 y el VST3 SDK incluido declara MIT. La posesión del archivo no prueba una licencia comercial. No se ha consultado información privada de cuentas/licencias.

Otras copias encontradas: JUCE 8.0.12 en un proyecto descargado, 8.0.17 en LAB-T y 9.0.2 en VITADAW. La copia LAB-T tiene un parche AU específico de su bypass; LAB-RC usa el archivo limpio indicado arriba.

## Identidad y alcance de plataforma

Los Info.plist de productos VitaPlug instalados muestran mayoritariamente fabricante `Vtpl`, entre otros `Vita`, `VtLb`, `VxLb`, `VpAe`, `Vp9x`. Subtypes examinados: `HmR1`, `LbDy`, `LbTr`, `Imsp`, `Lbq8`, `HhMx`, `Gn80`, `D67L`, `Dlab`, `Ia2a`, `Iaq8`, `L257`, `DlAe`, `Lb2C`, `Dl7q`.

LAB-RC fija fabricante `Vtpl`, subtype `LbRc`, tipo `aufx`, bundle `com.vitaplug.labrc`, versión 0.1.0. No se encontró colisión local; no equivale a una comprobación de registro mundial.

Los presets separan arm64/macOS 11.0 e Intel/macOS 10.15, fijando SDK 26.5. Las órdenes de carga de cada slice y las firmas se comprueban sobre binarios reales antes de empaquetar. No se deduce compatibilidad de esos metadatos sin pruebas en los sistemas de destino.

No se ha encontrado un entorno Windows/MSVC ejecutable local. El proyecto LAB-T contiene un workflow histórico de Windows y un pluginval.exe x64; son referencias, no evidencia de LAB-RC. VST3 Windows y smoke Windows quedan pendientes de build real en Windows o CI autorizado. El prompt menciona Logic 12.2; esta máquina tiene 13.0, por lo que no se puede declarar prueba en 12.2.

## Evidencia final de este entorno

Los builds Release arm64/macOS 11.0 e Intel/macOS 10.15 compilaron AU, VST3 macOS y Standalone; CTest final pasó 5/5 en ambos árboles. Intel se ejecutó mediante Rosetta sobre macOS 26.5, no sobre una máquina Intel con Catalina. Debug arm64 con AddressSanitizer/UndefinedBehaviorSanitizer compiló y pasó también 5/5.

AU/VST3/Standalone universales se verificaron con lipo, otool y codesign estricto en el workspace. La firma es ad hoc, sin Developer ID ni notarización. Pluginval 1.0.4 validó el VST3 universal en sus dos arquitecturas, strictness 5, con las seis tasas y nueve tamaños de bloque del plan; ambos procesos devolvieron 0 y SUCCESS. Su arranque dentro del sandbox había abortado antes de cargar plugin en NSApplication/_RegisterApplication; la ejecución gráfica autorizada funcionó.

El propietario autorizó posteriormente instalar el AU y ejecutar auval. El lead copió únicamente LAB-RC.component al directorio Components del usuario; verificó la firma y coincidencia exacta con el candidato. Auval arm64 y x86_64 devolvieron 0 con AU VALIDATION SUCCEEDED. No se abrió Logic ni otro DAW. El hash del ejecutable instalado y sus logs figuran en docs/reports/installed-au.json y auval-*.log.
