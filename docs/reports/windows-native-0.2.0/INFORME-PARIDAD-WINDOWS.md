# LAB-RC 0.2.0 · Paridad Windows x64 y AU

Comparación del procesador compartido compilado en Release con MSVC nativo en Windows frente a la referencia arm64 de la versión AU 0.2.0, con las tres EQ apagadas.

- 234 segmentos y 213.520.746 muestras float32 a seis frecuencias de muestreo.
- Pico residual de 3.57627869e-07 FS y RMS de 4.80121404e-09 FS.
- Límite de pico de 2e-05 FS. Resultado **PASS**.
- 158886358 muestras con diferencias de bits.
- Se comparó con la misma alineación y latencia de 128 muestras.
- CTest PASS. pluginval PASS (nivel 5). Steinberg VST3 Validator PASS (47 pruebas).
- SHA-256 del DLL compilado: a4a116e794d5b30393b5a8a9fc4deb48309daf975400ec67cbb8b5f5b88eaedc.
- SHA-256 del ZIP original generado en el runner: 8a295bb8e1e179d06ba7d19124855f9293bb86ae1132e4a424f5c2e1aa27c6af.

La prueba en el DAW de Windows sigue pendiente.

[Ejecución privada de compilación](https://github.com/sergiodevece/LAB-RC_VITAPLUG/actions/runs/45823374424)
