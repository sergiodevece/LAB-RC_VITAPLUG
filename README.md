# LAB-RC · Manza’s Rooms & Chambers

**A Tribute to Steve Albini · VITALAB / VITAPLUG · 0.1.0 · desarrollo local**

Mezclador de microfonía ambiental C++17/JUCE. Kentucky y Center Field (Studio A) y Live Room (Studio B) utilizan las nueve matrices wet v0.6 originales del kit. Es un homenaje independiente con respuestas **sintéticas**; no es una captura de Electrical Audio ni un producto oficial.

Las referencias v0.5 fueron aprobadas mediante escucha por el propietario. La equivalencia matemática v0.6 no sustituye la escucha del plugin en un DAW. El estado de cada gate se registra en [docs/ACCEPTANCE.md](docs/ACCEPTANCE.md).

El propietario ha validado la distribución funcional de la interfaz, pero **no ha aprobado el acabado gráfico**: todavía arrastra rasgos del prototipo descartado. La UI queda congelada en esta fase. Se prioriza un AU instalable para el smoke físico en Logic Pro. El diseño industrial definitivo y una EQ de tres bandas por pareja se reservan para una segunda iteración posterior a la aprobación sonora/funcional; esta versión no incorpora EQ.

## Compilación local reproducible

Herramientas utilizadas: CMake/CTest 4.4.3, Apple clang 21.0.0, JUCE 8.0.17 del archivo local fijado por SHA-256. CMake mínimo 3.22; C++17. No hay descarga automática de dependencias ni conexiones de red del plugin.

```sh
python3 scripts/verify_assets.py
python3 scripts/prepare_juce.py --archive /ruta/local/juce-8.0.17-security.tar.gz
```

`prepare_juce.py` explica cómo aportar el archivo JUCE local autorizado. El archivo aprobado durante esta sesión es `juce-8.0.17-security.tar.gz`, SHA-256 `d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9`. Se extrae en `build/dependencies/juce`; las dependencias no forman parte de los paquetes de fuentes.

```sh
cmake --preset mac-arm64
cmake --build --preset mac-arm64
ctest --preset mac-arm64
cmake --preset mac-x86_64
cmake --build --preset mac-x86_64
ctest --preset mac-x86_64
```

Los presets fijan macOS 11.0 para arm64 y 10.15 para x86_64. La compilación con SDK moderno y esos targets no demuestra ejecución en sistemas antiguos. Los scripts de empaquetado comprueban los slices y la firma local; consultar `--help` antes de utilizarlos. El build no instala plugins.

El driver puede ejecutar ambas arquitecturas, tests, capturas y paquete: `python3 scripts/build_macos.py --arch universal --screenshots --package`. La preparación del archivo JUCE sigue siendo local y explícita. Consultar también `python3 scripts/validate_macos.py --help` para pluginval local por ruta VST3.

También existe `mac-dsp` para pruebas sin editor y `mac-sanitize` para ASan/UBSan. Los comandos y resultados realmente ejecutados están en `docs/reports/`.

## Audio

Se admiten entradas mono→estéreo y estéreo→estéreo; float32. Mono duplica la entrada a unidad en las dos columnas de la matriz. Orden WAV `[LL, LR, RL, RR]`:

```
yL = xL * hLL + xR * hRL
yR = xL * hLR + xR * hRR
```

Cada pareja EARS/FRONT/FLOOR tiene ganancia posterior a convolución y MUTE/SOLO. Mute prevalece; se permiten varios solos. Las ganancias son globales entre salas. MIX es lineal: 0% dry, 100% wet. Los trims afectan a la ruta procesada; BYPASS entrega la entrada original a unidad, compensada por la latencia computacional. No hay limitador ni igualación oculta de salas.

Los motores y recursos se preparan fuera del callback; el procesamiento conserva los retardos físicos de las IR. Ganancias y bypass usan rampas de 20 ms; el cambio de sala atenúa la cola anterior mediante crossfade complementario de 50 ms. No se promete spillover íntegro. Las parejas muteadas siguen procesando para conservar continuidad. Se reportan al host latencia computacional y tail conservador.

La automatización de sala con audio activo espera a que el banco esté preparado en segundo plano. El instante de inicio de ese fundido depende de la preparación; no se promete selección de sala sample accurate ni un instante idéntico en dos bounces con automatización rápida. El recall inicial, antes del primer audio, se verifica por separado para que empiece en la sala guardada.

Fs previstas: 44.1, 48, 88.2, 96, 176.4 y 192 kHz. Consultar evidencias y límites de CPU en el informe DSP. Un fader en −60 dB sigue siendo distinto de mute matemático.

El procesamiento propio no asigna/libera memoria, toma mutex ni accede a archivos en `processBlock`. Esta afirmación no se extiende a toda la API del host: los wrappers y las notificaciones de parámetros de JUCE contienen locks del framework. La GUI lee snapshots desde su timer y evita publicar mensajes desde el render.

## Interfaz y estado

Chasis vintage basado en `approved_vintage.png`, con controles dinámicos independientes. Las tres fotografías aprobadas se muestran íntegras con contain; MICS solo muestra u oculta seis marcadores gráficos aproximados. No cambia sonido ni posición acústica.

Parámetros DSP automatizables y estado versionado por instancia. Los gestos de usuario tienen undo/redo local; la automatización del host no genera acciones de undo en el plugin. RESET agrupa su cambio en una sola transacción. La visibilidad de marcadores y el tamaño se guardan como preferencias por instancia.

## Validación y distribución

Integridad de assets, tests de software y escucha humana son validaciones separadas. El Python principal no tenía NumPy/SciPy: el verificador original se conserva intacto y el Gate 0 utiliza una implementación independiente con stdlib que añade comprobación PNG/WAV. Esto se declara en los logs.

Windows requiere un build Windows x64 real y smoke posterior. Logic 13.0 está instalado en esta máquina; la especificación menciona Logic 12.2. No se debe extrapolar compatibilidad de un host o plataforma a otro.

Nada se publica automáticamente en GitHub. La instalación local fuera de este workspace es una acción separada. Consultar [LICENSE.md](LICENSE.md) y [docs/PROVENANCE.md](docs/PROVENANCE.md) antes de distribuir.
