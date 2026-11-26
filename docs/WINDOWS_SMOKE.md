# LAB-RC 0.2.0 Final Touch para Windows x64

## Instalación del VST3 compilado

Descomprime el paquete Release y cierra el DAW. Copia la carpeta completa LAB-RC.vst3 a %CommonProgramFiles%\VST3 y vuelve a escanear los módulos desde el DAW. La carpeta contiene el binario en Contents\x86_64-win\LAB-RC.vst3. Si ya tienes una copia de LAB-RC, guarda el bundle anterior antes de sustituirlo. [Ubicaciones VST3 de Steinberg](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html)

## Smoke en el DAW x64

1. Inserta LAB-RC en una pista estéreo. Comprueba que el gestor del DAW muestra VITAPLUG y la versión 0.2.0.
2. Reproduce una pista de batería con las tres EQ en OFF. Cambia entre Kentucky, Center Field y Live Room. Prueba dry/wet, bypass, faders y mute/solo.
3. Activa EQ IN por separado en EARS, FRONT y FLOOR. Ajusta LOW, MID y HIGH en ambas direcciones y comprueba que afecta a la pareja correspondiente. Verifica que cada EQ conserva los ajustes al cambiar de sala.
4. Automatiza un control de EQ y guarda una copia del proyecto. Ciérralo y ábrelo para verificar la automatización, la sala y los valores recuperados. Abre y redimensiona el editor.
5. Anota DAW y versión, versión de Windows, frecuencia de muestreo, tamaño de bloque y cualquier fallo. El resultado del DAW es independiente de CTest y pluginval.

## Compilación local desde el kit de fuentes

Usa Windows x64 con Python 3.9 o posterior, CMake y CTest 3.22 o posterior en PATH y Visual Studio 2022 o Build Tools 2022. Selecciona la carga de trabajo **Desarrollo para el escritorio con C++**, el compilador MSVC v143 x64 y un SDK de Windows. [Componentes de Visual Studio Build Tools](https://learn.microsoft.com/en-us/visualstudio/install/workload-component-id-vs-build-tools?view=visualstudio)

Extrae el kit completo a una ruta local corta. En PowerShell, sitúate en la carpeta LAB-RC del kit y ejecuta:

~~~powershell
py -3 -B scripts/build_windows.py --juce-archive ..\juce-8.0.17-security.tar.gz
~~~

Para validar la carga del VST3 con pluginval, descomprime el ZIP del validador incluido en el kit y pasa la ruta de su ejecutable con --pluginval. [Uso de pluginval](https://github.com/Tracktion/pluginval#running-in-headless-mode)

~~~powershell
Expand-Archive ..\pluginval_Windows.zip ..\pluginval
py -3 -B scripts/build_windows.py --juce-archive ..\juce-8.0.17-security.tar.gz --pluginval ..\pluginval\pluginval.exe
~~~

El comando registra cada paso en build\windows-x64\validation-* y genera dist\LAB-RC-0.2.0-VST3-Windows-x64.zip con su SHA-256 al completar las comprobaciones.
