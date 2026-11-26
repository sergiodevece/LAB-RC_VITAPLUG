# LAB-RC 0.2.0 Final Touch — build y validación

Especificación y plan completos leídos: `docs/superpowers/specs/2026-11-26-lab-rc-final-touch.md` y su plan homónimo. Esta fase preserva los artefactos, checkpoint y AU instalado 0.1.0. No se ha instalado 0.2.0; para sustituir el componente del usuario se requiere autorización explícita nueva. No se abre Logic ni se publica o descarga nada.

## Entorno y decisiones

- Misma toolchain local de 0.1: CMake/CTest 4.4.3, Apple clang 21, JUCE 8.0.17-security desde el archivo local SHA-256 `d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9`, SDK macOS 26.5. CMake mínimo 3.22, C++17, sin fast-math ni características de red JUCE.
- Identidad conservada: `aufx/LbRc/Vtpl`, `com.vitaplug.labrc`; versión 0.2.0. Release separado arm64/macOS 11.0 e Intel/macOS 10.15. Intel se ejecuta bajo Rosetta en esta máquina, no en Catalina físico.
- CMake embebe nueve matrices v06 intactas y seis PNG Final Touch: tres fondos limpios originales, batería transparente y dos recortes de material aprobados. No embebe v05 ni la escena vintage de referencia completa.
- Los resultados nuevos usan exclusivamente `docs/reports/final-touch-*`. El wrapper pluginval comprueba la versión del candidato antes de ejecutarlo, para no atribuir evidencia 0.1 a 0.2.
- El candidato se fusionará en `build/universal-0.2.0`; el paquete privado será `dist/LAB-RC-0.2.0-AU-universal.zip`. No se sustituyen los ZIP 0.1 ni se necesita otro ZIP de fuentes.
- Si el propietario autoriza auval, el lead instalará 0.2 solo temporalmente y restaurará exactamente el checkpoint 0.1 al terminar. La identidad de hash del paquete se asocia al AU validado temporalmente, no a la versión que permanezca instalada. El recibo y metadata registran `checkpoint_restored` expresamente.
- Render real con processor/editor en el message thread, sin abrir device de audio: tres salas a mínimo/inicial/máximo/inicial@2x, más detalle de controles, en `docs/screenshots/final-touch`. El propietario aprobó expresamente las capturas y el acabado Final Touch tras ver la terna; el smoke humano de audio 0.2 permanece separado y pendiente.

## Registro

