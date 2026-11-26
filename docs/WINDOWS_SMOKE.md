# LAB-RC 0.2.0 Final Touch para Windows x64

## Instalación del VST3 compilado

Descomprime el paquete Release y cierra el DAW. Abre PowerShell x64 como administrador en la carpeta extraída. El instalador verifica el binario incluido y mantiene respaldo de una versión anterior al reemplazarla.

~~~powershell
powershell.exe -NoProfile -File .\install.ps1 -Action Install
powershell.exe -NoProfile -File .\install.ps1 -Action Uninstall
~~~

Para restaurar el respaldo, usa la ruta que imprimió el instalador con `-Action Restore -BackupPath 'C:\…\LAB-RC.vst3'`. Puedes añadir `-WhatIf` para ver la operación prevista.

También puedes copiar la carpeta completa LAB-RC.vst3 a C:\Program Files\Common Files\VST3. Guarda primero una copia del bundle anterior. Para desinstalar manualmente, retira esa carpeta con el DAW cerrado. Vuelve a escanear los módulos después de instalar o desinstalar. [Ubicaciones VST3 de Steinberg](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html)

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

El comando registra cada paso en build\windows-x64\validation-* y genera dist\LAB-RC-Windows-x64-VST3.zip con su SHA-256 al completar las comprobaciones. Si tienes el VST3 Validator x64 de Steinberg, añade --steinberg-validator con la ruta a validator.exe. [VST3 Validator de Steinberg](https://steinbergmedia.github.io/vst3_dev_portal/pages/What+is+the+VST+3+SDK/Validator.html)
