# Acoustic engineering / Ingeniería acústica

Status: project research record, 2026-11-26. This document records the development conversation and the reported prototype results; it is not a substitute for the original simulation code, logs or measurement files.

## Scope

Three acoustically distinct rooms inspired by Electrical Audio: Kentucky and Center Field (Studio A), and Live Room (Studio B). Alcatraz and Dead Room were deliberately excluded. No original measured room impulse responses were used. This is an independent, synthetic acoustic interpretation, not a certified replica or an endorsed product.

## Geometry and provenance

| Room | Floor area | Mean height in model | Main volume in model |
|---|---:|---:|---:|
| Kentucky | ~27.8 m² (estimated) | ~3.35 m (inferred) | ~93 m³ |
| Center Field | ~111.5 m² (published reference) | ~3.5 m (assumed) | ~390 m³ |
| Live Room | ~74.3 m² (published reference) | ~8.07 m (assumed) | ~600 m³ |

Studio B's published ceiling height is approximately 30 ft (9.14 m), not necessarily its mean height. The studio overview plan was digitized into irregular polygons; the two independent area calibrations were reported to differ by approximately 7% linearly. Dimensions, exact angles and volumes remain approximate. Generated UI photographs are not metric evidence. Kentucky's floating floor and acoustically coupled lower space inspired a separate experimental coupled-chamber model. Its exact acoustic parameters are unknown.

References to archive and verify alongside the original research:
- [Electrical Audio — Studio A](https://www.electricalaudio.com/studio-a)
- [Electrical Audio — Studio B](https://www.electricalaudio.com/studio-b)
- [Electrical Audio — Studios Overview](https://www.electricalaudio.com/studios-overview)
- [Recording Hacks — Touring Electrical Audio](https://recordinghacks.com/2008/10/07/touring-electrical-audio/)

Published descriptions, plan-derived estimates and engineering choices must remain separate in the model files.

## Microphone provenance

The relative microphone arrangements are based on the personal notes taken by Sergio Devece (SerxDev), creator of LAB-RC together with Compañero (ChatGPT), during Steve Albini's Mix With The Masters seminar at La Fabrique in February 2018. The following is the project interpretation of those notes, not an official transcription or endorsement.

- EARS: stereo ribbon pair, parallel and facing forward, at 1.25 m above the floor and approximately 0.30 m behind the drummer.
- FRONT: stereo large-diaphragm condenser pair, at 1.25 m above the floor and approximately 0.50 m in front of the kick's front reference line, facing EARS.
- FLOOR: stereo small-diaphragm condenser pair resting on the floor, roughly 2–3 m in front of the kit, arranged in a broad V and aimed toward the kit.

The prototype uses generic polar patterns; exact capsule models, spacing, orientation, capsule height and source geometry are not all known. Additional coordinates below are **simulation assumptions**, not measurements taken at the seminar. Origin: horizontal projection of kick front center, Y forward, Z up.

| Capsule | X (m) | Y (m) | Z (m) |
|---|---:|---:|---:|
| EARS L/R | ±0.65 | -1.30 | 1.25 |
| FRONT L/R | ±0.65 | +0.50 | 1.25 |
| FLOOR L/R | ±1.768 | +1.768 | 0.01 |

The notes also describe close-miking techniques; these are not separate inputs or processing channels in LAB-RC 1.0. A stereo bus cannot be decomposed into independently recorded drums by convolution.

## AcousticLab evolution

| Version | Milestone | Reported result |
|---|---|---|
| v0.2 | Plan digitization and geometry | Three irregular floor polygons |
| v0.3 | First-order image-source reflections | 114 valid reflected paths, 18 receivers, 27 WAV variants |
| v0.4 | Up to third-order reflections, frequency-dependent materials | 1,185 reflected paths, 36 WAV variants |
| v0.5 | Multiband late FDN and coupled Kentucky chamber | 27 WAV variants; nine stereo wet IR auditioned and approved by SerxDev |
| v0.6 | Experimental 2×2 convolution matrices | Nine four-transfer matrices; reported mono regression 9/9 |

The numbers above were reported during prototype development and are pending archival of the executable code and raw logs in this repository.

Model: direct propagation, specular early reflections, distance and microphone directivity, frequency-dependent absorption, minimum-phase reflection filters, multiband FDN late field (16 delay lines per band/volume, Hadamard mixing), and a passive experimental coupled network for Kentucky. The runtime plugin uses precomputed responses; it does not run the geometry solver in real time.

Reflection pressure magnitude per surface: R(f) = sqrt(1 - alpha(f)). Absorption, scattering and isolation are different quantities; the adobe and ceiling parameters are experimental approximations, not measurements of the actual building.

## Synthetic decay targets and reported metrics

FDN design targets (seconds):
| Hz | Kentucky | Center Field | Live Room |
|---:|---:|---:|---:|
| 125 | .85 | 1.55 | 2.35 |
| 250 | .78 | 1.48 | 2.25 |
| 500 | .72 | 1.40 | 2.10 |
| 1000 | .65 | 1.30 | 1.95 |
| 2000 | .57 | 1.10 | 1.60 |
| 4000 | .46 | .85 | 1.20 |

Reported v0.5 full synthetic IR metrics (median of six receivers):
| Metric | Kentucky | Center Field | Live Room |
|---|---:|---:|---:|
| EDT (s) | .73 | 1.51 | 2.14 |
| T20 (s) | .76 | 1.36 | 2.10 |
| T30 (s) | .78 | 1.39 | 2.14 |
| C80 (dB) | +7.56 | +3.20 | -.12 |

These are metrics of synthetic responses, influenced by chosen design targets. They are not measurements of Electrical Audio. The experimental lower chamber reportedly increased Kentucky's 125 Hz FDN T30 from .852 s to 1.018 s before early/late integration.

## True stereo

For a pair of receivers, use four transfer functions, with input-to-output naming explicitly documented:

- yL = hLL * xL + hRL * xR
- yR = hLR * xL + hRR * xR

Here * is convolution; first index is input and second output. Nine matrices contain 36 transfer functions. v0.6 used two provisional source locations 0.45 m either side of center and experimental late-field decorrelation. The mono reconstruction requirement for identical input channels is hLL+hRL = original left IR, hLR+hRR = original right IR. Reported maximum numerical residual was below 4e-9 in amplitude; reproduce this with the original assets, not this rounded report. Preserve actual channel order specified by the asset manifest; do not assume a WAV channel order from this notation.

## Limitations

No calibrated IR or RT60 recordings from the actual rooms; unknown dimensions, surface coefficients and exact microphone polar responses; simplified source positions, obstacles and diffuse field; true-stereo late field is a design approximation. Musical usefulness is validated by listening, not by claiming measured-room fidelity.
