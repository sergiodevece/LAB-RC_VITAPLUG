# LAB-RC 0.2.0 Final Touch · informe de entrega

El propietario aprobó el carácter acústico del AU 0.1.0 tras su primera evaluación en Logic Pro y validó las tres capturas de Final Touch el 26-11-2026. El AU 0.2.0 se prepara para distribución privada y una nueva prueba física en Logic.

## Checkpoint y procedencia

La rama local `codex/lab-rc-0.1.0` permanece en `31cf43a0d4b2f04b57952639d34449209d025912`. En `checkpoints/0.1.0` se guardaron un bundle Git verificable, el AU aprobado con firma ad hoc válida y el hash del ejecutable `6532f24cd0f908c9db627a966f2d8660327dfa360338bdc6a1031d0d49430f58`. El CTest previo al cambio pasó 5/5.

Se verificaron los 32 hashes originales del kit, sus 18 WAV float32 y las nueve regresiones mono. `MatrixBank.cpp`, `MatrixBank.h` y el kit original coinciden con el checkpoint. Las tres fotografías sin batería y el PNG BOOST THIS conservan el hash de los originales del propietario. La referencia vintage autorizada coincide byte a byte con la del kit. El editor incorpora dos muestras estáticas de material provenientes de esa referencia. El manifiesto Final Touch documenta ambos recortes y los cuatro PNG.

## Confirmación de la EQ

| Banda en las tres parejas | Forma verificada | Frecuencia | Control |
|---|---|---:|---:|
| LOW | Estante bajo, S = 1 | 100 Hz | −12 a +12 dB |
| MID | Campana, Q = 0,7 | 1 kHz | −12 a +12 dB |
| HIGH | Estante alto, S = 1 | 8 kHz | −12 a +12 dB |

Cada pareja usa coeficientes compartidos entre L y R y estados independientes por canal. La respuesta medida de L/R siguió exactamente la relación de amplitud del estímulo. El error complejo máximo contra la referencia RBJ fue **1,817 × 10⁻⁷** en seis tasas. La prueba incluye los extremos, combinaciones y transición de cada banda. El DSP pasó 236/236 comprobaciones y el processor 421/421 en la primera ejecución de integración con los recursos de 0.2.0. La auditoría instrumentada no detectó llamadas de heap en los escenarios medidos. Las ganancias y EQ IN se suavizan durante 20 ms.

La revisión independiente detectó que la cuantización de JUCE dejaba `+1,788 × 10⁻⁷ dB` en el valor nominal de cero de los controles nuevos. Tras devolver las tres bandas de +12 a 0 dB, un impulso mostró un residual de pico de `3,725 × 10⁻⁹` entre EQ plana ON y OFF. Se corrigió el paso de las nueve ganancias nuevas mediante redondeo a décimas centrado en cero. Processor pasó 439/439 comprobaciones, incluidas las 18 nuevas que comprueban cero exacto al automatizar y editar las nueve ganancias.

La EQ se inserta entre la salida húmeda del fundido y el fader de cada pareja. Se conserva el enrutamiento de dry, bypass y mute/solo, junto con la latencia de 128 muestras. Los proyectos antiguos reciben las tres EQ en OFF a 0 dB. Los controles de EQ se incluyen en automatización, historial y RESET. [Informe DSP](reports/final-touch-eq.md)

## Resultados entre versiones y validación de AU

El null test de los builds Release arm64 produjo el mismo hash de audio con 0.1.0, las tres EQ OFF y las tres EQ planas ON. En cada comparación hubo **0 diferencias de 213.520.746 muestras float32**, con pico y RMS residuales de cero. Las 128 muestras de latencia se conservaron en los 234 segmentos a las seis tasas. Se comprobaron las nueve parejas, impulsos L/R, mono, estéreo determinista, mezclas y controles, el estado legado real y cambios de sala. Los tramos de cambio de sala dejan lista cada sala en la misma frontera de muestras para que el resultado no dependa de la planificación del hilo de carga. [Protocolo y evidencia de regresión](reports/final-touch-regression.md)

En la medición intercalada sin compilación, el p99 mediano a 96 kHz con bloques de 64 muestras fue 55,0 µs en 0.1.0, 53,6 µs con EQ OFF y 54,8 µs con EQ plana ON. A 192 kHz y 64 muestras fue 86,2 / 93,7 / 97,6 µs, con 2 / 6 / 4 excesos de plazo sobre 54.000 callbacks por modo. La prueba instrumentada del DSP también cubrió EQ activa con ganancias no planas y automatización. La carga de sesiones con varias instancias y audio musical corresponde al smoke de Logic. [Tabla de CPU y memoria](reports/final-touch-regression.md)

| Verificación del candidato 0.2.0 | Resultado |
|---|---|
| CTest Release | 5/5 en arm64 y 5/5 en x86_64 bajo Rosetta |
| CTest con ASan y UBSan | 5/5 en arm64 |
| `pluginval` estricto del VST3 auxiliar | PASS en arm64 y x86_64 bajo Rosetta |
| AU Release universal | arm64 y x86_64 verificados con `lipo`. Firma ad hoc verificada con `codesign` y simulación de instalación superada |
| `auval` estricto del AU de 0.2.0 | PASS en arm64 y x86_64 bajo Rosetta. Ambas pruebas identificaron la versión 0.2.0 |

El AU 0.2.0 quedó instalado para el smoke de Logic con autorización del propietario. El ejecutable instalado coincide con el del ZIP. Se conserva el checkpoint 0.1.0 para restauración. [Informe de compilación](reports/final-touch-build.md) · [Recibo de validación AU](reports/final-touch-au.json)

La guía de instalación incluye respaldo y rollback. La escucha con EQ activa y la validación de un proyecto 0.2.0 en Logic quedan para revisión física. [Instalación y smoke de 0.2.0](FINAL_TOUCH_INSTALL.md)
