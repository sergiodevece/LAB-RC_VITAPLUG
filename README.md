# Manza’s Rooms & Chambers — A Tribute to Steve Albini

**LAB-RC · VITALAB / VitaPlug**

*Technology built from real human experience — Tecnología creada desde la experiencia humana real.*

An independent virtual acoustic recording environment inspired by Electrical Audio, combining computational acoustics, true-stereo convolution and vintage microphone mixing.

Developed by **Sergio Devece (SerxDev) & Compañero (ChatGPT)** at VITALAB, with AI-assisted implementation using Codex.

## A personal tribute / Un homenaje personal

LAB-RC began as a gift for Manza, a drummer and friend. The idea was to recreate the experience of mixing drum room microphones, rather than designing another conventional reverb. It is a tribute to Steve Albini's approach to recording and to the value of the physical recording space.

**The relative microphone placements are based on the personal notes Sergio Devece took during Steve Albini's seminar at La Fabrique, France, in February 2018, organized by Mix With The Masters.** These notes, his professional production experience and computational acoustic research form the foundation of this project.

Las posiciones microfónicas relativas a la batería parten de los apuntes personales que Sergio Devece (SerxDev), creador del plugin junto a Compañero, tomó durante el seminario impartido por Steve Albini en La Fabrique, en febrero de 2018, organizado por Mix With The Masters. Este proyecto nace del cariño, la admiración y el deseo de trasladar aquellas enseñanzas a una herramienta de producción musical.

The additional coordinates and acoustic parameters required by the simulation are engineering estimates. No original measured Electrical Audio impulse responses were used. This independent tribute is not affiliated with or endorsed by Electrical Audio, Mix With The Masters or the estate of Steve Albini.

## Three rooms, six virtual microphones

| Room | Studio | Intended acoustic character |
|---|---|---|
| Kentucky | A | Compact, bright, energetic; experimental coupled lower chamber |
| Center Field | A | Larger, balanced and enveloping |
| Live Room | B | Tall, spacious, reverberant |

Each room has three independent stereo microphone perspectives:

- **EARS:** ribbon microphones.
- **FRONT:** large-diaphragm condensers.
- **FLOOR:** small-diaphragm condensers near the floor.

Choose the room, blend the microphones and shape each pair with a simple vintage-inspired three-band EQ (100 Hz low shelf, 1 kHz bell, 8 kHz high shelf; ±12 dB). No generic decay, room size or predelay knobs: the concept is to mix the capture, not redesign the room.

## Engineering

Offline AcousticLab prototypes developed in Python combine irregular room geometry, multiband early reflections, FDN late reverberation and experimental acoustic coupling. The C++ / JUCE plugin uses precomputed true-stereo convolution matrices and independent stereo-linked EQ. The v0.5 synthetic room IRs were approved by human listening in Logic Pro; the v0.6 matrix approach was reported to preserve the mono reference.

## Status — 26 November 2026

| Milestone | Status |
|---|---|
| Three synthetic room IRs | Listening approved |
| macOS AU | Working in Logic Pro; user reports satisfactory sound |
| Vintage UI and per-pair EQ | Implemented; final assets and regression evidence to archive |
| Windows x64 VST3 | Build requested; physical validation pending |
| Public release | Not authorized; rights and final QA pending |

This repository is initially a documentation baseline. The exact implementation source, original AcousticLab artifacts, binary packages and raw test logs must still be imported from the development workspace. No public release or software license is established here.

## Documentation

- [Acoustic engineering and microphone provenance](docs/ACOUSTIC_ENGINEERING.md)
- [Product and DSP specification](docs/PRODUCT_AND_DSP.md)
- [Development history, validation and release gates](docs/DEVELOPMENT_AND_VALIDATION.md)

The documentation distinguishes published room descriptions, model assumptions, reported automated tests and human listening. It does not claim exact acoustic equivalence with the real rooms.

**VITALAB · Made for you, not for everyone.**
