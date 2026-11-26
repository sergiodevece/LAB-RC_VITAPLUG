#include "VintageComponents.h"
#include "IndustrialSurface.h"
#include "../PluginProcessor.h"
#include "../Resources.h"
#include <cmath>
#include <optional>

namespace labrc::ui
{
namespace
{
constexpr const char* pairNames[] { "EARS", "FRONT", "FLOOR" };
constexpr const char* pairIDs[] { "ears", "front", "floor" };
constexpr const char* pairDescriptions[] { "RIBBON", "LARGE DIAPHRAGM", "SMALL DIAPHRAGM" };
constexpr const char* bandNames[] { "LOW", "MID", "HIGH" };
constexpr const char* bandIDs[] { "Low", "Mid", "High" };
juce::Rectangle<float> boundsOf (const juce::Component& c) { return c.getLocalBounds().toFloat(); }
void focusRing (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour (theme::cream().withAlpha (.95f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), 3.0f, 1.5f);
}
std::optional<double> numericEntry (juce::String text, bool percentage)
{
    text = text.trim();
    if (percentage && text.endsWithChar ('%')) text = text.dropLastCharacters (1).trimEnd();
    else if (! percentage && text.endsWithIgnoreCase ("dB")) text = text.dropLastCharacters (2).trimEnd();
    const auto* cursor = text.toRawUTF8();
    if (*cursor == '+' || *cursor == '-') ++cursor;
    const auto consumeDigits = [&cursor]
    {
        bool found = false;
        while (*cursor >= '0' && *cursor <= '9') { ++cursor; found = true; }
        return found;
    };
    const auto beforePoint = consumeDigits();
    bool afterPoint = false;
    if (*cursor == '.') { ++cursor; afterPoint = consumeDigits(); }
    if (! beforePoint && ! afterPoint) return std::nullopt;
    if (*cursor == 'e' || *cursor == 'E')
    {
        ++cursor;
        if (*cursor == '+' || *cursor == '-') ++cursor;
        if (! consumeDigits()) return std::nullopt;
    }
    if (*cursor != '\0') return std::nullopt;
    // JUCE's conversion is locale-independent; the readout consistently uses a decimal point.
    const auto value = text.getDoubleValue();
    return std::isfinite (value) ? std::optional<double> (value) : std::nullopt;
}
}

namespace theme
{
juce::Colour cream() { return juce::Colour (0xffe4c794); }
juce::Colour gold() { return juce::Colour (0xffa07c43); }
juce::Colour dark() { return juce::Colour (0xff171c13); }
juce::Colour pairColour (int pair)
{
    constexpr juce::uint32 colours[] { 0xff91bfa2, 0xffd5b477, 0xff8eafca };
    return juce::Colour (colours[juce::jlimit (0, 2, pair)]);
}
juce::Font font (float height, bool serif, bool bold)
{
    return juce::Font (juce::FontOptions (serif ? "Times New Roman" : juce::Font::getDefaultSansSerifFontName(),
                                        height, bold ? juce::Font::bold : juce::Font::plain));
}
void lettering (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> bounds,
                float height, bool serif, bool bold, juce::Justification justification)
{
    g.setColour (cream());
    auto textFont = font (height, serif, bold);
    const auto textWidth = juce::GlyphArrangement::getStringWidth (textFont, text);
    if (textWidth > bounds.getWidth() - 2)
        textFont.setHorizontalScale (std::max (.55f, (bounds.getWidth() - 2) / textWidth));
    g.setFont (textFont);
    g.setColour (juce::Colour (0xa0060905));
    g.drawText (text, bounds.translated (0, .8f), justification, true);
    g.setColour (cream());
    g.drawText (text, bounds, justification, true);
}
void patina (juce::Graphics& g, juce::Rectangle<float> bounds, float scale, int seed)
{
    industrial::wear (g, bounds, scale, seed);
}
void screw (juce::Graphics& g, float x, float y, float radius)
{
    industrial::screw (g, x, y, radius);
}
void panel (juce::Graphics& g, juce::Rectangle<float> bounds, float scale, bool screws)
{
    industrial::panel (g, bounds, scale, screws);
}

}

VintageLookAndFeel::VintageLookAndFeel()
{
    setColour (juce::TextEditor::backgroundColourId, theme::dark());
    setColour (juce::TextEditor::textColourId, theme::cream());
    setColour (juce::TextEditor::highlightColourId, theme::gold().withAlpha (.5f));
    setColour (juce::TextEditor::outlineColourId, theme::gold());
    setColour (juce::TextEditor::focusedOutlineColourId, theme::cream());
    setColour (juce::TooltipWindow::backgroundColourId, juce::Colour (0xff20271a));
    setColour (juce::TooltipWindow::textColourId, theme::cream());
    setColour (juce::TooltipWindow::outlineColourId, theme::gold());
}
void VintageLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float proportion, float start, float end, juce::Slider& slider)
{
    const auto size = static_cast<float> (std::min (width, height));
    const auto cx = x + width * .5f, cy = y + height * .5f;
    const auto r = size * .37f;
    const auto bakelite = static_cast<bool> (slider.getProperties()["labrcBakelite"]);
    const auto ticks = bakelite ? 4 : 10;
    for (int tick = 0; tick <= ticks; ++tick)
    {
        const auto a = start + (end - start) * tick / ticks;
        g.setColour (theme::cream().withAlpha (tick == ticks / 2 ? .95f : .65f));
        g.drawLine (cx + std::sin (a) * r * 1.18f, cy - std::cos (a) * r * 1.18f,
                    cx + std::sin (a) * r * 1.34f, cy - std::cos (a) * r * 1.34f, std::max (.7f, size / 100));
    }
    const auto angle = start + (end - start) * proportion;
    if (bakelite)
    {
        industrial::bakeliteKnob (g, cx, cy + 2, r * .94f, angle);
        g.setColour (theme::cream());
        g.setFont (theme::font (std::max (8.0f, size * .14f)));
        g.drawText ("0", juce::Rectangle<float> (cx - 6, static_cast<float> (y), 12, std::max (9.0f, size * .18f)), juce::Justification::centred);
    }
    else industrial::metalKnob (g, cx, cy, r, angle);
    if (slider.isMouseOverOrDragging())
    {
        g.setColour (theme::cream().withAlpha (.4f));
        g.drawEllipse (cx - r - 1, cy - r - 1, r * 2 + 2, r * 2 + 2, 1);
    }
    if (slider.hasKeyboardFocus (true)) focusRing (g, slider.getLocalBounds().toFloat());
}
int VintageLookAndFeel::getSliderThumbRadius (juce::Slider& slider)
{
    return slider.isVertical() ? std::max (7, juce::roundToInt (slider.getHeight() * .08f)) : 12;
}
void VintageLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider& slider)
{
    const auto scale = std::max (.4f, width / 132.0f);
    const auto trackX = x + width * .72f;
    const auto top = static_cast<float> (y), bottom = static_cast<float> (y + height);
    g.setColour (juce::Colour (0xff090e09));
    g.fillRoundedRectangle (trackX - 7 * scale, top - 5 * scale, 14 * scale, height + 10 * scale, 6 * scale);
    g.setColour (theme::gold().withAlpha (.7f));
    g.drawRoundedRectangle (trackX - 7 * scale, top - 5 * scale, 14 * scale, height + 10 * scale, 6 * scale, .9f * scale);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (trackX - 3 * scale, top - 3 * scale, 6 * scale, height + 6 * scale, 3 * scale);
    constexpr double marks[] { 6, 0, -10, -27, -43, -60 };
    g.setFont (theme::font (std::max (9.0f, 12.0f * scale)));
    for (auto db : marks)
    {
        const auto py = bottom - static_cast<float> (ConsoleFaderScale::positionForDecibels (db)) * height;
        g.setColour (theme::cream().withAlpha (.85f));
        const auto text = juce::String (db > 0 ? "+" : "") + juce::String (static_cast<int> (db));
        g.drawText (text, juce::Rectangle<float> (static_cast<float> (x), py - 8 * scale,
                                               width * .46f, 16 * scale), juce::Justification::centredRight);
        g.drawLine (trackX - 31 * scale, py, trackX - 15 * scale, py, .8f * scale);
    }
    const juce::Rectangle<float> cap { trackX - 21 * scale, sliderPos - 20 * scale, 42 * scale, 40 * scale };
    g.setColour (juce::Colours::black.withAlpha (.7f));
    g.fillRoundedRectangle (cap.translated (0, 4 * scale), 3 * scale);
    juce::ColourGradient metal (juce::Colour (0xffe2c895), cap.getX(), cap.getY(),
                                juce::Colour (0xff624b2d), cap.getX(), cap.getBottom(), false);
    metal.addColour (.15, juce::Colour (0xffdfc697));
    metal.addColour (.25, juce::Colour (0xff745d3b));
    metal.addColour (.40, juce::Colour (0xffc3a372));
    metal.addColour (.54, juce::Colour (0xfff1d6a5));
    metal.addColour (.66, juce::Colour (0xff987749));
    metal.addColour (.82, juce::Colour (0xffc6a475));
    g.setGradientFill (metal);
    g.fillRoundedRectangle (cap, 3 * scale);
    g.setColour (theme::cream());
    g.drawRoundedRectangle (cap, 3 * scale, .9f * scale);
    for (int groove = 1; groove < 5; ++groove)
    {
        const auto py = cap.getY() + groove * cap.getHeight() / 5;
        g.setColour (juce::Colour (0xff4c3d26).withAlpha (.7f));
        g.drawLine (cap.getX() + 3 * scale, py, cap.getRight() - 3 * scale, py, .7f * scale);
        g.setColour (juce::Colour (0xfff5dca8).withAlpha (.6f));
        g.drawLine (cap.getX() + 3 * scale, py + scale, cap.getRight() - 3 * scale, py + scale, .5f * scale);
    }
    g.setColour (juce::Colour (0xfff9e4b6));
    g.drawLine (cap.getX() + 3 * scale, cap.getY() + 2 * scale, cap.getRight() - 3 * scale, cap.getY() + 2 * scale, 1.1f * scale);
    g.setColour (juce::Colour (0xff4b3922));
    g.drawLine (cap.getX() + 2 * scale, cap.getCentreY(), cap.getRight() - 2 * scale, cap.getCentreY(), 1.3f * scale);
    g.setColour (juce::Colour (0xffefd1a0));
    g.drawLine (cap.getX() + 3 * scale, cap.getCentreY() + 1.5f * scale, cap.getRight() - 3 * scale, cap.getCentreY() + 1.5f * scale, .7f * scale);
    if (slider.hasKeyboardFocus (true)) focusRing (g, slider.getLocalBounds().toFloat());
}
void VintageLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    const auto bounds = label.getLocalBounds().toFloat().reduced (.8f);
    g.setColour (juce::Colour (0xff090f09));
    g.fillRoundedRectangle (bounds, 3);
    g.setColour (theme::gold());
    g.drawRoundedRectangle (bounds, 3, .9f);
    if (! label.isBeingEdited())
    {
        g.setColour (theme::cream());
        g.setFont (label.getFont());
        g.drawText (label.getText(), bounds.reduced (3, 0), label.getJustificationType(), true);
    }
    if (label.hasKeyboardFocus (true)) focusRing (g, bounds);
}
void VintageLookAndFeel::drawCornerResizer (juce::Graphics& g, int width, int height, bool over, bool dragging)
{
    const auto extent = static_cast<float> (std::min (width, height));
    for (float inset : { .28f, .48f, .68f })
    {
        g.setColour (theme::dark());
        g.drawLine (width - extent * inset, height - 2.0f, width - 2.0f, height - extent * inset, 2.0f);
        g.setColour ((over || dragging ? theme::cream() : theme::gold()).withAlpha (.8f));
        g.drawLine (width - extent * inset, height - 3.0f, width - 3.0f, height - extent * inset, .9f);
    }
}

