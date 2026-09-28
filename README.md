# Algorithmic Generative Psytrance & Psybient MIDI Sequencer

An advanced, hardware-driven **algorithmic generative MIDI sequencer** built for the ESP32 and Cheap Yellow Display (CYD) clone boards. This engine dynamically generates evolving musical compositions (melodies, basslines, drum structures, and complex accents) using real-time **Markov chains**, **fractal noise parameters**, macro arrangement tracking, and dynamic scale lookups. 

It features native support for automated **Korg Electribe** pattern switching or dual-engine **XFM synthesizer** sound morphing with intelligent voice-stealing prevention.

---

## 🚀 Key Features

* **Generative Music Engine:** Real-time algorithmic generation of Goa/Psytrance motives, variable note durations, call-and-response phrasings, and dynamic arpeggios.
* **Micro-Evolutionary Engine (`processMicroEvolution`):** Automatically swaps single instrument voices mid-phrase and applies conditional structural breaks, applying random track mutes based on probability arrays every 4 bars.
* **Macro Arrangement Control (`runPsybientEngine`):** Operates on multi-bar structural windows to dynamically cycle through artist styles, shift global key transpositions, adjust chord maps via Markov states, and alter melody behavior.
* **Live Automation Injections:** Generates continuous parameter drift (sending continuous `CC 74` and `CC 83` values) via an emulated **Fractal Noise generator** to control filter cutoffs and patch behavior seamlessly over time.
* **Diatonic Call-and-Response Logic:** Intelligent melodic solvers calculate directional vectors to avoid erratic note behavior, applying gravitational pulls to stable structural intervals (Root, 3rd, 5th) at the tails of phrases.
* **Multi-Scale Musicality:** Integrated structural scale patterns stored in Flash memory, including *Natural Minor, Major, Dorian, Synthetic Clusters, Aeolian Dark Landscapes*, and traditional *Phrygian Dominant Goa Maps*.

---

## 🛠️ Hardware & Pin Configuration

Optimized for **ESP32 "Cheap Yellow Display" (CYD)** clone boards using a custom hardware serial allocation to bypass standard conflicts:

| Function | ESP32 Pin | Note |
| :--- | :--- | :--- |
| **MIDI RX** | `Pin 25` | Hardware Serial 2 Input |
| **MIDI TX** | `Pin 32` | Hardware Serial 2 Output |
| **Random Seed Source** | `Pin 35` | Floating Analog Input |
| **SPI TFT Display** | Standard CYD Pins | Driven via `TFT_eSPI` |

---

## 💾 Installation & Dependencies

### 1. Required Libraries
Ensure you have the following libraries installed in your Arduino IDE or PlatformIO project:
* `Arduino.h` (Core ESP32 Framework)
* `TFT_eSPI` (For display support)
* `SPI.h` (Display data bus)

### 2. Compilation Flags
Modify the behavior of the engine by defining flags at the top of your main codebase or within your `platformio.ini`:

```cpp
#define DISPLAY_ENABLED  // Uncomment to enable the visual UI on your CYD screen
#define SYNTH_XFM        // Uncomment to compile for XFM Synths instead of the Korg Electribe
```

---

## 🎹 MIDI Implementation Details

### Channel Mapping Layouts

[Standard Hardware Matrix Layout]
Channels 01 - 04 ───► Synth Modules
Channels 05 - 08 ───► Bass Engines
Channels 09 - 12 ───► Drums / Percussion
Channels 13 - 16 ───► Closed & Open Hi-Hats
[SYNTH_XFM Optimized Mapping Layout]
Channel 01 ───► Drums Channel 02 ───► Bass
Channel 03 ───► Hats Channel 04 ───► Synth (Dual Patch Control via CC 29/31)

* **`CC 29` & `CC 31`**: XFM Dual Synth Engine Patch Selectors
* **`CC 74`**: Synth Filter Cutoff (Driven by Fractal Noise + Bar Drift)
* **`CC 83`**: Bass Modulation / Filter Contour (Driven by Fractal Noise + Bar Drift)
* **`CC 123`**: All Notes Off (Failsafe Note Killer)

## 💻 Core Engine Structure & Scheduling

[Hardware Clock Tick]
│
▼
┌───────────────────┐
│ runPsybientEngine │ ◄── Calculates step variables (Base, Double, Quad Bars)
└─────────┬─────────┘
│
┌─────────┴─────────┐
│ Step Check? │
└────┬─────────┬────┘
│ │
[Quad-Bar Tail] [Step Quad == 0]
│ │
▼ ▼
┌───────────────────────┐ ┌────────────────────┐
│ processMicroEvolution │ │ Markov Generation │ ──► Steps smoothly through chords
└────────────┬──────────┘ └────────────────────┘
│
┌─────────┴─────────┐
│ Check Bar Counts │ ──► Triggers 'transitionToNextArtist()' when threshold met
└───────────────────┘

### Functional Component Deep Dive

* **`runPsybientEngine()`**
  The core system scheduling loop. Handles dynamic time-signature tracking (3/3, 4/4, 5/5) and subdivides incoming hardware ticks into structural macro-windows. It manages note cleanup on old channels, evaluates melodic directions, scales probability bounds, and looks up active chord pools dynamically.

* **`processMicroEvolution()`**
  Monitors phase durations via `longBarCounter`. If the threshold matches `styleExpirationBar`, it triggers a global structural shift. Otherwise, it uses `changeRate` probabilities to swap voice channels or generate randomized track mute breaks every 4 bars to drop drum patterns, synth loops, or hat layers.

* **`changeMarkovMatrix(uint8_t mode)`**
  Handles the mathematical matrix profiles. Features static linear profiles ("Glacial Aeolian Cycle") alongside a fully generative "Non-Euclidean" mode. The generative mode samples noise via `analogRead(35)` combined with `micros()` to roll random matrix cell weights, scaling them cleanly into matching 0-100% probability grids matching your time signature bounds.

* **`randomBassChannel()` / `randomSynthChannel()` / `randomHatChannel()` / `randomDrumChannel()`**
  Intelligent voice handoff managers. Before switching to a new randomized hardware channel range, these functions send a proactive `CC 123` (All Notes Off) packet directly to the previous voice channel to instantly kill hanging notes and prevent audio bleeding.

* **`generateFractalNoise()`**
  An emulated Linear Feedback Shift Register (LFSR) optimized for 32-bit environments. Generates fast, organic, pseudo-random integer paths (45 to 85) to emulate subtle human drift or dynamic parameter modulations without tax on the processor.

---

## 🎨 Creative Parameter Indexing

* **`currentArtistStyle` (0-4)**: Dictates the foundational musical flavor by altering scale choices:
  * `0`: C Natural Minor / A Minor / B Minor / G Minor / D Minor pool.
  * `1`: Dorian Scale Maps (Bright/Mystical).
  * `2`: Major Scale Maps.
  * `3`: Abstract / Synthetic Cluster Layouts.
  * `4`: Aeolian Dark Landscapes (Deep, continuous ambient pad modes).
* **`currentMelodyStyle` (0-3)**: Adjusts note selection logic:
  * `0`: Directional Momentum Tracking (Up/Down phrase vector shifts).
  * `1`: Wide Interval Skipped Arpeggiations.
  * `2`: Alternate Step Structures (Bouncing between specific structural intervals).
  * `3`: Static, low-complexity progressions.

---

## 📄 License

This project is open-source software. Feel free to modify, fork, and jam with it responsibly!

