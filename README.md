# LAB-RC · Manza’s Rooms & Chambers

**A Tribute to Steve Albini · VITALAB / VITAPLUG · 0.2.0 Final Touch**

Mezclador de microfonía ambiental C++17/JUCE. Kentucky y Center Field (Studio A) y Live Room (Studio B) utilizan las nueve matrices wet v0.6 originales del kit. Es un homenaje independiente con respuestas **sintéticas**; no es una captura de Electrical Audio ni un producto oficial.

El propietario aprobó el carácter acústico de 0.1.0 tras probar el AU en Logic Pro y validó las tres capturas de Final Touch. 0.2.0 añade una EQ estéreo por pareja. Se conserva 0.1.0 como referencia para la comparación con las tres EQ desactivadas. La escucha de 0.2.0 en Logic queda pendiente. [Informe de Final Touch](docs/FINAL_TOUCH_REPORT.md)

## Origin & Authenticity — The Steve Albini Legacy

**ES.** Las posiciones de las parejas de micrófonos respecto a la batería se basan en los apuntes personales que Sergio Devece (SerxDev), creador de LAB-RC junto a Compañero (ChatGPT), tomó durante el seminario de Steve Albini en Studios La Fabrique, en Saint-Rémy-de-Provence. Mix With The Masters organizó el seminario del 19 al 25 de febrero de 2018. Los apuntes recogen las enseñanzas de microfonía. Las coordenadas complementarias se estimaron para completar el modelo de LAB-RC. Las salas e IR se sintetizaron para el plugin tomando Electrical Audio como inspiración. LAB-RC es un homenaje independiente. [Archivo del seminario](https://mixwiththemasters.com/events/history/seminars)

**EN.** The microphone pairs’ placement around the drum kit draws on the personal notes that Sergio Devece (SerxDev), who created LAB-RC with Compañero (ChatGPT), took at Steve Albini’s seminar at Studios La Fabrique in Saint-Rémy-de-Provence. Mix With The Masters ran the seminar from February 19 to 25, 2018. The notes document the microphone techniques taught there. Additional coordinates were estimated to complete the LAB-RC model. The rooms and impulse responses were synthesized for the plugin, with Electrical Audio as inspiration. LAB-RC is an independent tribute. [Seminar archive](https://mixwiththemasters.com/events/history/seminars)

## Compilación local reproducible

Herramientas utilizadas: CMake/CTest 4.4.3, Apple clang 21.0.0, JUCE 8.0.17 del archivo local fijado por SHA-256. CMake mínimo 3.22; C++17. No hay descarga automática de dependencias ni conexiones de red del plugin.

```sh
python3 scripts/verify_assets.py --final-touch Assets/FinalTouch
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

Cada pareja EARS, FRONT y FLOOR tiene una EQ estéreo enlazada tras la convolución y el fundido de sala. Le siguen su fader, MUTE/SOLO y medidor. Mute prevalece y se permiten varios solos. Los ajustes de cada pareja se conservan al cambiar de sala. MIX es lineal de 0 % dry a 100 % wet. BYPASS entrega la entrada original a unidad, compensada por la latencia de cálculo. La señal dry queda fuera de la EQ.

| Banda | Curva | Frecuencia | Ganancia |
|---|---|---:|---:|
| LOW | Estante bajo, S = 1 | 100 Hz | ±12 dB |
| MID | Campana, Q = 0,7 | 1 kHz | ±12 dB |
| HIGH | Estante alto, S = 1 | 8 kHz | ±12 dB |

Los controles arrancan a 0 dB con EQ IN desactivado. Las transiciones de ganancia y de EQ IN duran 20 ms. La EQ conserva la latencia de 128 muestras del motor. Los realces pueden aumentar el nivel de pico.

Los motores y recursos se preparan fuera del callback; el procesamiento conserva los retardos físicos de las IR. Ganancias y bypass usan rampas de 20 ms; el cambio de sala atenúa la cola anterior mediante crossfade complementario de 50 ms. No se promete spillover íntegro. Las parejas muteadas siguen procesando para conservar continuidad. Se reportan al host latencia computacional y tail conservador.

La automatización de sala con audio activo espera a que el banco esté preparado en segundo plano. El instante de inicio de ese fundido depende de la preparación; no se promete selección de sala sample accurate ni un instante idéntico en dos bounces con automatización rápida. El recall inicial, antes del primer audio, se verifica por separado para que empiece en la sala guardada.

Fs previstas: 44.1, 48, 88.2, 96, 176.4 y 192 kHz. Consultar evidencias y límites de CPU en el informe DSP. Un fader en −60 dB sigue siendo distinto de mute matemático.

El procesamiento propio no asigna/libera memoria, toma mutex ni accede a archivos en `processBlock`. Esta afirmación no se extiende a toda la API del host: los wrappers y las notificaciones de parámetros de JUCE contienen locks del framework. La GUI lee snapshots desde su timer y evita publicar mensajes desde el render.

## Interfaz y estado

La referencia visual de Final Touch es `LAB-RC_REFERENCIA_VINTAGE_APROBADA.png`. Tres fondos originales sin batería conservan su proporción. El PNG transparente BOOST THIS se compone encima, con ajustes de escala, perspectiva, luz y sombra por sala. MICS muestra u oculta los seis marcadores de las posiciones fijas.

Parámetros DSP automatizables y estado versionado por instancia. Los gestos de usuario tienen undo/redo local. RESET agrupa los ajustes en una sola transacción. Los proyectos de 0.1.0 recuperan sus valores previos y reciben las tres EQ desactivadas a 0 dB. La visibilidad de marcadores y el tamaño se guardan como preferencias por instancia. Las posiciones fijas siguen siendo parte del diseño. El movimiento de micrófonos queda como posible investigación futura.

## Validación y distribución

Integridad de assets, tests de software y escucha humana son validaciones separadas. El Python principal no tenía NumPy/SciPy: el verificador original se conserva intacto y el Gate 0 utiliza una implementación independiente con stdlib que añade comprobación PNG/WAV. Esto se declara en los logs.

Windows requiere un build Windows x64 real y smoke posterior. Logic 13.0 está instalado en esta máquina; la especificación menciona Logic 12.2. No se debe extrapolar compatibilidad de un host o plataforma a otro.

Nada se publica automáticamente en GitHub. La instalación local fuera de este workspace es una acción separada. Consultar [LICENSE.md](LICENSE.md) y [docs/PROVENANCE.md](docs/PROVENANCE.md) antes de distribuir.
