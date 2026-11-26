# LAB-RC 0.1.0 — prueba física en Logic Pro

La prioridad es probar el AU local antes de continuar diseño gráfico o añadir EQ. La distribución funcional fue validada por el propietario; el acabado gráfico **no está aprobado** y permanece congelado. No hay EQ en este candidato.

## Instalación del candidato

El AU prioritario se entrega en `dist/LAB-RC-0.1.0-AU-universal.zip`. La arquitectura y firma de los binarios se documentan en los informes de build. Firma local ad hoc, sin afirmar firma Developer ID ni notarización. No hace falta desactivar Gatekeeper globalmente.

El paquete contiene `LAB-RC.component` y scripts de instalación/desinstalación. El instalador verifica identidad y firma y actúa exclusivamente sobre `LAB-RC.component` en `~/Library/Audio/Plug-Ins/Components`. No usa sudo. Usar primero `--dry-run` para comprobar las rutas. Las llamadas concretas desde el repositorio son:

```sh
bash scripts/install_macos.sh "/ruta/descomprimida/LAB-RC.component" --dry-run
bash scripts/install_macos.sh "/ruta/descomprimida/LAB-RC.component"
auval -strict -v aufx LbRc Vtpl
```

La instalación real y auval solo se consideran ejecutados si aparecen sus logs. El build y la creación del ZIP no instalan automáticamente.

## Sesión de prueba

1. Crear un proyecto de prueba en Logic. Registrar versión exacta de Logic/macOS, arquitectura, Fs y buffer. La máquina inspeccionada tiene Logic13; el brief original menciona12.2.
2. Instanciar LAB-RC en una pista estéreo, en mono→estéreo si Logic ofrece ese layout y en un aux estéreo. No se anuncia mono→mono. Comprobar L/R por separado, suma mono y varias instancias.
3. Empezar a 48 o96k y buffer256. Envío dry separado, LAB-RC a100% wet, trims0 y ganancias constantes. Aislar EARS/FRONT/FLOOR con SOLO. Comparar las nueve respuestas con Space Designer sin normalización, trim de impulso, predelay ni cambio de duración. Igualar nivel para comparar. Usar caja, bombo, toms y platos propios.
4. Verificar MIX0% dry y100% wet, trims, mute prioritario, varios solos, fader−60dB aún audible frente a mute. Comprobar que clip indica saturación de nivel sin limitador oculto.
5. Con cola activa, probar bypass del plugin y del host, mute/solo, faders y cambios rápidos de sala. El cambio de sala usa preparación asíncrona y fundido50ms: la cola anterior se atenúa, sin spillover íntegro. Conservar faders al cambiar sala.
6. Automatizar parámetros a sus límites. Probar undo/redo local, reset deshacible, cierre/reapertura, cambio de tamaño, MICS, guardar/reabrir proyecto, reiniciar Logic y duplicar instancia. El primer audio de una reapertura debe usar la sala guardada.
7. Probar reproducción/parada, cambios de Fs/buffer y carga sostenida con varias instancias. Después contrastar192k/64 con especial atención al margen de CPU; los benchmarks de tiempo de pared registraron plazos incumplidos y no sustituyen la prueba del hilo de audio real.

## Registro

Anotar fecha, versiones, Fs/buffer, instancias, cada paso PASS/FAIL y observaciones audibles. Adjuntar el proyecto de prueba si se desea, sin sobrescribir sesiones de producción. Esta sesión de desarrollo no afirma haber escuchado ni aprobado el sonido del plugin.

Tras la aprobación sonora/funcional se abrirá una segunda iteración para el diseño industrial vintage definitivo y la EQ de tres bandas por pareja. No forman parte de este candidato congelado.
