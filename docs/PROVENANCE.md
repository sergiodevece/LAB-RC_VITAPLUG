# Procedencia y recursos inmutables

Origen suministrado por el propietario: `/Users/serx/Downloads/LAB-RC_Implementation_Kit/` y `/Users/serx/Downloads/LAB-RC_MASTER_PROMPT.md`. Ambos MASTER_PROMPT son idénticos, SHA-256 `d226d3067a34ee7e9561ddadccd277e4867564df16eb1d052833f8bfae35ff56`.

`Assets/ImplementationKit/` es copia íntegra local. Ningún recurso de origen se edita. El manifiesto conserva tamaño, SHA-256, formato, dimensiones, frames, duración y orden declarado. Los informes `gate0-original.log` y `gate0-workspace.log` verifican ambos árboles independientemente.

| Recursos | Uso |
|---|---|
| `assets/ir/v06/*_wet_matrix.wav` | Nueve matrices wet 2×2, 96 kHz/float32, canales LL/LR/RL/RR; únicos kernels de audio |
| `assets/ir/v05/*_wet.wav` | Nueve referencias mono→estéreo de regresión; no se suman como otra capa |
| `assets/rooms/*.png` | Tres fotografías aprobadas con batería BOOST THIS, íntegramente embebidas y mostradas con contain |
| `assets/design/approved_vintage.png` | Solo referencia del diseño industrial; no fondo completo de producto |
| `assets/design/prototype_v01.html` | Referencia de interacción y posiciones gráficas, no estilo ni fuente de fotografías |
| `research/*` | Antecedentes del solver/FDN offline y métricas históricas; no código generador runtime |

Se examinaron los cuatro PNG visualmente, los 18 WAV numéricamente y todos los documentos/scripts del kit. Los JPEG base64 del HTML están recodificados: se excluyen de la implementación. Los estados históricos de research que indicaban ausencia de escucha no sustituyen la aprobación posterior v0.5 recogida en MASTER_PROMPT.

La geometría es provisional y sintética. Las IR v0.6 derivan algebraicamente de v0.5 y una componente lateral heurística. No son capturas reales ni una validación física de las salas. El plugin reproduce los archivos; no ejecuta el solver ni el FDN ni ofrece parámetros que alteren sus modelos.

Dependencia local: archivo JUCE 8.0.17-security de `COMPRESOR EXPERIMENTAL/build/dependencies/`, SHA-256 `d37bab9f2330c50f42ba979575f6b55d5d3b23fa94221c2e57cac3abb69ac6d9`. Ese proyecto se consulta en lectura y no se modifica. Códigos propuestos tras revisar productos locales: fabricante `Vtpl`, plugin `LbRc`, bundle `com.vitaplug.labrc`. No hay publicación ni registro mundial de exclusividad de esos códigos.

Estado legal: véase `LICENSE.md`; revisión pública de derechos PENDIENTE. Las comprobaciones realizadas no inventan una licencia ni sustituyen decisión del titular.
