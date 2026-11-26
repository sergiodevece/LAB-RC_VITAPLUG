# LAB-RC 0.2.0 — instalación privada

Descomprime el ZIP en una carpeta local y cierra Logic Pro antes de instalar. El AU mantiene el identificador `com.vitaplug.labrc` y los códigos `aufx/LbRc/Vtpl`. Instalarlo sustituye la versión previa en tu carpeta personal de Audio Units.

Antes de sustituir un AU, conserva su `.component` completo en una carpeta de respaldo. En el proyecto ya existe una copia verificada del AU que superó el smoke de 0.1.0 en `checkpoints/0.1.0/LAB-RC.component`.

Desde la carpeta del paquete, comprueba primero el destino y después instala:

```sh
bash scripts/install_macos.sh AU/LAB-RC.component --dry-run
bash scripts/install_macos.sh AU/LAB-RC.component
auval -strict -v aufx LbRc Vtpl
```

En Logic, verifica que el gestor de módulos muestra 0.2.0. Abre una copia del proyecto que usaste con 0.1.0 y comprueba primero el recall y el sonido con EQ IN apagado en las tres parejas. Activa luego cada pareja por separado y comprueba los tres controles, el medidor de pareja, la mezcla dry/wet y bypass. Revisa el cambio de sala con audio activo y guarda/reabre el proyecto para probar automatización, undo/redo y persistencia.

Para volver a 0.1.0, cierra Logic y ejecuta `bash scripts/install_macos.sh '/ruta/al/respaldo/LAB-RC.component'`. El respaldo debe conservar el nombre `LAB-RC.component`. Comprueba la versión en el gestor de módulos cuando vuelvas a abrir Logic.

El AU usa firma local ad hoc. El propietario validó las tres capturas. La ejecución en otro equipo y la escucha de 0.2.0 se comprueban durante el smoke.
