# Build y validadores — LAB-RC 0.1.0

Plan: `docs/superpowers/plans/2026-11-25-lab-rc.md`, Task 4. Registro de implementación y evidencia; los estados se actualizan tras cada ejecución real.

## Decisiones de integración

- Ruling: JUCE se obtiene del archivo local ya inspeccionado y se extrae limpio en `build/dependencies/juce`; no se enlaza la copia LAB-T parcheada. Evita heredar cambios específicos de otro producto; exige conservar el pin y documentar cualquier parche futuro.
- Ruling: se usan dos builds Release por arquitectura para fijar mínimos distintos (arm64 11.0, Intel 10.15). El universal se produce únicamente después de verificar ambos slices; nunca se infiere un build Windows desde macOS.
- Ruling: las capturas usan un ejecutable sin device de audio que construye processor/editor reales en el message thread; no se incluye API de screenshots en producción. Captura min/default/max y default@2x en cada sala.
- Ruling: el runner falla si una categoría no registra tests, evitando éxito por suite vacía. CTest no lanza un host ni abre un dispositivo.
- Revisión QA: `EDITOR_WANTS_KEYBOARD_FOCUS` pasa a TRUE. Verificado en JUCE local: `juce_VSTWindowUtilities.h:56` añade `windowIgnoresKeyPresses` si el macro es falso; el wrapper AU también decide foco con ese macro. La interfaz tiene controles de teclado y valores editables, por lo que TRUE es necesario. El rebuild y las pruebas finales deben usar el macro 1; la prueba de teclado en DAW sigue siendo humana.
- Ruling: instalación/desinstalación requieren invocación explícita del script; no forman parte de build ni paquete. Solo actúan sobre `LAB-RC.component`, id `com.vitaplug.labrc`, sin sudo. Las pruebas de los scripts usan fixtures dentro del workspace.
- Revisión QA I8: bootstrap cambia de `/usr/bin/tar` a `tarfile` stdlib para no depender de esa ruta macOS. Conserva hash/version obligatorios, valida todas las rutas antes de extraer y rechaza traversal, symlinks/hardlinks, tipos especiales y duplicados. Se probó sin binario tar disponible y con el archivo JUCE real; build/smoke Windows siguen pendientes.
- Regresión propia de capturas: JUCE FileOutputStream abre al final del archivo. Un segundo render provocaba PNG concatenados y mostraba la captura antigua. El checker independiente detectó bytes después de IEND (RED); el renderer ahora posiciona/trunca antes de escribir. Tras recompilar y regenerar, las 12 PNG pasan CRC, dimensiones y fin exacto IEND (GREEN), y están copiadas en docs/screenshots.

## Registro hasta ahora