VintageButton::VintageButton (juce::String name, Kind type)
    : juce::Button (std::move (name)), kind (type), accent (theme::gold())
{
    setWantsKeyboardFocus (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}
void VintageButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto disabled = ! isEnabled();
    if (disabled) g.beginTransparencyLayer (.38f);
    const auto local = getLocalBounds().toFloat();
    const auto on = getToggleState();
    const auto scale = std::max (.45f, std::min (local.getHeight() / (kind == Kind::room ? 60.0f : 38.0f), 1.4f));
    if (kind == Kind::marker)
    {
        const auto pill = local.reduced (2, std::max (2.0f, (local.getHeight() - 24 * scale) * .5f));
        g.setColour (theme::dark().withAlpha (routeActive ? .92f : .68f));
        g.fillRoundedRectangle (pill, pill.getHeight() * .5f);
        g.setColour (accent.withAlpha (routeActive ? .9f : .4f));
        g.drawRoundedRectangle (pill, pill.getHeight() * .5f, on ? 1.8f : .9f);
        const auto radius = std::max (2.0f, 3.5f * scale);
        g.setColour (accent.withMultipliedBrightness (routeActive ? 1.15f : .45f));
        g.fillEllipse (pill.getX() + 5 * scale, pill.getCentreY() - radius, radius * 2, radius * 2);
        g.setColour (theme::cream().withAlpha (routeActive ? 1.0f : .65f));
        const auto textBounds = pill.withTrimmedLeft (16 * scale).reduced (3, 0);
        auto markerFont = theme::font (std::max (8.5f, 12 * scale), false, true);
        const auto textWidth = juce::GlyphArrangement::getStringWidth (markerFont, getName());
        if (textWidth > textBounds.getWidth()) markerFont.setHorizontalScale (textBounds.getWidth() / textWidth);
        g.setFont (markerFont);
        g.drawText (getName(), textBounds, juce::Justification::centred, false);
        if (over || hasKeyboardFocus (false)) focusRing (g, pill);
        if (disabled) g.endTransparencyLayer();
        return;
    }
    auto body = local.reduced (2 * scale);
    if (down) body = body.translated (0, scale);
    const auto lit = on && (kind == Kind::solo || kind == Kind::room || kind == Kind::normal);
    const auto base = on && kind == Kind::mute ? juce::Colour (0xff8b3f2e)
                                              : lit ? juce::Colour (0xffa27c3b) : juce::Colour (0xff252c1e);
    if (on)
    {
        g.setColour ((kind == Kind::mute ? juce::Colour (0xffa95638) : theme::gold()).withAlpha (.20f));
        g.fillRoundedRectangle (body.expanded (2 * scale), 6 * scale);
    }
    g.setColour (juce::Colours::black.withAlpha (.7f));
    g.fillRoundedRectangle (body.translated (0, 2 * scale), 4 * scale);
    g.setGradientFill (juce::ColourGradient (base.brighter (.22f), body.getX(), body.getY(),
                                            base.darker (.35f), body.getX(), body.getBottom(), false));
    g.fillRoundedRectangle (body, 4 * scale);
    if (! on)
    {
        g.setTiledImageFill (industrial::surfaceTexture(), 0, 0, 1);
        g.fillRoundedRectangle (body.reduced (2 * scale), 3 * scale);
        if (kind == Kind::mute || kind == Kind::solo)
        {
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xb080795e), body.getX(), body.getY(),
                                                    juce::Colour (0xa02b2c22), body.getRight(), body.getBottom(), false));
            g.fillRoundedRectangle (body.reduced (2 * scale), 3 * scale);
        }
    }
    g.setColour (on ? juce::Colour (0xffe8bf6d) : theme::gold().withAlpha (.85f));
    g.drawRoundedRectangle (body, 4 * scale, on ? 1.5f * scale : .9f * scale);
    g.setColour (theme::cream().withAlpha (on ? .7f : .2f));
    g.drawRoundedRectangle (body.reduced (2 * scale), 3 * scale, .6f * scale);
    theme::patina (g, body, scale, getName().hashCode());
    if (kind == Kind::room)
    {
        theme::lettering (g, getName(), body.withTrimmedBottom (body.getHeight() * .33f),
                           std::max (10.0f, body.getHeight() * .38f), true, true);
        theme::lettering (g, subtitle, body.withTrimmedTop (body.getHeight() * .60f),
                           std::max (8.0f, body.getHeight() * .19f));
    }
    else
        theme::lettering (g, getName(), body, std::max (10.0f, std::min (20 * scale, body.getHeight() * .55f)),
                           kind == Kind::normal, true);
    if (over)
    {
        g.setColour (theme::cream().withAlpha (.35f));
        g.drawRoundedRectangle (body, 4 * scale, 1.0f);
    }
    if (hasKeyboardFocus (false)) focusRing (g, local);
    if (disabled) g.endTransparencyLayer();
}

