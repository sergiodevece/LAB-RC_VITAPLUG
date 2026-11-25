#include "PluginEditor.h"
#include "PluginProcessor.h"

using namespace labrc::ui;

namespace
{
juce::Rectangle<int> rectangle (Rect r)
{
    return juce::Rectangle<float> (static_cast<float> (r.x), static_cast<float> (r.y),
                                   static_cast<float> (r.width), static_cast<float> (r.height)).toNearestInt();
}
constexpr const char* roomNames[] { "KENTUCKY", "CENTER FIELD", "LIVE ROOM" };
constexpr const char* roomStudios[] { "STUDIO A", "STUDIO A", "STUDIO B" };
constexpr const char* pairIDs[] { "ears", "front", "floor" };
}

LabRCEditor::LabRCEditor (LabRCProcessor& p)
    : juce::AudioProcessorEditor (&p), processor (p), tooltip (this, 650),
      input (p, "inputDb", "Input trim", false), output (p, "outputDb", "Output trim", false),
      mix (p, "mixPercent", "Dry wet mix", false), bypass ("BYPASS"), mics ("MICS ON"),
      reset ("RESET"), undo ("UNDO"), redo ("REDO")
{
    setName ("LAB-RC - Manza's Rooms & Chambers");
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);
    setOpaque (true);
    for (auto* component : std::array<juce::Component*, 11> {{ &input, &output, &mix, &inputMeter, &outputMeter, &scene,
                                                              &bypass, &mics, &reset, &undo, &redo }})
        addAndMakeVisible (*component);
    for (int i = 0; i < 3; ++i)
    {
        auto& room = rooms[static_cast<std::size_t> (i)];
        room = std::make_unique<VintageButton> (roomNames[i], VintageButton::Kind::room);
        room->setComponentID ("room" + juce::String (i));
        room->setTitle (juce::String (roomNames[i]) + " room, " + roomStudios[i]);
        room->setSubtitle (roomStudios[i]);
        room->setTooltip ("Select " + juce::String (roomNames[i]) + ". Pair faders remain global.");
        room->onClick = [this, i] { changeParameter ("room", static_cast<float> (i), "Select room"); };
        addAndMakeVisible (*room);
        pairs[static_cast<std::size_t> (i)] = std::make_unique<PairStrip> (p, i);
        addAndMakeVisible (*pairs[static_cast<std::size_t> (i)]);
    }
    bypass.setComponentID ("bypass");
    bypass.setTooltip ("Bypass to the original dry signal at unity. Parameters are preserved.");
    bypass.onClick = [this]
    {
        changeParameter ("bypass", processor.apvts.getRawParameterValue ("bypass")->load() >= .5f ? 0.0f : 1.0f, "Bypass");
        refreshPresentation();
    };
    mics.setComponentID ("micsVisible");
    mics.setTooltip ("Show or hide the six illustrative virtual microphone markers. Audio is unchanged.");
    mics.onClick = [this] { processor.setMicsVisible (! processor.micsVisible()); refreshPresentation(); };
    reset.setComponentID ("reset");
    reset.setTooltip ("Restore default parameters and show mic markers in one undoable transaction.");
    reset.onClick = [this] { processor.resetParameters(); refreshPresentation(); };
    undo.setComponentID ("undo");
    undo.setTooltip ("Undo the last user change (Command/Ctrl Z)");
    undo.onClick = [this] { processor.undoUser(); refreshPresentation(); };
    redo.setComponentID ("redo");
    redo.setTooltip ("Redo a user change (Command/Ctrl Shift Z)");
    redo.onClick = [this] { processor.redoUser(); refreshPresentation(); };
    scene.onHighlight = [this] (int pair) { highlightedPair = pair; refreshPresentation(); };
    selectRoom (juce::roundToInt (processor.apvts.getRawParameterValue ("room")->load (std::memory_order_relaxed)));
    refreshPresentation();
    setResizable (true, true);
    setResizeLimits (800, 500, 1586, 992);
    if (auto* constrainer = getConstrainer()) constrainer->setFixedAspectRatio (1586.0 / 992.0);
    const auto preferredSize = processor.editorSize();
    setSize (juce::jlimit (800, 1586, preferredSize.x), juce::jlimit (500, 992, preferredSize.y));
    restoringSize = false;
    startTimerHz (30);
}
LabRCEditor::~LabRCEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}
void LabRCEditor::changeParameter (const juce::String& id, float actual, const juce::String& name)
{
    processor.beginParameterGesture (id, name);
    processor.setParameterFromUser (id, actual);
    processor.endParameterGesture (id);
    refreshPresentation();
}
void LabRCEditor::selectRoom (int index)
{
    selectedRoom = juce::jlimit (0, 2, index);
    scene.setRoom (selectedRoom);
    for (int i = 0; i < 3; ++i)
        if (rooms[static_cast<std::size_t> (i)] != nullptr)
            rooms[static_cast<std::size_t> (i)]->setToggleState (i == selectedRoom, juce::dontSendNotification);
    repaint();
}
void LabRCEditor::refreshPresentation()
{
    const auto value = [this] (const juce::String& id)
    { const auto* raw = processor.apvts.getRawParameterValue (id); return raw != nullptr ? raw->load() : 0.0f; };
    const auto nextRoom = juce::jlimit (0, 2, juce::roundToInt (value ("room")));
    if (nextRoom != selectedRoom) selectRoom (nextRoom);
    input.refreshFromParameter();
    output.refreshFromParameter();
    mix.refreshFromParameter();
    const auto bypassed = value ("bypass") >= .5f;
    bypass.setToggleState (bypassed, juce::dontSendNotification);
    const auto visible = processor.micsVisible();
    scene.setMicsVisible (visible);
    mics.setToggleState (visible, juce::dontSendNotification);
    mics.setName (visible ? "MICS ON" : "MICS OFF");
    std::array<bool, 3> muted {}, soloed {};
    bool anySolo = false;
    for (int i = 0; i < 3; ++i)
    {
        muted[static_cast<std::size_t> (i)] = value (juce::String (pairIDs[i]) + "Mute") >= .5f;
        soloed[static_cast<std::size_t> (i)] = value (juce::String (pairIDs[i]) + "Solo") >= .5f;
        anySolo = anySolo || soloed[static_cast<std::size_t> (i)];
    }
    for (int i = 0; i < 3; ++i)
    {
        const auto active = ! muted[static_cast<std::size_t> (i)] && (! anySolo || soloed[static_cast<std::size_t> (i)]);
        pairs[static_cast<std::size_t> (i)]->setPresentation (muted[static_cast<std::size_t> (i)], soloed[static_cast<std::size_t> (i)],
                                                           active, bypassed, highlightedPair == i);
        pairs[static_cast<std::size_t> (i)]->refreshFromParameters();
        scene.setPairState (i, active);
    }
    undo.setEnabled (processor.canUndoUser());
    redo.setEnabled (processor.canRedoUser());
}
void LabRCEditor::timerCallback()
{
    const auto restoredSize = processor.editorSize();
    if (restoredSize.x != getWidth() || restoredSize.y != getHeight())
    {
        const juce::ScopedValueSetter<bool> guard (restoringSize, true);
        setSize (restoredSize.x, restoredSize.y);
    }
    const auto meters = processor.meterSnapshot();
    inputMeter.setLevels (meters.input);
    outputMeter.setLevels (meters.output);
    for (int i = 0; i < 3; ++i) pairs[static_cast<std::size_t> (i)]->setLevels (meters.pairs[static_cast<std::size_t> (i)]);
    refreshPresentation();
}
bool LabRCEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.getModifiers().isCommandDown() && (key.getKeyCode() == 'Z' || key.getKeyCode() == 'z'))
    {
        if (key.getModifiers().isShiftDown()) processor.redoUser();
        else processor.undoUser();
        refreshPresentation();
        return true;
    }
    return false;
}
void LabRCEditor::resized()
{
    const auto geometry = EditorGeometry::forSize (getWidth(), getHeight());
    const auto scale = static_cast<float> (geometry.scale);
    const auto ox = (getWidth() - 1586 * scale) * .5f, oy = (getHeight() - 992 * scale) * .5f;
    const auto rect = [=] (float x, float y, float w, float h)
    { return juce::Rectangle<float> (ox + x * scale, oy + y * scale, w * scale, h * scale).toNearestInt(); };
    scene.setBounds (rectangle (geometry.stage));
    const auto tabWidth = (geometry.tabs.width - 18 * scale) / 3.0;
    for (int i = 0; i < 3; ++i)
    {
        rooms[static_cast<std::size_t> (i)]->setBounds (rectangle ({ geometry.tabs.x + i * (tabWidth + 9 * scale),
                                                                 geometry.tabs.y, tabWidth, geometry.tabs.height }));
        pairs[static_cast<std::size_t> (i)]->setBounds (rectangle (geometry.pairs[static_cast<std::size_t> (i)]));
    }
    input.setBounds (rectangle (geometry.input));
    output.setBounds (rectangle (geometry.output));
    mix.setBounds (rectangle (geometry.output));
    const auto localRect = [=] (float x, float y, float w, float h)
    { return juce::Rectangle<float> (x * scale, y * scale, w * scale, h * scale).toNearestInt(); };
    input.layout (localRect (41, 43, 122, 112), localRect (40, 156, 128, 30));
    output.layout (localRect (42, 43, 116, 112), localRect (38, 156, 131, 30));
    mix.layout (localRect (51, 247, 100, 94), localRect (39, 339, 130, 29));
    inputMeter.setBounds (rect (75, 370, 107, 210));
    outputMeter.setBounds (rect (1403, 500, 112, 109));
    bypass.setBounds (rect (1380, 610, 160, 44).withHeight (std::max (28, juce::roundToInt (44 * scale))));
    mics.setBounds (rect (1220, 619, 104, 31).withHeight (std::max (28, juce::roundToInt (31 * scale))));
    const auto footerButtonHeight = std::max (28, juce::roundToInt (36 * scale));
    undo.setBounds (rect (1175, 931, 90, 36).withHeight (footerButtonHeight));
    redo.setBounds (rect (1273, 931, 90, 36).withHeight (footerButtonHeight));
    reset.setBounds (rect (1433, 931, 102, 36).withHeight (footerButtonHeight));
    if (! restoringSize) processor.setEditorSize (getWidth(), getHeight());
}
void LabRCEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0c100b));
    const auto layout = EditorGeometry::forSize (getWidth(), getHeight());
    const auto scale = static_cast<float> (layout.scale);
    const auto ox = (getWidth() - 1586 * scale) * .5f, oy = (getHeight() - 992 * scale) * .5f;
    g.addTransform (juce::AffineTransform::scale (scale).translated (ox, oy));
    theme::panel (g, { 2, 2, 1582, 988 }, 1.0f);
    theme::patina (g, { 6, 6, 1574, 980 }, 1.0f, 739);
    g.setColour (theme::gold().withAlpha (.8f));
    g.drawLine (18, 94, 1568, 94, 1.3f);
    g.drawLine (25, 97, 1561, 97, .5f);
    theme::lettering (g, "V I T A L A B", { 106, 20, 211, 35 }, 29, true, true, juce::Justification::centredLeft);
    theme::lettering (g, "V I T A P L U G", { 106, 55, 211, 24 }, 14, false, false, juce::Justification::centredLeft);
    juce::Path waveform;
    waveform.startNewSubPath (59, 41); waveform.lineTo (64, 41); waveform.lineTo (67, 33); waveform.lineTo (72, 49);
    waveform.lineTo (77, 27); waveform.lineTo (81, 56); waveform.lineTo (85, 38); waveform.lineTo (91, 43); waveform.lineTo (99, 43);
    g.setColour (juce::Colour (0xffbdc85c));
    g.strokePath (waveform, juce::PathStrokeType (1.7f));
    theme::lettering (g, juce::String::fromUTF8 ("MANZA’S ROOMS & CHAMBERS"), { 389, 13, 815, 45 }, 42, true, true);
    auto italic = theme::font (25, true);
    italic.setItalic (true);
    g.setFont (italic);
    g.setColour (theme::cream());
    g.drawText ("A Tribute to Steve Albini", juce::Rectangle<float> (593, 57, 410, 31), juce::Justification::centred);
    g.setColour (theme::gold());
    g.drawLine (416, 72, 581, 72, 1.2f); g.drawLine (1017, 72, 1188, 72, 1.2f);
    theme::lettering (g, "L A B - R C", { 1330, 23, 210, 33 }, 28, true, true);
    theme::lettering (g, "R O O M  M I C R O P H O N E  M I X E R", { 1314, 56, 226, 25 }, 10);
    theme::panel (g, { 25, 104, 201, 564 }, 1);
    theme::panel (g, { 1358, 104, 201, 564 }, 1);
    theme::panel (g, { 238, 107, 1110, 560 }, 1, false);
    theme::lettering (g, "I N P U T", { 53, 117, 147, 33 }, 23, true, true);
    theme::lettering (g, "O U T P U T", { 1380, 117, 158, 33 }, 23, true, true);
    theme::lettering (g, "S T E R E O  I N", { 48, 334, 155, 33 }, 16, true, true);
    theme::lettering (g, "M I X", { 1407, 331, 104, 34 }, 23, true, true);
    theme::lettering (g, "P E A K  -  d B F S", { 50, 592, 151, 25 }, 11);
    g.setColour (theme::gold().withAlpha (.7f));
    for (auto x : { 50.0f, 1383.0f })
    {
        g.drawLine (x, 315, x + 151, 315, .7f);
        g.drawLine (x, 317, x + 151, 317, .35f);
    }
    theme::lettering (g, juce::String (roomStudios[selectedRoom]) + " / " + roomNames[selectedRoom],
                       { 263, 619, 390, 31 }, 14, false, true, juce::Justification::centredLeft);
    theme::lettering (g, "V I R T U A L  M I C  P O S I T I O N S", { 653, 619, 470, 31 }, 12);
    g.setColour (theme::gold().withAlpha (.6f));
    g.drawLine (20, 918, 1566, 918, 1.1f);
    theme::lettering (g, "D E S I G N E D  B Y  S E R X D E V  -  V I T A L A B", { 60, 933, 580, 30 }, 12,
                       false, false, juce::Justification::centredLeft);
    theme::lettering (g, "LAB-RC  0.1.0", { 716, 933, 204, 30 }, 12);
}