| Comprobación | Resultado observado |
|---|---|
| Hash JUCE local | Coincide con pin `d37bab9f…c9` |
| Extracción local limpia | `build/dependencies/juce`, versión 8.0.17 |
| `cmake --preset mac-dsp` | Exit 0; juceaide configurado y compilado con CLTools |
| `cmake --build --preset mac-dsp --target labrc_tests` | Exit 0; runner DSP disponible |
| `cmake --preset mac-arm64` y build `labrc_tests` | Exit 0 sin errores de compilación; SDK 26.5 y deployment 11.0 |
| Processor RED inicial | Exit 1, 9 secciones, 68 checks pasan y 47 fallan por recursos/estado/undo/gestos no implementados; log `build/evidence/processor-red.log` |
| Processor GREEN inicial | Exit 0, 13 secciones/139 checks pasan/0 fallos; incluye integración de audio |
| UI edición numérica RED | Exit 1, 6 secciones/2180 checks pasan/1 fallo: `3-2 dB` se aceptaba como 3 dB; propietario UI corrigió parser y se verificará de nuevo |
| Scripts de instalación, RED | Dos fallos de comportamiento por scripts inexistentes, tras corregir el fixture para no copiar flags SIP de `/usr/bin/true` |
| `python3 -B scripts/test_build_tools.py` | 5/5 pruebas pasan: instalar/remover conservando otro bundle, dry-run, rechazo de identidad ajena, symlink y destino inválido |
| Bootstrap offline, RED→GREEN | 3 fallos reproducidos antes del fix y después 4/4 pruebas pasan; incluye hash incorrecto, tar indisponible, traversal y symlink |
| Bootstrap archivo real vía stdlib | Exit 0 con hash pin; se extrajo y eliminó únicamente un fixture dentro de build/ |
| Release intermedio arm64 | AU/VST3/Standalone y renderer compilan exit 0; CTest 4/4 pasa, 12 PNG con tamaños correctos; anterior a revisiones finales |
| Release intermedio x86_64 | AU/VST3/Standalone compilan exit 0; CTest 4/4 pasa bajo Rosetta; anterior a revisiones finales |
| ASan/UBSan core | `labrc_dsp` compila exit 0 en Debug arm64; falta suite completa |
| Capturas finales UI | 12 PNG pasan el checker independiente y están copiadas en docs/screenshots; Center Field 800×500 muestra seis nombres/pares L/R íntegros |
| Release final arm64 | AU/VST3/Standalone y renderer build exit 0; CTest 5/5 PASS en 29,76 s, después de los fixes I1–I9 |
| Release final x86_64 | AU/VST3/Standalone build exit 0; CTest 5/5 PASS bajo Rosetta en 27,96 s |
| ASan/UBSan final | Debug arm64 build exit 0; CTest 5/5 PASS en 116,69 s, sin diagnóstico sanitizer en la ejecución |
| Foco de teclado, wrappers finales | Flags reales de VST3 arm64 e Intel contienen `JucePlugin_EditorRequiresKeyboardFocus=1`; prueba física de teclado en host pendiente |
| Universal local | AU/VST3/Standalone fusionados y verificados en build/universal: arm64 mínimo 11.0, x86_64 mínimo 10.15, bundle/versión/códigos AU correctos y firma ad hoc estricta válida |
| pluginval arm64 | 1.0.4, strictness 5, seed 314159, tasas 44100/48000/88200/96000/176400/192000 y bloques 1/32/64/127/128/256/512/1024/2048; SUCCESS, exit 0, un plugin descubierto |
| pluginval x86_64 | Misma configuración y mismo VST3 universal bajo Rosetta; SUCCESS, exit 0, un plugin descubierto |

El agente DSP controló las recompilaciones de los fixes y su benchmark final antes de reanudar los builds anteriores. Los logs finales de Build están en `build/evidence/build-{arm64,x86_64,sanitize}-final.log`, `ctest-{arm64,x86_64,sanitize}-final.log` y sus XML. Los validadores guardan `build/evidence/pluginval-{arm64,x86_64}/process.log`. No confundir los tiempos CTest, tomados con trabajo paralelo, con mediciones de deadline de audio: estas pertenecen al informe DSP.

El plugin validado por pluginval es VST3 macOS. Sus apartados internos llamados “auval” no validan el AU de esta entrega. El validador Steinberg externo se omitió por no estar configurado y así consta en el log.

Después, el propietario autorizó explícitamente instalar el componente y ejecutar auval. El lead instaló únicamente `~/Library/Audio/Plug-Ins/Components/LAB-RC.component`, sin abrir Logic. El AU instalado pasó la verificación universal estricta y es idéntico al candidato: SHA-256 del ejecutable `6532f24cd0f908c9db627a966f2d8660327dfa360338bdc6a1031d0d49430f58` (`docs/reports/installed-au.json`). Los dos procesos auval, arm64 y x86_64 bajo Rosetta, devolvieron 0 y `AU VALIDATION SUCCEEDED.`; constan en `docs/reports/auval-arm64.log` y `auval-x86_64.log`. No se abrió un DAW. El empaquetador exige que su ejecutable AU coincida con este hash para asociar esa validación al ZIP entregado.

La distribución funcional de la UI se conserva para el smoke físico solicitado; el propietario ha indicado que el acabado visual **no está aprobado** y ha pedido no seguir modificándolo en esta iteración. Las capturas son evidencia del estado actual, no aceptación estética. No se ha implementado EQ por pareja: pertenece a una posible segunda iteración.

## Comandos reproducibles

Desde la raíz del repositorio, con CMake 4.4.3 usado en esta máquina:

```sh
python3 -B scripts/prepare_juce.py --archive '/ruta/local/juce-8.0.17-security.tar.gz'
cmake --preset mac-arm64
cmake --build --preset mac-arm64
ctest --preset mac-arm64
build/mac-arm64/tests/labrc_tests --category Processor
build/mac-arm64/tests/labrc_tests --benchmark
build/mac-arm64/tools/labrc_render_ui --output "$PWD/build/evidence/screenshots"
```

