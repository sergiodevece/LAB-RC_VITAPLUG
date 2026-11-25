# Contrato LAB-RC 0.1.0

Namespace común `labrc`. Includes JUCE por módulos explícitos (juce_audio_processors, juce_audio_formats, juce_dsp, juce_gui_extra); no JuceHeader generado.

## DSP

`Source/DSP/RoomMixer.h` define:

```cpp
struct Parameters {
    int room = 0;
    float inputDb=0, outputDb=0, mixPercent=100;
    std::array<float,3> pairDb{{-6,-3,-9}};
    std::array<bool,3> mute{{false,false,false}}, solo{{false,false,false}};
    bool bypass=false;
};
struct MeterValues {
    std::array<float,2> input{}, output{};
    std::array<std::array<float,2>,3> pairs{};
};
using IRProvider = std::function<juce::AudioBuffer<float>(int room, int pair)>;
// Provider retorna cuatro canales [LL LR RL RR] a 96000 Hz; puede lanzar error fuera de RT.
class RoomMixer {
public:
    RoomMixer(); ~RoomMixer();
    void prepare(double sampleRate, int maxBlockSize, int initialRoom, IRProvider);
    void release(); // host audio stopped only; nunca dentro callback
    void process(float* const* channels, int numInputChannels, int numSamples,
                 const Parameters&, bool hostBypassed=false) noexcept;
    int latencySamples() const noexcept;
    double tailSeconds() const noexcept;
    MeterValues meterSnapshot() const noexcept;
};
```

El buffer siempre tiene al menos dos canales, aunque entrada sea mono. No double. Parámetros leídos una vez por bloque; DSP sanea valores defensivamente. prepare puede bloquear/asignar; process jamás. Los detalles y clases auxiliares pertenecen al agente DSP. Si el contrato necesita variar, avisar al lead antes de cambiar.

## Processor / editor

`Source/PluginProcessor.h` define clase global `LabRCProcessor : public juce::AudioProcessor`:

- Miembros públicos `juce::AudioProcessorValueTreeState apvts; juce::UndoManager undoManager;` (orden construcción correcto: undoManager antes de apvts).
- `labrc::MeterValues meterSnapshot() const noexcept;`
- `bool micsVisible() const noexcept; void setMicsVisible(bool);`
- `void setEditorSize(int width,int height); juce::Point<int> editorSize() const;`
- `void resetParameters(); void beginUserGesture(const juce::String& name);`
- APVTS construido con nullptr UndoManager; historial público separado, sin undo por automation.
- `void beginParameterGesture(const juce::String& id,const juce::String& name={}); void setParameterFromUser(const juce::String& id,float actualValue); void endParameterGesture(const juce::String& id);`
- ParameterAttachment solo escucha; controles llaman las tres funciones anteriores. Cambios de usuario coalescen por ID y transacción, begin/end host.
- setMicsVisible es usuario deshacible; restore escribe preferencias sin acción. Reset parámetros DSP y MICS=true en una transacción; conserva tamaño.
- Estado XML envelope LABRC_STATE version=1 con hijo PARAMETERS; aceptar PARAMETERS antiguo; parcial usa defaults y clamp; blob corrupto conserva estado previo.

IDs exactos: `room`, `inputDb`, `outputDb`, `mixPercent`, `earsDb`, `frontDb`, `floorDb`, `earsMute`, `frontMute`, `floorMute`, `earsSolo`, `frontSolo`, `floorSolo`, `bypass`. versionHint=1. Labels producto inglés, docs español.

`Source/Resources.h` funciones namespace labrc: `juce::AudioBuffer<float> loadMatrixIR(int room,int pair); juce::Image roomImage(int room);`. Recursos en `LabRCAssets` BinaryData (namespace explícito). Ningún UI usa imágenes reference ni HTML en runtime.

Editor global `LabRCEditor : public juce::AudioProcessorEditor`, constructor `explicit LabRCEditor(LabRCProcessor&)`. Puede exponer `setRoomForScreenshot` solo si es cambio de parámetro normal; evitar API de producción exclusiva de test. Capturas mediante instanciar processor/editor y parámetros estándar en ejecutable de pruebas.

## Build/tests

Librería compartida estática target `LabRC` generado por juce_add_plugin; core target `labrc_dsp` sin UI. Tests target `labrc_tests` agrupa Tests/TestMain.cpp, DSPTests.cpp, ProcessorTests.cpp, UITests.cpp; JUCE UnitTest con categorías `DSP`, `Processor`, `UI`. Console runner soporta categorías/benchmark/screenshot si implementado y documentado por Build. CTest debe usar procesos sin device de audio.

Ruta kit en tests definida como `LABRC_KIT_ROOT` string por CMake, no runtime plugin. Generador de BinaryData solo 9 WAV v06+3 PNG, no v05 ni referencia vintage. No descarga implícita, JUCE_ROOT cache variable requerida o build/dependencies/juce local.