ConsoleSlider::ConsoleSlider (bool fader) : consoleFader (fader)
{
    setSliderStyle (fader ? juce::Slider::LinearVertical : juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (220);
    setVelocityModeParameters (.25, 1, 0, true, juce::ModifierKeys::shiftModifier);
    setWantsKeyboardFocus (true);
    setScrollWheelEnabled (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}
double ConsoleSlider::proportionOfLengthToValue (double proportion)
{
    return consoleFader ? ConsoleFaderScale::decibelsForPosition (proportion)
                        : juce::Slider::proportionOfLengthToValue (proportion);
}
double ConsoleSlider::valueToProportionOfLength (double value)
{
    return consoleFader ? ConsoleFaderScale::positionForDecibels (value)
                        : juce::Slider::valueToProportionOfLength (value);
}
void ConsoleSlider::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    // Scrolling the host does not change a parameter merely because it is under the pointer.
    if (hasKeyboardFocus (false)) juce::Slider::mouseWheelMove (event, wheel);
}

ParameterControl::ParameterControl (LabRCProcessor& p, juce::String id, juce::String name, bool fader)
    : processor (p), parameterID (std::move (id)), parameterName (std::move (name)), slider (fader)
{
    setInterceptsMouseClicks (false, true);
    setComponentID (parameterID);
    slider.getProperties().set ("labrcBakelite", parameterID.contains ("Eq") && parameterID.endsWith ("Db"));
    slider.setComponentID (parameterID + "Slider");
    slider.setName (parameterName);
    slider.setTitle (parameterName);
    slider.setTooltip (parameterName + ": drag; Shift for fine; double-click for default. Focus to use wheel.");
    readout.setComponentID (parameterID + "Value");
    readout.setName (parameterName + " value");
    readout.setTitle (parameterName + " value");
    readout.setJustificationType (juce::Justification::centred);
    readout.setEditable (true, true, false);
    readout.setWantsKeyboardFocus (true);
    readout.setTooltip ("Click or press Return to enter " + parameterName.toLowerCase());
    addAndMakeVisible (slider);
    addAndMakeVisible (readout);
    if (auto* parameter = processor.apvts.getParameter (parameterID))
    {
        const auto range = parameter->getNormalisableRange();
        slider.setRange (range.start, range.end, range.interval);
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        rawParameter = processor.apvts.getRawParameterValue (parameterID);
        refreshFromParameter();
    }
    slider.onDragStart = [this]
    {
        dragging = true;
        processor.beginParameterGesture (parameterID, parameterName);
    };
    slider.onDragEnd = [this]
    {
        if (dragging) processor.endParameterGesture (parameterID);
        dragging = false;
    };
    slider.onValueChange = [this] { if (! updating) change (static_cast<float> (slider.getValue())); };
    readout.onTextChange = [this]
    {
        if (updating) return;
        if (const auto value = numericEntry (readout.getText(), parameterID == "mixPercent"))
            change (static_cast<float> (juce::jlimit (slider.getMinimum(), slider.getMaximum(), *value)));
        update (static_cast<float> (slider.getValue()));
    };
}
ParameterControl::~ParameterControl()
{
    if (dragging) processor.endParameterGesture (parameterID);
}
juce::String ParameterControl::formattedValue (double value) const
{
    value = std::round (value * 10.0) / 10.0;
    if (value == 0.0) value = 0.0; // Canonicalise the display sign, including slider-interval roundoff.
    return (value > 0 && parameterID != "mixPercent" ? "+" : "") + juce::String (value, 1)
           + (parameterID == "mixPercent" ? " %" : " dB");
}
void ParameterControl::update (float actual)
{
    const juce::ScopedValueSetter<bool> guard (updating, true);
    slider.setValue (actual, juce::dontSendNotification);
    const auto text = formattedValue (slider.getValue());
    // Label::setText closes an active editor even when its string is unchanged.
    if (! readout.isBeingEdited() && readout.getText (false) != text)
        readout.setText (text, juce::dontSendNotification);
}
void ParameterControl::refreshFromParameter()
{
    if (rawParameter != nullptr) update (rawParameter->load (std::memory_order_relaxed));
}
void ParameterControl::change (float actual)
{
    if (! dragging) processor.beginParameterGesture (parameterID, parameterName);
    processor.setParameterFromUser (parameterID, actual);
    if (! dragging) processor.endParameterGesture (parameterID);
    update (actual);
}
void ParameterControl::layout (juce::Rectangle<int> sliderArea, juce::Rectangle<int> readoutArea)
{
    slider.setBounds (sliderArea);
    readout.setBounds (readoutArea);
    auto typeface = theme::font (std::max (10.0f, readoutArea.getHeight() * .60f), false, true);
    typeface.setTypefaceName (juce::Font::getDefaultMonospacedFontName());
    const auto longest = std::max (juce::GlyphArrangement::getStringWidth (typeface, formattedValue (slider.getMinimum())),
                                    juce::GlyphArrangement::getStringWidth (typeface, formattedValue (slider.getMaximum())));
    if (longest > readoutArea.getWidth() - 8)
        typeface.setHeight (typeface.getHeight() * (readoutArea.getWidth() - 8) / longest);
    readout.setFont (typeface);
}

StereoMeter::StereoMeter()
{
    setName ("Stereo peak meter");
    setDescription ("Left and right peak level in dBFS. Red indicates clipping at zero dBFS.");
    setInterceptsMouseClicks (false, false);
}
void StereoMeter::setLevels (std::array<float, 2> linear)
{
    const auto now = juce::Time::getMillisecondCounterHiRes();
    for (std::size_t channel = 0; channel < 2; ++channel)
    {
        const auto level = std::isfinite (linear[channel]) ? std::max (0.0f, linear[channel]) : 0.0f;
        const auto db = juce::Decibels::gainToDecibels (level, -100.0f);
        displayDb[channel] = std::max (db, displayDb[channel] - 1.8f);
        if (level >= 1.0f) clippedUntil[channel] = now + 1500.0;
    }
    repaint();
}
void StereoMeter::paint (juce::Graphics& g)
{
    const auto area = boundsOf (*this);
    const auto scale = area.getHeight() / 195.0f;
    const auto textFont = theme::font (juce::jlimit (9.0f, 12.0f, area.getHeight() * .065f));
    const auto labelHeight = textFont.getHeight() + 2;
    const auto top = labelHeight * .5f + 2, bottom = area.getHeight() - textFont.getHeight() - 7;
    const auto meterHeight = bottom - top;
    const auto labelWidth = std::ceil (juce::GlyphArrangement::getStringWidth (textFont, "-48")) + 3;
    const auto columnWidth = juce::jlimit (5.0f, 12.0f, area.getWidth() * .15f);
    const auto gap = std::max (3.0f, columnWidth * .42f);
    const auto frame = juce::jlimit (1.0f, 2.0f, scale * 1.5f);
    const auto blockWidth = labelWidth + 5 + frame * 2 + columnWidth * 2 + gap;
    const auto start = std::max (0.0f, (area.getWidth() - blockWidth) * .5f);
    const auto left = start + labelWidth + 5 + frame;
    g.setFont (textFont);
    for (auto db : { 0, -6, -12, -24, -48 })
    {
        if (db == -6 && meterHeight * .1f < textFont.getHeight() + 1) continue;
        g.setColour (theme::cream().withAlpha (.8f));
        const auto y = top + -db / 60.0f * meterHeight;
        g.drawText (juce::String (db), juce::Rectangle<float> (start, y - labelHeight * .5f, labelWidth, labelHeight),
                    juce::Justification::centredRight, false);
    }
    constexpr int segments = 30;
    const auto step = meterHeight / segments;
    const auto now = juce::Time::getMillisecondCounterHiRes();
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto x = left + channel * (columnWidth + gap);
        g.setColour (juce::Colour (0xff050b06));
        g.fillRect (x - frame, top - frame, columnWidth + frame * 2, meterHeight + frame * 2);
        g.setColour (theme::gold().withAlpha (.7f));
        g.drawRect (x - frame, top - frame, columnWidth + frame * 2, meterHeight + frame * 2, .7f);
        for (int segment = 0; segment < segments; ++segment)
        {
            const auto threshold = -60.0f + segment * 2.0f;
            const auto lit = displayDb[static_cast<std::size_t> (channel)] >= threshold;
            const auto colour = segment >= 28 ? juce::Colour (0xfff56932)
                                              : segment >= 24 ? juce::Colour (0xffe6bd36) : juce::Colour (0xff87c734);
            g.setColour (lit ? colour : juce::Colour (0xff182016));
            g.fillRect (x, bottom - (segment + 1) * step, columnWidth, std::max (.5f, step - std::max (.7f, scale)));
        }
        g.setColour (now < clippedUntil[static_cast<std::size_t> (channel)] ? juce::Colour (0xffff6a3c) : juce::Colour (0xff2a2014));
        g.fillRect (x, top - 3.0f * scale, columnWidth, std::max (1.0f, 1.8f * scale));
        g.setColour (theme::cream().withAlpha (.85f));
        g.drawText (channel == 0 ? "L" : "R", juce::Rectangle<float> (x - 2, area.getHeight() - labelHeight, columnWidth + 4, labelHeight),
                    juce::Justification::centred);
    }
}

