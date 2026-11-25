# LAB-RC 0.1.0 — informe final del candidato AU

25-11-2026 · Europe/Madrid. Candidato local para smoke físico en Logic Pro. La distribución funcional de la interfaz está validada por el propietario; **el acabado gráfico no está aprobado y queda congelado**. No se ha añadido EQ ni cambiado las IR o el modelo sonoro.

## Resultado de software y artefacto

| Comprobación ejecutada | Resultado | Evidencia |
|---|---|---|
| Integridad original y copia | PASS: 32 hashes, 4 PNG, 18 WAV float32/96k, 9 regresiones mono | `reports/gate0-original-final.log`, `gate0-workspace-final.log` |
| Matrices v06 → referencia v05 | Error máximo de assets 3,72529029846e-9 | Manifiesto y Gate0 independiente |
| Motor DSP | 209 checks PASS; error máximo de impulso a96k 2,98e-8 | `reports/dsp.md`, `recall-i9-ctest.log` |
| Processor/estado/undo | 206 checks PASS | `reports/recall-fix.md`, `recall-i9-ctest.log` |
| UI funcional | 2231 checks PASS; 12 capturas reales comprobadas | `reports/ui.md`, `screenshots/*.png` |
| CTest Release arm64 | 5/5 PASS, 29,76s | `reports/ctest-arm64-final.log` |
| CTest Release x86_64 bajo Rosetta | 5/5 PASS, 27,96s | `reports/ctest-x86_64-final.log` |
| ASan+UBSan Debug arm64 | 5/5 PASS, 116,69s; sin diagnóstico en la ejecución | `reports/ctest-sanitize-final.log` |
| AU universal | arm64 mínimo11.0 y x86_64 mínimo10.15; identidad/versión/códigos y firma ad hoc verificados | `reports/universal-merge-final.log` |
| pluginval1.0.4 / VST3 universal macOS | SUCCESS en arm64 y x86_64, strictness5, seed314159 | `reports/pluginval-arm64.log`, `pluginval-x86_64.log` |
| Instalación AU consentida | PASS: solo `~/Library/Audio/Plug-Ins/Components/LAB-RC.component` | `reports/au-install.log`, `installed-au.json` |
| auval estricto arm64 | AU VALIDATION SUCCEEDED, exit0 | `reports/auval-arm64.log` |
| auval estricto x86_64 bajo Rosetta | AU VALIDATION SUCCEEDED, exit0 | `reports/auval-x86_64.log` |
| QA independiente | I1–I9 corregidos y re-revisados; apto para smoke en el alcance local registrado | `reports/qa.md` |

Los seis Fs de pluginval fueron 44100/48000/88200/96000/176400/192000; los nueve bloques1/32/64/127/128/256/512/1024/2048. El AU también pasó las pruebas de render que ejecutó auval, incluidas otras tasas/bloques elegidos por esa herramienta. El validador Steinberg externo no estaba configurado y fue omitido: no se anuncia como ejecutado.

El AU instalado tiene bundleID `com.vitaplug.labrc`, versión0.1.0 y códigos `aufx/LbRc/Vtpl`. Su ejecutable coincide con el candidato universal: SHA256 `6532f24cd0f908c9db627a966f2d8660327dfa360338bdc6a1031d0d49430f58`. La firma es local ad hoc; no se ha realizado firma DeveloperID ni notarización. No se abrió Logic ni se modificó Gatekeeper.

## Alcance matemático y de tiempo real

Las pruebas recorren las nueve matrices a96k con impulsos L-only, R-only y mono, y toda la cola. Resampling se contrasta a los seis Fs con convolución directa de FIR analíticas y DFT independiente de los WAV en cinco frecuencias de control. Los límites numéricos, espectrales y las mediciones por Fs están en `reports/dsp.md`; cinco frecuencias no equivalen a una exploración espectral continua.

Se conserva una latencia de cálculo de128 muestras, dry/bypass alineados y cola conservadora4,5s+128/Fs. La restauración secuencial en frío antes o después de prepare produjo el mismo primer impulso de cada una de las nueve parejas a192k/64. Durante audio activo, cambiar de sala espera la preparación asíncrona y funde50ms; no ofrece spillover íntegro ni automatización de sala sampleaccurate independiente del tiempo de preparación.

La auditoría con control positivo pasó29/29 y observó **cero operaciones de heap interpuestas en el callback del mezclador**. No establece ausencia de locks en todo el pipeline: JUCE/APVTS y los wrappers tienen sincronización propia. El render de aplicación no toma mutex ni publica notificaciones GUI. El host debe ordenar el recall frío antes del primer audio; si lo hace concurrentemente, el wrapper puede esperar la preparación bajo su callback lock.

**El presupuesto de CPU no queda certificado.** El último benchmark de tiempo de pared, con compilaciones de los agentes pausadas, registró0/18000 excesos en régimen estable192k/64, pero5/600 durante fundidos (máximo839µs frente a333,33µs de plazo),2/4500 a192k/256 y5/9000 a96k/64. Los bloques de una muestra también excedieron su plazo estricto. Se conservan los resultados anteriores y la variabilidad en `reports/recall-i9-heap-benchmark.log` y los informes DSP. Son mediciones offline de una instancia, sin hilo de audio de Logic ni estrés musical de múltiples instancias.

## Pendientes reales

- Escucha/A-B nivelado de las nueve IR y smoke físico en Logic Pro, incluido recall, automatización, teclas, bypass, colas, mono y varias instancias. No se simula aprobación auditiva.
- Aprobación del acabado gráfico. El propietario ha ordenado posponer el diseño industrial definitivo; la UI actual solo sirve a esta fase funcional.
- Build y host Windows x64 reales. Los binarios VST3 de esta sesión son macOS auxiliares de validación.
- Ejecución en macOS11/10.15 físicos y en hardware Intel; los mínimos se verificaron en Mach-O, los tests Intel se ejecutaron bajo Rosetta en macOS26.5.
- Definir derechos/licencia de distribución pública. No publicación/push/GitHub.

El verificador NumPy/SciPy original se conserva intacto y no se ejecutó con éxito por falta de NumPy. El Gate0 independiente stdlib comprueba hashes, dimensiones PNG, encabezados/muestras WAV y regresión de matriz sin descargar dependencias.

## Entrega local y siguientes pasos

El AU instalable prioritario es `dist/LAB-RC-0.1.0-AU-universal.zip`; sus hashes y el de las fuentes se entregan en sidecars `.zip.sha256`, sin incrustar un hash circular en este informe. Los paquetes se inspeccionan después de generarlos. La guía es `SMOKE_LOGIC.md`.

Tras aprobar el comportamiento sonoro y funcional se abordará una segunda iteración de diseño industrial vintage definitivo y EQ analógica de tres bandas por pareja. Esa ampliación no está implementada en el candidato actual.
