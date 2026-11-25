# Registro LAB-RC — plan: docs/superpowers/plans/2026-11-25-lab-rc.md

2026-11-25, Europe/Madrid. Petición explícita de implementar con Superpowers/subagentes y gates; sin publicación.

- Gate0 ejecutado ANTES del desarrollo: verificador original falla por ModuleNotFoundError numpy. Oracle stdlib independiente: 32 hashes OK, 4 PNG OK, 18 WAV IEEE float32/96k/frames/duración/finite OK; 9 regresiones mono OK, max 3.725290298461914e-09. Prompts exterior/interior idénticos.
- Auditorías especializadas UI y compilación terminadas solo lectura. UI detecta JPEG recodificados en HTML: usar PNG originales. JUCE local limpio 8.0.17 disponible; licencia comercial no acreditada. Windows no disponible. Logic13 instalado, no12.2.
- Ruling: ejecutar el plan sin otra ronda de aprobación de diseño — el usuario pide expresamente seguir la especificación y «Adelante», el propio prompt pide comenzar fases locales sin aprobaciones repetitivas; no cambiar estética/sonido.
- Ruling: workspace local designado por usuario está vacío, sin commits ni otros archivos. Trabajar aquí en rama codex/lab-rc; un segundo worktree no protege contenido existente y crearía otra ubicación innecesaria. Ninguna escritura en proyectos ajenos.
- Ruling: usar stdlib para Gate0 mientras no hay NumPy/SciPy locales, sin descargarlos; conservar el verificador original y registrar la diferencia.
- Ruling: RESET afecta parámetros DSP en una transacción; conserva preferencias visuales y tamaño por instancia.
- Ruling: agentes DSP/UI/Build tienen propiedad disjunta; QA posterior es independiente. Lead resuelve contratos. No commits simultáneos de agentes.

## Preflight del plan
| Tareas | Interfaz / alcance | Resultado y decisión |
|---|---|---|
| DSP/Processor | RoomMixer, Parameters, MeterValues | API fijada en interfaces.md; propiedad separada |
| Processor/UI | APVTS, gestures, preferencias, reset | APVTS sin UndoManager; acciones explícitas; reset también MICS=true, tamaño preservado |
| Build/DSP | labrc_dsp, Tests/DSPTests.cpp | Build integra sin editar DSP; runner DSP aislado |
| Build/Processor/UI | LabRC, BinaryData, runner | HEADER_NAME LabRCAssets.h; namespace LabRCAssets; UI constructor LabRCEditor |
| QA/todas | paquetes y reportes | QA independiente cuando archivos estén completos |
| Task1 | tests FIR frente a implementación FFT | coherente; medir RT y resampling, no alterar IR |
| Task2 | estado antiguo/inválido, undo usuario | tests escritos antes de funcionalidad, skeleton no-op para RED |
| Task3 | UI/contain/anclas | testfirst geometría; ninguna imagen nueva |
| Task4 | herramientas locales/Windows | sin descarga; Windows PENDIENTE |
| Task5 | integridad/entrega/gates humanos | no cerrar revisión humana con software |

- Rama local creada: `codex/lab-rc-0.1.0`; commit base `489e279` con especificación/plan/recursos. No remoto ni publicación.
- Verificador stdlib e informes de original/copia guardados; ambos exit0.
- Usuario actualiza el plan y pide continuar; plan releído, preservada ejecución por especialidades y gates.
- Processor test-first: Tests/ProcessorTests.cpp y skeleton sin funcionalidad de estado/undo listos para RED real. Compilación inicial pendiente de runner/editor.

- Processor RED registrado: 47 fallos reales en estado/undo/recursos; RED audio adicional antes de implementación. Primera GREEN: 13 secciones,139 checks,0 fallos. No se cierra gate: revisión independiente encuentra I1–I8.
- QA primera revisión: `docs/reports/qa.md`, dos veredictos requieren correcciones. I1 sala al recall inicial, I2 historia undo anterior, I3 envelope inválido, I4 tamaño abierto, I5 posts GUI desde automation, I6 foco wrapper, I7 etiquetas L/R truncadas, I8 tar Windows.
- Ruling plan-mandated I5: el contrato sugería ParameterAttachment, pero la restricción RT obliga a polling GUI30Hz de atomics sin suscripción GUI desde hilo audio; conserva interacción/sonido/estética.
- Ruling I1: reparar orden de preparación/restore, no cambiar IR ni mix/crossfade. Operaciones iniciales de control fuera de process; documentar locks inherentes JUCE y room automation async.
- Ruling propiedad: delegar fixes I1/I2/I3 al agente DSP con Processor/DSP/Tests correspondientes temporalmente; lead congela esos archivos. UI atiende I4/I5/I7 y usa nuevas APIs undo/redo seguras. Build atiende I6/I8. Re-review tras fix, no paquete final previo.

- Fix round1/5 en curso: UI RED de polling/edición (2197PASS/9FAIL) reproducido antes de fix; DSP escribe RED recall I1/I2/I3; Build confirma foco y repara bootstrap portable/overwrite capturas.
- Limitación de arquitectura registrada: room automation espera preparaciónasync (instante no sampleaccurate); recall inicial es otro contrato y se corrige para primerimpulsoidéntico. Locks JUCE/host no se presentan como zerolocks del pipeline.

- Usuario: «Lo revisaré después» a gate visual; posteriormente valida distribuciónfuncional, rechaza aprobaracabado porrasgosdelprototipodescartado y ordena congelarestética. Prioridad AUinstalable/smokeLogic. Segunda iteración futura diseñoindustrialdefinitivo+EQ3bandas/pareja, SINimplementarlaahora. DSP/IR/arquitecturaseconservan.
- Fixround2 I9: modo externoautoridadúnicaparaseleccióndesalaenProcessor. RED2fallos; DSP209PASS, arm645/5 ymacdsp3/3; heapbenchmark29/29con0heap. Re-reviewybuildfinalporplataformaenprogreso.

- Verificación final: Releasearm64/x86_64 5/5 cadauno, ASan+UBSan5/5, pluginvaluniversalVST3strictness5dobleSUCCESS. Auvalrealarm64/x86_64estrictoSUCCESStrasautorizaciónexplícita einstalaciónúnicoAU. Logicnoabierto. Candidatouniversalinstalado6532f24cd0f908c9db627a966f2d8660327dfa360338bdc6a1031d0d49430f58.
- QA I1–I9cerradosenalcancesoftware. VisualNOaprobadoycongelado; humo/escuchaLogicypresupuestorealtimeconmaterialmusicalpendientes. SinEQ, descargas, GitHubnipush. Documentosfinalespreparadosparaempaquetadoinmediato; confirmarCRC/hash/sidecarsdespuéscreación.