| Acción | Evidencia observada |
|---|---|
| Shared Release arm64 inicial | `cmake --build --preset mac-arm64 --target LabRC` exit 0; archive `build/mac-arm64/LabRC_artefacts/Release/libLAB-RC_SharedCode.a` disponible para el null de QA |
| Log | `docs/reports/final-touch-build-arm64-core.log` |
| UI RED | Build exit 0, categoría UI exit 1: 2237 checks pasan y 15 fallan (12 controles EQ aún ausentes y tres expectativas de tamaño 0.1); propietario UI recibió `final-touch-ui-red.log` |
| Cero exacto EQ | La sonda independiente QA reprodujo el defecto; el primer intento de corrección tuvo un error de overload de C++ y no se cuenta como RED funcional. Tras la corrección del lead, build exit 0 y Processor 439/439 PASS en 23 secciones; `final-touch-build-eq-zero-green.log` / `final-touch-eq-zero-green.log` |
| Preparación Intel y sanitizer | Los shared y módulos genéricos se compilaron. Al reconstruir tests con el nuevo header de UI apareció `Rectangle<float>::toEnclosingInt()` inexistente en JUCE 8; exit 2 en ambos, comunicado a UI. Logs `final-touch-build-intel-prepare.log` / `final-touch-build-sanitize-prepare.log`; no se atribuye a un defecto DSP |
| UI final | Propietario UI corrigió la incompatibilidad y detalles visuales: 15 secciones/2366 checks PASS; `final-touch-ui-final.log`. CTest arm64 completo 5/5 PASS en 12,90 s, `final-touch-ctest-arm64.log`, sin repetir la misma suite |
| Capturas finales | 13 PNG reales copiadas y verificadas en docs/screenshots/final-touch: 1024×684, 1280×854, 1586×1058, 2560×1708 por sala, más detalle 1006×740. CRC/dimensiones/IEND PASS; `final-touch-screenshots.json` |
| Release wrappers finales | AU, VST3 y Standalone arm64 e Intel compilan exit 0; `final-touch-build-arm64-final.log` / `final-touch-build-x86_64-final.log` |
| CTest Intel final | 5/5 PASS en 23,22 s bajo Rosetta; `final-touch-ctest-x86_64.log` y XML |
| ASan/UBSan final | Debug arm64 compila exit 0, 5/5 CTest PASS en 128,64 s, sin diagnóstico sanitizer; `final-touch-build-sanitize-final.log` / `final-touch-ctest-sanitize.log` y XML |
| Fusión final | `build/universal-0.2.0`, tres formatos con dos slices y firmas ad hoc válidas; `final-touch-merge.log`. AU SHA-256 `5bf36ca80b635aea63e25f9d1e28cabe68774437231922a286ee6c16553e4541` |
| Correspondencia con QA | 7/7 archivos DSP/processor coinciden con `final-touch-build-audio-sources.json`; el candidato usa esas fuentes y la misma toolchain del render independiente. QA verificó además archive DSP y 13/13 capturas |
| Pluginval arm64 e Intel | Ambos procesos exit 0/SUCCESS sobre el VST3 universal; versión runtime 0.2.0 comprobada, strictness 5, seed 314159, seis Fs y nueve bloques. Logs `final-touch-pluginval-{arm64,x86_64}.log`; se omite del ZIP el duplicado `*-validator` |

Antes de continuar compilaciones, Build y UI cedieron la máquina a la comparación temporal de QA; QA notificó 54/54 procesos correctos y fin de la ventana. Los resultados acústicos y de CPU pertenecen al informe independiente, no se infieren de un build.

El wrapper pluginval comprueba también la versión declarada por el plugin en ejecución, además del Info.plist: un Info.plist regenerado por CMake no demuestra que un wrapper antiguo haya sido reenlazado. Por ello ningún wrapper se anuncia como candidato hasta su build Release completo.

| Artefacto final | Versión esperada | Arquitecturas / mínimos | Estado |
|---|---|---|---|
| AU `LAB-RC.component` | 0.2.0, `aufx/LbRc/Vtpl` | arm64 11.0 + x86_64 10.15 | Build/fusión/firma PASS; auval pendiente de consentimiento nuevo |
| VST3 auxiliar `LAB-RC.vst3` | 0.2.0, `com.vitaplug.labrc` | arm64 11.0 + x86_64 10.15 | Build/fusión/firma y pluginval ambas arquitecturas PASS |
| Standalone auxiliar `LAB-RC.app` | 0.2.0 | arm64 11.0 + x86_64 10.15 | Build/fusión/firma PASS; no se ha abierto |

La UI cerró su ciclo RED/GREEN y el propietario aprobó las capturas finales. Los wrappers se recompilaron desde esas fuentes congeladas. No se confunden los tiempos CTest con mediciones de CPU: la ventana de benchmark independiente de QA se realizó sin compilaciones concurrentes del equipo.

## Pendiente

El primer ZIP se entrega con auval 0.2 pendiente de autorización nueva; no se interpreta la etiqueta interna “auval” de pluginval VST3 como validación del AU. Tampoco se ejecutó el validador Steinberg externo, no configurado. Si el propietario autoriza después, el lead validará temporalmente 0.2 y restaurará 0.1, actualizando recibo y paquete sin cambiar estas fuentes. La escucha física Logic 0.2 sigue pendiente. No se declara compatibilidad Windows ni firma Developer ID/notarización. El tamaño, CRC y SHA-256 del ZIP se registran en su sidecar y recibo de entrega fuera de los documentos sellados para evitar hashes circulares.
