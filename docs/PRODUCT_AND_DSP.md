# Product and DSP specification

Status: agreed design and reported implementation as of 2026-11-26. Source code and build logs are not yet in this repository, so the specifications below are acceptance requirements, not an independent inspection of the C++ implementation.

## Product philosophy

Select the room and mix its microphone captures. Do not change the physical identity of the room with generic decay, size, damping or diffusion controls. Designed as a vintage microphone mixing console rather than a conventional reverb.

## Architecture

C++ / JUCE / CMake. Offline AcousticLab generates the assets, runtime partitioned convolution reproduces them. Mono and stereo input support to be checked against actual supported bus layouts. Each EARS, FRONT and FLOOR stereo pair has a separate 2×2 convolution matrix, followed by linked stereo EQ, fader, mute and solo. Sum the pairs, combine with dry path using MIX and apply output trim. Preserve physical propagation delays; compensate only computational latency. No normalization or gain changes that break the approved acoustic reference. DSP must operate offline.

| Control | Agreed initial specification |
|---|---|
| ROOM | Kentucky / Center Field / Live Room |
| INPUT / OUTPUT | -18 to +18 dB, initial 0 dB |
| EARS / FRONT / FLOOR | -60 to +6 dB, proposed initial -6 / -3 / -9 dB |
| MUTE / SOLO | Per pair |
| MIX | 0–100%, proposed default 100% |
| BYPASS | Global, initially off |
| MICS ON/OFF | Graphic visibility only |

Actual defaults and parameter IDs must be checked against the current source. Do not change existing IDs or preset/session compatibility.

## Final Touch EQ

Three independent stereo-linked sections, after convolution and before pair fader. Each has one knob per fixed frequency, center detent at 0 dB and EQ IN (off by default for compatibility). Musical, broad filters inspired by vintage console topology, not an electronic emulation of a named console.

| Band | Type | Frequency | Range | Q |
|---|---|---:|---:|---:|
| LOW | Low shelf | 100 Hz | ±12 dB | implementation-specific shelf slope |
| MID | Bell / peaking | 1 kHz | ±12 dB | 0.7 |
| HIGH | High shelf | 8 kHz | ±12 dB | implementation-specific shelf slope |

IIR biquads; no oversampling required. Smooth changes (20 ms was proposed; verify actual implementation). Avoid unstable coefficient transitions and allocations/locks on the audio thread. EQ OFF must preserve the pre-EQ output; verify using a matched null test. Test both boost and cut and each pair independently. A user request to confirm the shelf/bell/shelf topology was made on 2026-11-26; source-level confirmation remains to be archived.

## Visual design

Approved vintage industrial aesthetic: aged VITALAB green chassis, worn screws, brushed metal/patina, realistic knobs and 1970s-style faders, cream lettering, independent EQ knobs, working meters. Use the approved empty-room backgrounds and the latest approved BOOST THIS drum-kit transparent PNG as a separate graphic layer. Do not use the early flat UI prototype or the illustrative drum kit in the style reference. No generated replacements without approval. Graphic positions are not a source of acoustic coordinates. HiDPI, proportional resize and saved size. Actual final screenshot and asset hashes to be archived.

## Stability / host requirements

- Stable parameter IDs, automation, state restore, undo/redo and resizing.
- Click-free room, gain, mute/solo, EQ and bypass changes; tail behavior explicitly tested.
- No audio-thread allocation, blocking or file I/O; safe IR load/swap.
- Sample-rate adaptation, variable block sizes, multiple instances; finite output and no unexpected clipping.
- Report measured CPU, memory and algorithmic latency.
- AU universal macOS and native Windows x86_64 VST3 targeted; actual minimum OS, architectures and supported host configurations must come from build metadata.
- No changes to acoustic assets when porting.

## Frozen scope

No decay, room size, predelay, damping, diffusion or movable microphones in 1.0. Possible v2 research: ALBINI fixed-position mode and FREE PLACEMENT via precomputed positions, time-aware interpolation, or dynamic early reflections plus late convolution. Not a release commitment.
