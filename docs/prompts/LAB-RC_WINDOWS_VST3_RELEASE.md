# LAB-RC — Windows VST3 Release

**Producto:** Manza’s Rooms & Chambers — A Tribute to Steve Albini  
**Fabricante:** VITALAB / VitaPlug  
**Versión:** Mantener la última versión funcional del AU.

## 1. Objetivo

Generar una compilación VST3 nativa para Windows x86_64 a partir del código fuente actual de LAB-RC, conservando íntegramente el comportamiento sonoro, las funcionalidades y el diseño gráfico de la versión AU que está funcionando satisfactoriamente en Logic Pro.

Utiliza Superpowers y subagentes especializados en compilación multiplataforma, DSP, compatibilidad y QA, si están disponibles.

## 2. Protección de la versión actual

- Crear un checkpoint de seguridad antes de intervenir.
- No modificar ni sobrescribir el AU instalado.
- Mantener los identificadores de parámetros y producto.
- Conservar la compatibilidad con los estados y presets existentes.
- Verificar la integridad SHA-256 de las respuestas impulsionales.

## 3. DSP — Paridad absoluta

Conservar sin modificaciones funcionales:

- Las tres salas: Kentucky, Center Field y Studio B Live Room.
- Las matrices de convolución true stereo.
- Las parejas microfónicas EARS, FRONT y FLOOR.
- Las posiciones y características acústicas originales.
- Las ganancias, faders, mute y solo.
- La compensación de latencia.
- Las transiciones entre salas sin clicks.
- Los controles INPUT, OUTPUT, MIX y BYPASS.

Mantener las tres secciones independientes de ecualización analógica:

- LOW: low shelf a 100 Hz.
- MID: campana a 1 kHz, Q = 0,7.
- HIGH: high shelf a 8 kHz.
- Rango de ±12 dB por banda.
- Procesamiento estéreo enlazado por pareja.
- EQ IN independiente.

No modificar las IR, introducir normalizaciones adicionales ni alterar el gain staging.

## 4. Interfaz gráfica

Mantener exactamente el diseño industrial vintage actual:

- Chasis verde VITALAB envejecido.
- Tornillería gastada.
- Potenciómetros realistas.
- Faders inspirados en las consolas británicas de los años setenta.
- Las tres fotografías aprobadas.
- La batería BOOST THIS definitiva.
- Las nueve perillas de ecualización.
- Medidores estéreo funcionales.
- Redimensionamiento proporcional.
- Compatibilidad HiDPI y escalado de pantalla de Windows.

No regenerar los recursos gráficos ni sustituirlos por controles genéricos de JUCE.

## 5. Compilación Windows

- Formato: VST3.
- Arquitectura: Windows x86_64.
- Sistemas objetivo: Windows 10 y Windows 11 de 64 bits.
- Configuración: Release.
- Utilizar C++ / JUCE / CMake.
- Compilar mediante MSVC en un entorno Windows nativo.
- Incorporar todos los recursos gráficos y acústicos necesarios.
- Evitar dependencias externas innecesarias.
- Garantizar que el plugin funciona sin conexión a Internet.

Si el entorno actual macOS no permite realizar una compilación Windows nativa, preparar y ejecutar un workflow de GitHub Actions con runner Windows, previa autorización para utilizar el repositorio, o utilizar otro entorno Windows disponible.

No entregar un VST3 compilado para macOS como si fuera compatible con Windows.

## 6. Validación automatizada

Ejecutar las siguientes comprobaciones:

- Compilación Release satisfactoria.
- Validación mediante pluginval y VST3 Validator, cuando estén disponibles.
- Verificación de configuraciones de canales compatibles con el AU.
- Integridad de las IR.
- Regresión sonora respecto a la implementación AU.
- Respuesta en frecuencia de las nueve bandas de EQ.
- Null test con las EQ desactivadas.
- Automatización de todos los parámetros.
- Cambios de sala durante la reproducción.
- Ausencia de clicks y discontinuidades no deseadas.
- Undo, redo y reset.
- Persistencia de parámetros y tamaño de ventana.
- Apertura y cierre repetidos.
- Varias instancias simultáneas.
- Estabilidad con diferentes frecuencias de muestreo y tamaños de buffer.
- Ausencia de NaN, infinitos y errores de memoria.
- Consumo de CPU y memoria.

Distinguir claramente las pruebas ejecutadas de las pendientes de smoke físico.

## 7. Entregables

Preparar un paquete:

`LAB-RC-Windows-x64-VST3.zip`

Debe contener:

- El bundle completo `LAB-RC.vst3`.
- Script de instalación PowerShell o instalador Windows.
- Instrucciones de instalación y desinstalación.
- README.
- CHANGELOG.
- Informe de compilación y validación.
- Hash SHA-256 del paquete.

Directorio estándar de instalación:

`C:\Program Files\Common Files\VST3\`

Entregar una ruta o enlace de descarga real y verificable del ZIP.

## 8. Restricciones finales

No añadir funcionalidades nuevas.

No modificar el AU.

No modificar el carácter sonoro.

No rediseñar la interfaz.

No publicar una release pública sin autorización expresa.

El objetivo es conseguir una versión Windows funcionalmente equivalente al AU actual, preparada para nuestro smoke físico.

Adelante con LAB-RC Windows VST3 Release.
