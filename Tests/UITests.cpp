#include "../Source/UI/ConsoleFaderScale.h"
#include "../Source/UI/EditorGeometry.h"
#include "../Source/UI/SceneComposition.h"
#include <cmath>
#include <iostream>
#include <thread>

#if ! defined (LABRC_UI_STANDALONE_TEST)
#include <juce_core/juce_core.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#endif

namespace
{
template <typename Check>
void geometryTests (Check& test)
{
    using namespace labrc::ui;
    // A linear gain slider would put the console markings at the wrong values.
    const double positions[] { 0.0, .15, .36, .65, .86, 1.0 };
    const double decibels[] { -60.0, -43.0, -27.0, -10.0, 0.0, 6.0 };
    for (int i = 0; i < 6; ++i)
    {
        test.near (ConsoleFaderScale::decibelsForPosition (positions[i]), decibels[i], 1.e-10,
                   "Console fader must meet every dB anchor");
        test.near (ConsoleFaderScale::positionForDecibels (decibels[i]), positions[i], 1.e-10,
                   "Console fader must invert every dB anchor");
    }
    double last = -61.0;
    for (int i = 0; i <= 1000; ++i)
    {
        const auto position = i / 1000.0;
        const auto db = ConsoleFaderScale::decibelsForPosition (position);
        test.check (db > last, "Fader must remain strictly monotone between anchors");
        test.near (ConsoleFaderScale::positionForDecibels (db), position, 1.e-10,
                   "Fader inverse must not move a value on recall");
        last = db;
    }
    test.near (ConsoleFaderScale::decibelsForPosition (-2), -60.0, 0.0, "Clamp fader bottom");
    test.near (ConsoleFaderScale::positionForDecibels (100), 1.0, 0.0, "Clamp fader top");

    // Literal fixtures catch cropping, stretching, or fitting to the wrong axis.
    const auto portrait = fitContain ({ 20, 40, 600, 300 }, 1448, 1086);
    test.near (portrait.x, 120, 1.e-9, "4:3 photo centred horizontally");
    test.near (portrait.y, 40, 1.e-9, "4:3 photo fills stage height");
    test.near (portrait.width, 400, 1.e-9, "4:3 photo must not stretch");
    test.near (portrait.height, 300, 1.e-9, "4:3 photo must not crop");
    const auto wide = fitContain ({ 0, 0, 300, 400 }, 1671, 941);
    test.near (wide.width, 300, 1.e-9, "Panoramic photo fills stage width");
    test.near (wide.height, 168.9407540394973, 1.e-8, "Panoramic photo preserves ratio");
    test.near (wide.y, 115.52962298025135, 1.e-8, "Panoramic photo centred vertically");
    test.check (fitContain ({ 0, 0, 0, 40 }, 1448, 1086).empty(), "Empty stage remains empty");
    test.check (fitContain ({ 0, 0, 40, 40 }, 0, 1086).empty(), "Invalid image has no geometry");

    for (const auto size : { Size { 1024, 684 }, Size { 1280, 854 }, Size { 1586, 1058 } })
    {
        const auto layout = EditorGeometry::forSize (size.width, size.height);
        const Rect outer { 0, 0, size.width, size.height };
        for (const auto region : { layout.header, layout.input, layout.output, layout.tabs,
                                   layout.stage, layout.sceneFooter, layout.pairs[0],
                                   layout.pairs[1], layout.pairs[2], layout.footer })
            test.check (! region.empty() && outer.contains (region), "All panels stay inside resized editor");
        test.check (layout.stage.y >= layout.tabs.bottom(), "Room tabs must not cover the photo");
        test.check (layout.pairs[0].y >= layout.sceneFooter.bottom(), "Mix strips must not cover the scene");
        test.check (layout.pairs[0].right() <= layout.pairs[1].x, "EARS and FRONT must not overlap");
        test.check (layout.pairs[1].right() <= layout.pairs[2].x, "FRONT and FLOOR must not overlap");
        const auto image = fitContain (layout.stage, 1448, 1086);
        for (int room = 0; room < 3; ++room)
            for (int mic = 0; mic < 6; ++mic)
            {
                const auto point = markerPoint (room, mic, image);
                test.check (image.contains (point), "Six markers stay on the contain photo");
            }
    }
    // A marker relative to the stage would move when a 4:3 image is letterboxed.
    const auto marker = markerPoint (0, 0, { 120, 40, 400, 300 });
    test.near (marker.x, 276, 1.e-9, "Kentucky EARS L uses 39% of the photo width");
    test.near (marker.y, 202, 1.e-9, "Kentucky EARS L uses 54% of the photo height");

    const auto finalLayout = EditorGeometry::forSize (1586, 1058);
    test.near (finalLayout.stage.height, 350, 1.e-9, "Final Touch leaves room for a readable three-band EQ");
    test.check (finalLayout.pairs[0].height >= 370, "Final Touch modules must fit three complete EQ bands");
    const auto kentuckyKit = drumLayerBounds (0, { 100, 200, 400, 300 });
    test.near (kentuckyKit.x, 242, 1.e-9, "Kentucky layer stays at the previous drum position");
    test.near (kentuckyKit.y, 342.5, 1.e-9, "Kentucky layer preserves the drum floor location");
    test.near (kentuckyKit.width, 124, 1.e-9, "Kentucky kit remains contextual rather than room-sized");
    test.near (kentuckyKit.height, 93, 1.e-9, "Kit alpha layer preserves its own aspect ratio");
    for (int room = 0; room < 3; ++room)
    {
        const auto photo = room == 1 ? Rect { 0, 0, 600, 600.0 * 941 / 1671 } : Rect { 0, 0, 400, 300 };
        test.check (photo.contains (drumLayerBounds (room, photo)), "Whole kit stays inside the unmodified photograph");
    }
}

#if defined (LABRC_UI_STANDALONE_TEST)
struct Checks
{
    int failures = 0;
    void check (bool ok, const char* reason)
    {
        if (! ok) { ++failures; std::cerr << "FAIL: " << reason << '\n'; }
    }
    void near (double value, double expected, double tolerance, const char* reason)
    {
        check (std::isfinite (value) && std::abs (value - expected) <= tolerance, reason);
    }
};
#else
juce::Component* findComponent (juce::Component& parent, const juce::String& id)
{
    if (parent.getComponentID() == id) return &parent;
    for (auto* child : parent.getChildren())
        if (auto* result = findComponent (*child, id)) return result;
    return nullptr;
}
void consumeGuiFrame()
{
    struct FrameProbe final : juce::Timer
    {
        FrameProbe() { startTimer (40); }
        void timerCallback() override { arrived = true; stopTimer(); }
        bool arrived = false;
    } probe;
    const auto deadline = juce::Time::getMillisecondCounterHiRes() + 750.0;
    // Wait for an actual timer dispatch, rather than assuming the timer thread ran during one sleep.
    while (! probe.arrived && juce::Time::getMillisecondCounterHiRes() < deadline)
    {
        juce::Thread::sleep (5);
        juce::Timer::callPendingTimersSynchronously();
    }
}

class UITests final : public juce::UnitTest
{
public:
    UITests() : juce::UnitTest ("UI geometry and console controls", "UI") {}
    void runTest() override
    {
        beginTest ("Console anchors, exact inverse, contain and minimum editor geometry");
        geometryTests (*this);

        LabRCProcessor processor;
        LabRCEditor editor (processor);
        editor.setVisible (true);
        beginTest ("Every EQ band is editable and each EQ IN reflects its own persistent parameter");
        for (const auto* pair : { "ears", "front", "floor" })
        {
            for (const auto* band : { "Low", "Mid", "High" })
            {
                const auto id = juce::String (pair) + "Eq" + band + "Db";
                auto* readout = dynamic_cast<juce::Label*> (findComponent (editor, id + "Value"));
                expect (readout != nullptr, "Editable EQ band: " + id);
                if (readout != nullptr)
                {
                    expectEquals (readout->getText(), juce::String ("0.0 dB"), "Neutral EQ must display zero without a positive-zero artefact");
                    processor.undoManager.clearUndoHistory();
                    readout->setText ("+5.2 dB", juce::sendNotificationSync);
                    expectWithinAbsoluteError (processor.apvts.getRawParameterValue (id)->load(), 5.2f, .001f);
                    expect (processor.undoUser());
                    expectWithinAbsoluteError (processor.apvts.getRawParameterValue (id)->load(), 0.0f, .001f);
                }
            }
            const auto id = juce::String (pair) + "EqEnabled";
            auto* button = dynamic_cast<juce::Button*> (findComponent (editor, id));
            expect (button != nullptr, "Real EQ IN control: " + id);
            if (button != nullptr)
            {
                expect (! button->getToggleState());
                button->onClick();
                expectWithinAbsoluteError (processor.apvts.getRawParameterValue (id)->load(), 1.0f, .001f);
                consumeGuiFrame();
                expect (button->getToggleState());
                button->onClick();
            }
        }
        beginTest ("All new EQ automation reaches GUI polling without user undo");
        processor.undoManager.clearUndoHistory();
        std::thread eqWorker ([&]
        {
            for (const auto* pair : { "ears", "front", "floor" })
            {
                for (const auto* band : { "Low", "Mid", "High" })
                {
                    auto* parameter = processor.apvts.getParameter (juce::String (pair) + "Eq" + band + "Db");
                    parameter->setValueNotifyingHost (parameter->convertTo0to1 (-3.4f));
                }
                processor.apvts.getParameter (juce::String (pair) + "EqEnabled")->setValueNotifyingHost (1.0f);
            }
        });
        eqWorker.join();
        consumeGuiFrame();
        for (const auto* pair : { "ears", "front", "floor" })
        {
            for (const auto* band : { "Low", "Mid", "High" })
                if (auto* label = dynamic_cast<juce::Label*> (findComponent (editor, juce::String (pair) + "Eq" + band + "DbValue")))
                    expectWithinAbsoluteError (static_cast<float> (label->getText().getDoubleValue()), -3.4f, .001f);
            if (auto* button = dynamic_cast<juce::Button*> (findComponent (editor, juce::String (pair) + "EqEnabled"))) expect (button->getToggleState());
        }
        expect (! processor.canUndoUser());

        beginTest ("EQ controls remain directly reachable and numeric values fit at the supported minimum");
        editor.setSize (1024, 684);
        for (const auto* pair : { "ears", "front", "floor" })
            for (const auto* band : { "Low", "Mid", "High" })
            {
                const auto id = juce::String (pair) + "Eq" + band + "Db";
                if (auto* control = findComponent (editor, id + "Slider"))
                {
                    expect (control->getWidth() >= 36 && control->getHeight() >= 36);
                    const auto centre = editor.getLocalPoint (control, control->getLocalBounds().getCentre());
                    expect (editor.getComponentAt (centre) == control, "Reachable EQ knob " + id);
                }
                if (auto* label = dynamic_cast<juce::Label*> (findComponent (editor, id + "Value")))
                {
                    expect (label->getWidth() >= 60 && label->getHeight() >= 22);
                    const auto maximum = juce::GlyphArrangement::getStringWidth (label->getFont(), "+12.0 dB");
                    expect (maximum <= label->getWidth() - 6, "Extreme EQ value remains fully visible");
                }
            }
        beginTest ("Output knob remains reachable behind the transparent mix container at every size");
        for (const auto size : { labrc::ui::Size { 1024, 684 }, labrc::ui::Size { 1280, 854 }, labrc::ui::Size { 1586, 1058 } })
        {
            editor.setSize (static_cast<int> (size.width), static_cast<int> (size.height));
            const auto layout = labrc::ui::EditorGeometry::forSize (size.width, size.height);
            const auto ox = (size.width - 1586 * layout.scale) * .5;
            const auto oy = (size.height - 1058 * layout.scale) * .5;
            for (const auto& item : std::array<std::pair<const char*, labrc::ui::Point>, 3> {{
                    { "inputDbSlider", { 127, 203 } }, { "outputDbSlider", { 1458, 203 } }, { "mixPercentSlider", { 1459, 359 } } }})
            {
                auto* hit = editor.getComponentAt (juce::roundToInt (ox + item.second.x * layout.scale),
                                                   juce::roundToInt (oy + item.second.y * layout.scale));
                expect (hit != nullptr && hit->getComponentID() == item.first, juce::String ("Reachable ") + item.first);
            }
        }

        beginTest ("Numeric entry changes the real parameter once and rejects malformed numbers");
        auto* value = dynamic_cast<juce::Label*> (findComponent (editor, "inputDbValue"));
        expect (value != nullptr);
        if (value != nullptr)
        {
            processor.undoManager.clearUndoHistory();
            value->setText ("-14.2 dB", juce::sendNotificationSync);
            expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("inputDb")->load(), -14.2f, .001f);
            expect (processor.undoManager.undo());
            expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("inputDb")->load(), 0.0f, .001f);
            expect (! processor.undoManager.canUndo(), "One entry is one user transaction");
            value->setText ("3-2 dB", juce::sendNotificationSync);
            expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("inputDb")->load(), 0.0f, .001f,
                                       "Malformed number must not unexpectedly turn into 3 dB");
            value->setText ("99 dB", juce::sendNotificationSync);
            expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("inputDb")->load(), 18.0f, .001f);
        }

        beginTest ("Room selection keeps global faders and the six stable marker components");
        std::array<juce::Component*, 6> markerPointers {};
        for (int i = 0; i < 6; ++i)
        {
            markerPointers[static_cast<std::size_t> (i)] = findComponent (editor, "mic" + juce::String (i));
            expect (markerPointers[static_cast<std::size_t> (i)] != nullptr);
        }
        if (auto* earsValue = dynamic_cast<juce::Label*> (findComponent (editor, "earsDbValue")))
            earsValue->setText ("-1.2 dB", juce::sendNotificationSync);
        for (int room = 0; room < 3; ++room)
        {
            if (auto* button = dynamic_cast<juce::Button*> (findComponent (editor, "room" + juce::String (room)))) button->onClick();
            expectEquals (static_cast<int> (processor.apvts.getRawParameterValue ("room")->load()), room);
            expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("earsDb")->load(), -1.2f, .001f);
            for (int i = 0; i < 6; ++i)
                expect (findComponent (editor, "mic" + juce::String (i)) == markerPointers[static_cast<std::size_t> (i)],
                        "Room change must not destroy keyboard focus by replacing markers");
        }
        beginTest ("MICS is an undoable instance preference and has no audio parameter side effects");
        const auto earsBefore = processor.apvts.getRawParameterValue ("earsDb")->load();
        if (auto* button = dynamic_cast<juce::Button*> (findComponent (editor, "micsVisible"))) button->onClick();
        expect (! processor.micsVisible());
        for (auto* marker : markerPointers) if (marker) expect (! marker->isVisible());
        expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("earsDb")->load(), earsBefore, .00001f);
        expect (processor.undoManager.undo());
        expect (processor.micsVisible());

        beginTest ("Host parameter updates reach controls without adding user undo");
        processor.undoManager.clearUndoHistory();
        auto* inputParameter = processor.apvts.getParameter ("inputDb");
        inputParameter->setValueNotifyingHost (inputParameter->convertTo0to1 (-8.7f));
        consumeGuiFrame();
        if (value) expect (value->getText().startsWith ("-8.7"), "After GUI refresh: " + value->getText());
        expect (! processor.undoManager.canUndo());

        beginTest ("GUI observes host parameters only at GUI refresh, with no synchronous listener push");
        inputParameter->setValueNotifyingHost (inputParameter->convertTo0to1 (-4.3f));
        if (value) expect (value->getText().startsWith ("-8.7"), "Host setter must not invoke GUI control updates");
        consumeGuiFrame();
        if (value) expect (value->getText().startsWith ("-4.3"));

        beginTest ("Worker automation of every parameter reaches the next GUI frame without user undo");
        const std::array<std::pair<const char*, float>, 14> automation {{
            { "room", 1 }, { "inputDb", -2.1f }, { "outputDb", 3.2f }, { "mixPercent", 41.3f },
            { "earsDb", -14.4f }, { "frontDb", -25.5f }, { "floorDb", -36.6f },
            { "earsMute", 1 }, { "frontMute", 0 }, { "floorMute", 1 },
            { "earsSolo", 1 }, { "frontSolo", 1 }, { "floorSolo", 0 }, { "bypass", 1 }
        }};
        const auto beforeWorker = value != nullptr ? value->getText() : juce::String();
        processor.undoManager.clearUndoHistory();
        std::thread worker ([&]
        {
            for (const auto& item : automation)
            {
                auto* parameter = processor.apvts.getParameter (item.first);
                parameter->setValueNotifyingHost (parameter->convertTo0to1 (item.second));
            }
        });
        worker.join();
        if (value) expectEquals (value->getText(), beforeWorker, "Worker must not touch a GUI component");
        consumeGuiFrame();
        for (int i = 1; i < 7; ++i)
            if (auto* label = dynamic_cast<juce::Label*> (findComponent (editor, juce::String (automation[static_cast<std::size_t> (i)].first) + "Value")))
                expectWithinAbsoluteError (static_cast<float> (label->getText().getDoubleValue()), automation[static_cast<std::size_t> (i)].second, .001f);
        if (auto* selected = dynamic_cast<juce::Button*> (findComponent (editor, "room1"))) expect (selected->getToggleState());
        for (int i = 7; i < 14; ++i)
            if (auto* button = dynamic_cast<juce::Button*> (findComponent (editor, automation[static_cast<std::size_t> (i)].first)))
                expect (button->getToggleState() == (automation[static_cast<std::size_t> (i)].second >= .5f));
        expect (! processor.undoManager.canUndo());

        beginTest ("Numeric editing survives GUI frames and automation, then cancels or commits normally");
        if (value)
        {
            value->showEditor();
            if (auto* textEditor = value->getCurrentTextEditor()) textEditor->setText ("-6.4", false);
            inputParameter->setValueNotifyingHost (inputParameter->convertTo0to1 (4.2f));
            consumeGuiFrame();
            expect (value->isBeingEdited(), "Polling must not call Label::setText while the user types");
            if (auto* textEditor = value->getCurrentTextEditor()) expectEquals (textEditor->getText(), juce::String ("-6.4"));
            value->hideEditor (true);
            consumeGuiFrame();
            expectWithinAbsoluteError (static_cast<float> (value->getText().getDoubleValue()), 4.2f, .001f);
            expect (! processor.undoManager.canUndo(), "Cancel and host automation must not create user actions");
            value->showEditor();
            if (auto* textEditor = value->getCurrentTextEditor()) textEditor->setText ("-5.6", false);
            consumeGuiFrame();
            value->hideEditor (false);
            expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("inputDb")->load(), -5.6f, .001f);
            expect (processor.undoManager.undo());
            expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("inputDb")->load(), 4.2f, .001f);
            expect (! processor.undoManager.canUndo(), "Committed entry remains one user action");
        }

        beginTest ("Mic label bounds stay on the original photograph at minimum size in every room");
        editor.setSize (1024, 684);
        for (int room = 0; room < 3; ++room)
        {
            auto* roomParameter = processor.apvts.getParameter ("room");
            roomParameter->setValueNotifyingHost (roomParameter->convertTo0to1 (static_cast<float> (room)));
            consumeGuiFrame();
            if (auto* marker = findComponent (editor, "mic0"))
            {
                auto* photograph = marker->getParentComponent();
                const auto photo = labrc::ui::fitContain ({ 0, 0, static_cast<double> (photograph->getWidth()), static_cast<double> (photograph->getHeight()) },
                                                          room == 1 ? 1671 : 1448, room == 1 ? 941 : 1086);
                for (int mic = 0; mic < 6; ++mic)
                    if (auto* label = findComponent (editor, "mic" + juce::String (mic)))
                    {
                        const auto bounds = label->getBounds();
                        expect (photo.contains (labrc::ui::Rect { static_cast<double> (bounds.getX()), static_cast<double> (bounds.getY()),
                                                                 static_cast<double> (bounds.getWidth()), static_cast<double> (bounds.getHeight()) }),
                                "Mic marker hit area must remain on the contain photo");
                    }
            }
        }

        beginTest ("Open editor recalls instance size and cannot undo changes from the previous restored state");
        LabRCProcessor savedInstance;
        savedInstance.beginParameterGesture ("inputDb", "Saved input");
        savedInstance.setParameterFromUser ("inputDb", 7.7f);
        savedInstance.endParameterGesture ("inputDb");
        savedInstance.setEditorSize (1280, 854);
        juce::MemoryBlock recalled;
        savedInstance.getStateInformation (recalled);
        processor.undoManager.clearUndoHistory();
        processor.beginParameterGesture ("inputDb", "Old session input");
        processor.setParameterFromUser ("inputDb", -3.3f);
        processor.endParameterGesture ("inputDb");
        std::thread recallWorker ([&] { processor.setStateInformation (recalled.getData(), static_cast<int> (recalled.getSize())); });
        recallWorker.join();
        if (auto* undoButton = dynamic_cast<juce::Button*> (findComponent (editor, "undo"))) undoButton->onClick();
        expectWithinAbsoluteError (processor.apvts.getRawParameterValue ("inputDb")->load(), 7.7f, .001f,
                                   "Immediate undo after off-thread recall must not apply an old-session action");
        consumeGuiFrame();
        expectEquals (editor.getWidth(), 1280);
        expectEquals (editor.getHeight(), 854);
        expectEquals (processor.editorSize().x, 1280);
        editor.setSize (1024, 684);
        consumeGuiFrame();
        expectEquals (editor.getWidth(), 1024, "A subsequent manual resize takes precedence");
        expectEquals (processor.editorSize().x, 1024);

        beginTest ("Repeated editor recreation recalls EQ, overlays and the migrated layout");
        LabRCProcessor reopened;
        reopened.setEditorSize (1280, 854);
        reopened.beginParameterGesture ("frontEqLowDb", "Front low");
        reopened.setParameterFromUser ("frontEqLowDb", 4.1f);
        reopened.endParameterGesture ("frontEqLowDb");
        reopened.setMicsVisible (false);
        for (int opening = 0; opening < 4; ++opening)
        {
            std::unique_ptr<juce::AudioProcessorEditor> window (reopened.createEditor());
            expect (window != nullptr);
            if (! window) continue;
            expectEquals (window->getWidth(), 1280);
            expectEquals (window->getHeight(), 854);
            if (auto* label = dynamic_cast<juce::Label*> (findComponent (*window, "frontEqLowDbValue")))
                expectWithinAbsoluteError (static_cast<float> (label->getText().getDoubleValue()), 4.1f, .001f);
            if (auto* marker = findComponent (*window, "mic0")) expect (! marker->isVisible());
        }
    }
    void check (bool ok, const char* reason) { expect (ok, reason); }
    void near (double value, double expected, double tolerance, const char* reason)
    {
        expect (std::isfinite (value) && std::abs (value - expected) <= tolerance, reason);
    }
};
static UITests uiTests;
#endif
}

#if defined (LABRC_UI_STANDALONE_TEST)
int main()
{
    Checks checks;
    geometryTests (checks);
    std::cout << "UI geometry: " << checks.failures << " failures\n";
    return checks.failures == 0 ? 0 : 1;
}
#endif