Build completo por slices, pruebas, capturas y paquete, sin instalar:

```sh
python3 -B scripts/build_macos.py --arch universal --screenshots --package
python3 -B scripts/package_macos.py --au-only
python3 -B scripts/package_sources.py
```

El paquete se crea en `dist/LAB-RC-0.1.0-macOS-universal.zip`, solo si existen los dos builds reales. Verifica AU, VST3 macOS y Standalone, arquitecturas, `LC_BUILD_VERSION`/`LC_VERSION_MIN_MACOSX`, id/versión/códigos AU y firma ad hoc, y escribe hashes. VST3 macOS y Standalone son auxiliares de QA. No incluye ni anuncia VST3 Windows.

La opción `--au-only` prioriza `dist/LAB-RC-0.1.0-AU-universal.zip`, con el AU, scripts, documentos de smoke/pruebas, reports JSON/logs, manifiesto original como `asset-manifest.json` y licencias. El bundle es `AU/LAB-RC.component`; el archivo ZIP no se instala automáticamente. Los hashes finales están en el sidecar `.zip.sha256`, para evitar un hash circular dentro de documentos que forman parte del propio ZIP.

Antes de la entrega final, `python3 -B scripts/package_macos.py --merge-only` crea un candidato en build/universal para validar el VST3 universal por ruta con pluginval. No instala ni crea el ZIP de distribución. `scripts/validate_macos.py --pluginval /ruta/local/pluginval --bundle build/universal/VST3/LAB-RC.vst3 --arch arm64` guarda sus logs bajo build/evidence; repetir con x86_64 valida la otra arquitectura. La aplicación pluginval debe poder registrar su proceso en la sesión gráfica; bajo el sandbox abortó antes de cargar el plugin, en `_RegisterApplication`/`NSApplication`.

El ZIP de fuentes usa una whitelist: Source, Tests, Assets, cmake, scripts, docs, CMakeLists/CMakePresets y README/LICENSE/CHANGELOG. Incluye las IR/fotos y manifiesto del kit para trabajo offline; excluye build, dependencias, .git, .superpowers, dist, cachés Python, qa-review.diff y todos los docs/briefs/*.diff intermedios. Tiene SOURCE_MANIFEST.json con hashes por archivo, CRC verificados y SHA-256 exterior. El archivo JUCE no se redistribuye dentro del ZIP de fuentes; hay que aportar el pin local autorizado.

Instalación manual fuera del workspace, únicamente después de autorización concreta:

```sh
bash scripts/install_macos.sh 'dist/LAB-RC-0.1.0-macOS-universal/AU/LAB-RC.component' --dry-run
bash scripts/install_macos.sh 'dist/LAB-RC-0.1.0-macOS-universal/AU/LAB-RC.component'
auval -strict -v aufx LbRc Vtpl
arch -x86_64 auval -strict -v aufx LbRc Vtpl
bash scripts/uninstall_macos.sh --dry-run
```

La instalación y auval anteriores se ejecutaron después de la autorización explícita del propietario. No hay firma Developer ID, notarización ni cambios globales de Gatekeeper.

## Procedimiento Windows propuesto, no ejecutado

Requiere Windows real, Visual Studio 2022 con herramientas C++ x64/SDK y CMake ≥3.22, además del archivo JUCE local con el hash fijado. No descarga dependencias automáticamente ni usa una ruta `/usr/bin/tar`:

```powershell
python scripts/prepare_juce.py --archive 'C:\ruta-local\juce-8.0.17-security.tar.gz'
python scripts/test_prepare_juce.py
cmake -S . -B build/windows-x64 -G 'Visual Studio 17 2022' -A x64
cmake --build build/windows-x64 --config Release --parallel 4
ctest --test-dir build/windows-x64 -C Release --output-on-failure
```

La comprobación portable del bootstrap en macOS no ejecutó un compilador Windows. Estas órdenes todavía deben verificarse en esa plataforma, junto con el paquete VST3 x64 y un host Windows. No existe un artefacto Windows de LAB-RC en esta entrega local.

## Pendiente

Crear los ZIP definitivos después de la reconciliación de QA y documentación del lead. La instalación consentida y auval de ambas arquitecturas ya pasan. Smoke humano de Logic, escucha/A-B de las nueve IR, aceptación estética y prueba física de teclas/HiDPI siguen pendientes. Windows sigue pendiente de plataforma real. No hay firma Developer ID ni notarización.