RoomScene::RoomScene()
{
    setName ("Approved room photograph and virtual microphone positions");
    setComponentID ("roomScene");
    addAndMakeVisible (drumLayer);
    for (int i = 0; i < 3; ++i) images[static_cast<std::size_t> (i)] = labrc::roomImage (i);
    for (int i = 0; i < 6; ++i)
    {
        const auto pair = i / 2;
        auto& marker = markers[static_cast<std::size_t> (i)];
        marker = std::make_unique<VintageButton> (juce::String (pairNames[pair]) + (i % 2 == 0 ? " L" : " R"), VintageButton::Kind::marker);
        marker->setComponentID ("mic" + juce::String (i));
        marker->setAccent (theme::pairColour (pair));
        marker->setTitle ("Highlight " + marker->getName());
        marker->setTooltip ("Illustrative virtual position. Highlight " + juce::String (pairNames[pair]) + "; does not change audio.");
        marker->onClick = [this, pair]
        {
            const auto next = highlightedPair == pair ? -1 : pair;
            setHighlightedPair (next);
            if (onHighlight) onHighlight (next);
        };
        addAndMakeVisible (*marker);
    }
}
void RoomScene::setRoom (int index)
{
    index = juce::jlimit (0, 2, index);
    if (room != index) { room = index; resized(); repaint(); }
}
void RoomScene::setMicsVisible (bool visible)
{
    if (mics == visible) return;
    mics = visible;
    for (auto& marker : markers) marker->setVisible (visible);
}
void RoomScene::setPairState (int pair, bool active)
{
    markers[static_cast<std::size_t> (pair * 2)]->setRouteActive (active);
    markers[static_cast<std::size_t> (pair * 2 + 1)]->setRouteActive (active);
}
void RoomScene::setHighlightedPair (int pair)
{
    highlightedPair = pair;
    for (int i = 0; i < 6; ++i)
        markers[static_cast<std::size_t> (i)]->setToggleState (pair == i / 2, juce::dontSendNotification);
}
void RoomScene::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0e140e));
    const auto& image = images[static_cast<std::size_t> (room)];
    if (image.isValid())
    {
        const auto photo = fitContain ({ 0, 0, static_cast<double> (getWidth()), static_cast<double> (getHeight()) }, image.getWidth(), image.getHeight());
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (image, juce::Rectangle<float> (static_cast<float> (photo.x), static_cast<float> (photo.y),
                                                  static_cast<float> (photo.width), static_cast<float> (photo.height)),
                     juce::RectanglePlacement::stretchToFit, false);
    }
}
void RoomScene::resized()
{
    const auto& image = images[static_cast<std::size_t> (room)];
    const auto photo = fitContain ({ 0, 0, static_cast<double> (getWidth()), static_cast<double> (getHeight()) }, image.getWidth(), image.getHeight());
    drumLayer.setBounds (getLocalBounds());
    drumLayer.setScene (room, photo);
    const auto scale = std::max (.55, getHeight() / 350.0);
    for (int i = 0; i < 6; ++i)
    {
        const auto p = markerPoint (room, i, photo);
        const auto mate = markerPoint (room, i % 2 == 0 ? i + 1 : i - 1, photo);
        const auto desiredWidth = i < 2 ? std::min (102 * scale, std::abs (mate.x - p.x) * .94)
                                       : std::max (62.0, 102 * scale);
        const auto maximumWidth = 2.0 * std::max (0.0, std::min (p.x - photo.x, photo.right() - p.x)) - 4.0;
        const auto w = std::max (1, static_cast<int> (std::floor (std::min (desiredWidth, maximumWidth))));
        const auto h = std::max (26, juce::roundToInt (30 * scale));
        markers[static_cast<std::size_t> (i)]->setBounds (juce::roundToInt (p.x) - w / 2, juce::roundToInt (p.y) - h / 2, w, h);
    }
}

