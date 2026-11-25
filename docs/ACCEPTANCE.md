# Gates de aceptación LAB-RC 0.1.0

Registro de resultados reales. PENDIENTE no equivale a PASS. Los logs conservan comandos y fallos; el informe final se actualiza tras la verificación.

| Gate | Estado actual | Evidencia / condición |
|---|---|---|
| 0: integridad | PASS mediante oracle stdlib | 32 SHA-256, 4 PNG, 18 WAV IEEE float32/96k y 9 regresiones mono; max 3.725290298461914e-09 |
| Verificador original NumPy/SciPy | NO EJECUTADO CON ÉXITO | Falta NumPy; archivo original intacto, sin dependencias descargadas |
| 1: DSP offline | PASS, regresión de software | 209 checks tras I9; ver informes DSP/recall y logs finales de build |
| 1: presupuesto de tiempo real | LIMITACIONES MEDIDAS | Excesos variables: última medición fundido192k/64 5/600; 192k/256 2/4500. Ver TEST_REPORT e históricos; no promesa universal de CPU |
| Processor/estado/automatización | PASS de software y QA | 206 checks; recall inicial idéntico antes/después de prepare; I1–I3 e I9 corregidos y re-revisados |
| 2: UI/software y capturas | PASS de software | 2231 checks, 12 PNG reales sin concatenación; sin listeners GUI desde audio |
| 2: distribución funcional | APROBADA POR EL PROPIETARIO | Instrucción explícita posterior a las capturas |
| 2: acabado gráfico | NO APROBADO; CONGELADO | El propietario identifica rasgos del prototipo descartado. No más cambios estéticos en esta fase |
| 3: AU universal y VST3 macOS | PASS local | arm64+x86_64, lipo/minos/codesign ad hoc, pluginval doble SUCCESS |
| 3: auval/instalación local | PASS, autorizados por el propietario | AU instalado; auval estricto SUCCESS en arm64 y x86_64 |
| 3: VST3 Windows x64 | PENDIENTE | No hay entorno Windows; no CI remoto autorizado |
| 3: escucha/smoke Logic | PENDIENTE | Usuario en Logic 12.2; instalado aquí 13.0. Matriz de abajo |
| 4: documentación/paquetes | ENTREGA LOCAL | Informes sellados, AU y fuentes en dist; comprobar sidecars .zip.sha256 y manifiestos internos |
| Distribución pública | NO AUTORIZADA | Sin push/GitHub. Derechos/licencias públicos pendientes |

Primera revisión independiente: `docs/reports/qa.md`, ocho hallazgos importantes corregidos y re-revisados. I9, introducido durante el fix de recall, obtuvo RED antes de corregirse y está corregido y re-revisado. Los resultados finales de arquitectura, firma, pluginval, sanitizers y auval están registrados en `TEST_REPORT.md`.

La prioridad indicada por el propietario es entregar un **AU instalable para el smoke físico en Logic Pro**. La segunda iteración de diseño industrial vintage definitivo y EQ analógica de tres bandas por pareja queda expresamente diferida hasta aprobar el comportamiento sonoro/funcional. No se implementa EQ en 0.1.0 ni se modifican IR, procesamiento sonoro o arquitectura funcional para esa fase futura.

## Smoke humano obligatorio

Usar audio propio de caja, bombo, toms y platos. Comparar a nivel igualado las nueve IR contra Space Designer, sin normalización, trim, reescalado temporal o predelay adicional. Primero wet aislado; luego mezcla. Revisar mono→estéreo, estéreo, aux y suma mono. Escuchar cambios rápidos de sala, mute/solo, faders, trims, MIX extremos y bypass con cola activa.

En sesión de prueba revisar automatización a límites, undo/redo local, reset deshacible, tamaño min/default/max, cierre/reapertura, guardado/reinicio del host, duplicación de instancia, reproducción/parada, cambio de Fs/buffer y carga sostenida con múltiples instancias. Registrar Fs, buffer, instancias, CPU y observaciones auditivas. La prueba no se marca superada sin ejecución humana.
