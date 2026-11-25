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