PairStrip::PairStrip (LabRCProcessor& p, int index)
    : processor (p), pair (index), gain (p, juce::String (pairIDs[index]) + "Db", juce::String (pairNames[index]) + " gain", true),
      mute ("M", VintageButton::Kind::mute), solo ("S", VintageButton::Kind::solo), eqIn ("EQ IN")
{
    setComponentID (juce::String (pairIDs[index]) + "Strip");
    setName (juce::String (pairNames[index]) + " microphone pair");
    for (auto* button : { &mute, &solo })
    {
        const auto isMute = button == &mute;
        const auto id = juce::String (pairIDs[pair]) + (isMute ? "Mute" : "Solo");
        button->setComponentID (id);
        button->setTitle ((isMute ? "Mute " : "Solo ") + juce::String (pairNames[pair]));
        button->setTooltip ((isMute ? "Mute " : "Solo ") + juce::String (pairNames[pair])
                            + (isMute ? ". Mute overrides solo." : ". Multiple pairs can be soloed."));
        button->onClick = [this, id]
        {
            const auto value = processor.apvts.getRawParameterValue (id)->load() >= .5f ? 0.0f : 1.0f;
            processor.beginParameterGesture (id, juce::String (pairNames[pair]) + (id.endsWith ("Mute") ? " mute" : " solo"));
            processor.setParameterFromUser (id, value);
            processor.endParameterGesture (id);
        };
        addAndMakeVisible (*button);
    }
    for (int band = 0; band < 3; ++band)
    {
        const auto id = juce::String (pairIDs[index]) + "Eq" + bandIDs[band] + "Db";
        eq[static_cast<std::size_t> (band)] = std::make_unique<ParameterControl> (p, id, juce::String (pairNames[index]) + " " + bandNames[band] + " EQ", false);
        addAndMakeVisible (*eq[static_cast<std::size_t> (band)]);
    }
    const auto enabledID = juce::String (pairIDs[index]) + "EqEnabled";
    eqIn.setComponentID (enabledID);
    eqIn.setTitle (juce::String (pairNames[index]) + " EQ in");
    eqIn.setTooltip ("Enable " + juce::String (pairNames[index]) + " LOW / MID / HIGH EQ. Gains remain stored when EQ is out.");
    eqIn.onClick = [this, enabledID]
    {
        processor.beginParameterGesture (enabledID, juce::String (pairNames[pair]) + " EQ in");
        processor.setParameterFromUser (enabledID, processor.apvts.getRawParameterValue (enabledID)->load() >= .5f ? 0.0f : 1.0f);
        processor.endParameterGesture (enabledID);
        refreshFromParameters();
    };
    addAndMakeVisible (eqIn);
    addAndMakeVisible (gain);
    addAndMakeVisible (meter);
    meter.setName (juce::String (pairNames[index]) + " stereo peak meter");
    refreshFromParameters();
}
void PairStrip::refreshFromParameters()
{
    gain.refreshFromParameter();
    for (auto& control : eq) control->refreshFromParameter();
    const auto enabled = processor.apvts.getRawParameterValue (juce::String (pairIDs[pair]) + "EqEnabled")->load() >= .5f;
    eqIn.setToggleState (enabled, juce::dontSendNotification);
}
void PairStrip::setPresentation (bool muted, bool soloed, bool active, bool bypass, bool highlight)
{
    mute.setToggleState (muted, juce::dontSendNotification);
    solo.setToggleState (soloed, juce::dontSendNotification);
    if (routeActive != active || bypassed != bypass || highlighted != highlight)
    {
        routeActive = active; bypassed = bypass; highlighted = highlight;
        repaint();
    }
}
void PairStrip::resized()
{
    plateCache = {};
    const auto geometry = PairGeometry::forSize (getWidth(), getHeight());
    const auto rect = [] (Rect r)
    { return juce::Rectangle<float> (static_cast<float> (r.x), static_cast<float> (r.y), static_cast<float> (r.width), static_cast<float> (r.height)).toNearestInt(); };
    gain.setBounds (getLocalBounds());
    gain.layout (rect (geometry.fader), rect (geometry.gainReadout));
    for (int band = 0; band < 3; ++band)
    {
        auto& control = *eq[static_cast<std::size_t> (band)];
        control.setBounds (getLocalBounds());
        control.layout (rect (geometry.eqKnobs[static_cast<std::size_t> (band)]), rect (geometry.eqReadouts[static_cast<std::size_t> (band)]));
    }
    mute.setBounds (rect (geometry.mute)); solo.setBounds (rect (geometry.solo));
    eqIn.setBounds (rect (geometry.eqIn)); meter.setBounds (rect (geometry.meter));
}
void PairStrip::paint (juce::Graphics& g)
{
    const auto scale = getHeight() / 370.0f;
    const auto geometry = PairGeometry::forSize (getWidth(), getHeight());
    const auto rect = [] (Rect r)
    { return juce::Rectangle<float> (static_cast<float> (r.x), static_cast<float> (r.y), static_cast<float> (r.width), static_cast<float> (r.height)); };
    const auto pixels = std::max (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto pixelWidth = juce::roundToInt (getWidth() * pixels), pixelHeight = juce::roundToInt (getHeight() * pixels);
    if (plateCache.getWidth() != pixelWidth || plateCache.getHeight() != pixelHeight || plateScale != pixels)
    {
        plateScale = pixels;
        plateCache = juce::Image (juce::Image::ARGB, pixelWidth, pixelHeight, true);
        juce::Graphics plate (plateCache);
        plate.addTransform (juce::AffineTransform::scale (pixels));
        theme::panel (plate, boundsOf (*this).reduced (1), scale);
        theme::lettering (plate, pairNames[pair], rect (geometry.title), std::max (20.0f, 34 * scale), true, true, juce::Justification::centredLeft);
        auto description = rect (geometry.description);
        theme::lettering (plate, pairDescriptions[pair], description.removeFromTop (description.getHeight() * .55f), std::max (10.0f, 13 * scale), false, false, juce::Justification::centredLeft);
        theme::lettering (plate, "STEREO PAIR", description, std::max (10.0f, 12 * scale), false, false, juce::Justification::centredLeft);
        for (int band = 0; band < 3; ++band)
            theme::lettering (plate, bandNames[band], rect (geometry.eqLabels[static_cast<std::size_t> (band)]), std::max (11.0f, 14 * scale), false, true);
        plate.setColour (theme::gold().withAlpha (.28f));
        const auto separator = getWidth() * 162.0f / 503;
        plate.drawLine (separator, 24 * scale, separator, getHeight() - 25 * scale, .6f);
    }
    g.drawImage (plateCache, getLocalBounds().toFloat());
    const auto status = rect (geometry.status);
    const auto radius = std::max (3.0f, 5.0f * scale);
    const auto centreX = status.getX() + radius, centreY = status.getCentreY();
    g.setColour (routeActive && ! bypassed ? theme::pairColour (pair).brighter (.20f) : juce::Colour (0xff293728));
    g.fillEllipse (centreX - radius, centreY - radius, radius * 2, radius * 2);
    theme::lettering (g, bypassed ? "BYPASSED" : routeActive ? "ACTIVE" : "SILENCED", status.withTrimmedLeft (radius * 3), std::max (10.0f, 12 * scale), false, false, juce::Justification::centredLeft);
    if (highlighted)
    {
        g.setColour (theme::pairColour (pair).withAlpha (.75f));
        g.drawRoundedRectangle (boundsOf (*this).reduced (6 * scale), 5 * scale, 1.3f);
    }
}
}
