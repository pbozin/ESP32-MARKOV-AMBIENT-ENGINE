#include <Arduino.h>
#ifdef DISPLAY_ENABLED
#include <TFT_eSPI.h> 
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI(); 
#endif


// Hardware Serial Configuration for CYD Clone Board
#define RX_PIN 25 
#define TX_PIN 32 
#define MIDI_SERIAL Serial2

// --- THE 4X4 HARDWARE MATRIX LAYOUT ---
#ifndef SYNTH_XFM
  #define SYNTH_START 1 
  #define SYNTH_END   4 

  #define BASS_START  5 
  #define BASS_END    8 

  #define DRUM_START  9 
  #define DRUM_END    12 

  #define HAT_START   13 
  #define HAT_END     16 
#else
  #define SYNTH_START 4 
  #define SYNTH_END   4 

  #define BASS_START  2 
  #define BASS_END    2 

  #define DRUM_START  1 
  #define DRUM_END    1 

  #define HAT_START   3 
  #define HAT_END     3 
#endif

// --- GLOBAL GOA ENGINE PARAMETERS ---
// 0 = never (0%), 1 = very-rarely (10%), 2 = rarely (25%), 3 = occasionally (50%), 4 = often (85%)
volatile uint8_t goaMotiveChance = 3;

// 0 = none, 1 = very short (1 step), 2 = short (2 steps), 3 = medium (4 steps), 4 = long (infinite / legato)
volatile uint8_t goaMotiveDuration = 2;

// Track the current step-count duration of the active note-on event
uint8_t goaNoteDurationCounter = 0;

// Global Timing & Mix variables
uint16_t currentBpm = 94; 
uint32_t tickCount = 0;
uint32_t longBarCounter = 0; // The validated variable tracker

// State Controllers
uint8_t currentChordIndex = 0;
uint8_t activeSynthChannel = 1;
uint8_t activeBassChannel  = 5;
uint8_t activeDrumChannel = 9;
uint8_t activeHatChannel  = 13;

uint8_t lastArpNotes[4] = {0, 0, 0, 0};
uint8_t lastDroneNotes[4] = {0, 0, 0, 0};

// --- MIX MACHINE MACRO CONTROL FLAGS ---
uint8_t currentArtistStyle = 0;
uint32_t styleExpirationBar = 90;
uint8_t dynamicArpChance = 50;
uint8_t dynamicHatSkip = 4;
bool systemMuteArray[17] = {false};
uint8_t breakrollBase = 30;
uint8_t changeRate = 10;

uint8_t globalDrumDensity = 20;
uint8_t glitchPercussionChance = 30;
uint8_t ghostKickChance = 15;
uint8_t activePanIntensity = 32;

uint8_t lastScalePositionIndex = 0;
bool bassMelodyInherit = false; 
uint8_t activeMelodyBaseNote = 60; 
uint8_t currentMelodyStyle = 0;

// Structural Harmonies
const uint8_t scaleMinor[5][7] = {
    {60, 62, 63, 65, 67, 68, 70}, // C Natural Minor
    {57, 59, 60, 62, 64, 65, 67}, // A Minor
    {59, 61, 62, 64, 66, 67, 69}, // B Minor
    {55, 57, 58, 60, 62, 63, 65}, // G Minor
    {62, 64, 65, 67, 69, 70, 72}  // D Minor
};

const uint8_t scaleMajor[5][7] = {
    {60, 62, 64, 65, 67, 69, 71}, // C Major
    {62, 64, 66, 67, 69, 71, 73}, // D Major
    {65, 67, 69, 70, 72, 74, 76}, // F Major
    {57, 59, 61, 62, 64, 66, 68}, // A Major
    {67, 69, 71, 72, 74, 76, 78}  // G Major
};

const uint8_t scaleDorian[5][7] = {
    {60, 62, 63, 65, 67, 69, 70}, // C Dorian
    {62, 64, 65, 67, 69, 71, 72}, // D Dorian
    {58, 60, 61, 63, 65, 67, 68}, // Bb Dorian
    {55, 57, 58, 60, 62, 64, 65}, // G Dorian
    {64, 66, 67, 69, 71, 73, 74}  // E Dorian
};

const uint8_t scaleAbstract[5][7] = {
    {58, 59, 62, 63, 66, 67, 70}, // Synthetic Cluster 1
    {56, 57, 60, 61, 64, 65, 68}, // Synthetic Cluster 2
    {54, 55, 58, 59, 63, 64, 66}, // ...
    {52, 53, 56, 57, 61, 62, 64}, 
    {50, 51, 54, 55, 58, 59, 62}  
};

const uint8_t scaleGoa[5][7] = {
    {40, 41, 44, 45, 47, 48, 50}, // E Phrygian Dominant Root Map
    {45, 47, 48, 50, 52, 53, 56}, 
    {48, 50, 52, 53, 56, 57, 59}, 
    {40, 44, 47, 48, 50, 52, 53}, 
    {41, 45, 47, 48, 50, 52, 55}  
};

const uint8_t scaleAeolian[5][7] = {
    {36, 38, 39, 41, 43, 44, 46}, // C Aeolian Dark Landscape
    {44, 46, 47, 49, 51, 52, 54}, 
    {41, 43, 44, 46, 48, 49, 51}, 
    {36, 38, 39, 41, 43, 44, 46}, 
    {39, 41, 42, 44, 46, 47, 49}  
};

uint8_t markovMatrix[5][5] = {
    {40, 30, 10, 10, 10}, 
    {20, 30, 30, 10, 10}, 
    {10, 20, 40, 20, 10}, 
    {30, 10, 20, 30, 10}, 
    {10, 20, 30, 20, 20}
};

int8_t globalKeyTransposition = 0;
// --- TIME SIGNATURE CONFIGURATION ---
// 3 = 3/3 (Triple), 4 = 4/4 (Standard), 5 = 5/5 (Quintupal / Odd-meter)
volatile uint8_t currentTimeSignature = 3; 

// Dynamic step tracking variables
uint8_t currentStep        = 0; // The active step within a single bar
uint8_t currentMacroBar    = 0; // The active bar within a long phrase

uint32_t stepInterval = (uint32_t)((60000000ULL / currentBpm) / currentTimeSignature);


void midiMsg(uint8_t cmd, uint8_t d1, uint8_t d2) {
    MIDI_SERIAL.write(cmd);
    MIDI_SERIAL.write(d1);
    MIDI_SERIAL.write(d2);
}

void silenceChannel(int ch) {
    midiMsg(0xB0 | (ch - 1), 123, 0);   
}

void silenceAllChannels() {
    for (uint8_t ch = 1; ch <= 16; ch++) {
        midiMsg(0xB0 | (ch - 1), 123, 0);   
    }
}

#ifdef DISPLAY_ENABLED
void displayValue(uint8_t simpleDisplayValue, uint8_t row) {
      tft.fillRect(145, ((row-1)*40)+10, 80, 40, TFT_BLACK); 
      if (row == 0) {
          tft.drawNumber(simpleDisplayValue, 200, ((row-1)*40)+10, 4); 
      } else if (row == 3) { 
          tft.drawNumber(simpleDisplayValue, 145, ((row-1)*40)+10, 4); 
          tft.drawString("/", 205, ((row-1)*40)+10, 4); 
          tft.drawNumber(currentTimeSignature, 190, ((row-1)*40)+10, 4); 
          tft.drawNumber(currentTimeSignature, 215, ((row-1)*40)+10, 4); 
      
      } else {
          tft.drawNumber(simpleDisplayValue, 145, ((row-1)*40)+10, 4); 
          if (row == 8) {
              tft.drawString("/", 185, ((row-1)*40)+10, 4); 
              tft.drawNumber(styleExpirationBar, 195, ((row-1)*40)+10, 4); 
          }
      }
}
void displayArray(const char* simpleDisplayValue, uint8_t row) {
      tft.drawString(simpleDisplayValue, 5, ((row-1)*40)+10, 4); 
}
#endif


#ifdef SYNTH_XFM
void changeXFMSynths(uint8_t soundA, uint8_t soundB, uint8_t midiChannel) {
  // Synth 1 (Sound A)
  midiMsg(0xB0 | (midiChannel), 29, soundA);

  // Synth 2 (Sound B)
  midiMsg(0xB0 | (midiChannel), 31, soundB);
}
#else
void selectElectribePattern(uint16_t patternIndex) {
    if (patternIndex < 1 || patternIndex > 250) return;

    uint8_t cc0_msb = 0;
    uint8_t cc32_lsb = 0;
    uint8_t program = 0;

    if (patternIndex <= 127) {
        cc0_msb  = 0;
        cc32_lsb = 0;
        program  = patternIndex - 1; // 0-126 mapped directly
    } else {
        cc0_msb  = 0;
        cc32_lsb = 1;                // Shifts memory bank address register to LSB 1
        program  = (patternIndex - 128); // 0-122 mapped for bank 2
    }

    // Broadcast mapping across global layout (Channel 1 handles master sync routing)
    midiMsg(0xB0, 0,  cc0_msb);  // Bank Select MSB (CC 0)
    midiMsg(0xB0, 32, cc32_lsb); // Bank Select LSB (CC 32)
    MIDI_SERIAL.write(0xC0);     // Program Change command
    MIDI_SERIAL.write(program);

#ifdef DISPLAY_ENABLED
    displayValue(patternIndex, 2);
#endif

    Serial.printf("PTRN -> ");
    Serial.println(patternIndex);

    delay(40); // Small execution hold window to allow Electribe's CPU to load the pattern data
}

void randomizeElectribePattern() {
    silenceAllChannels();
    uint16_t targetedPattern = random(1, 5);
    selectElectribePattern(targetedPattern);
}
#endif


uint8_t generateFractalNoise() {
    static uint16_t x = 0xACE1u;
    x ^= x >> 7;
    x ^= x << 9;
    x ^= x >> 13;
    return (x % 40) + 45; 
}



void setChannelVolume(uint8_t channel, uint8_t volume) {
    MIDI_SERIAL.write(0xB0 | (channel - 1));
    MIDI_SERIAL.write(7);                    // CC 7 = Amp Level / Volume
    MIDI_SERIAL.write(volume);               // Value (0-127)
}

// --- CHANNEL HANDOFF INTERCEPTORS (Pure Logic Shuffling - No Volume Intervention) ---
void randomBassChannel() {
    if (activeBassChannel >= BASS_START && activeBassChannel <= BASS_END) {
        midiMsg(0xB0 | (activeBassChannel - 1), 123, 0); // Crucial: Kill hanging notes on old voice
    }
    activeBassChannel = random(BASS_START, BASS_END + 1);
    
#ifdef DISPLAY_ENABLED
    displayValue(activeBassChannel, 5);
#endif

#ifdef SYNTH_XFM
    uint8_t soundA = random(0,16);
    uint8_t soundB = random(0,16);
    uint8_t safeBase = (BASS_START > 0) ? (BASS_START - 1) : 0;
    changeXFMSynths(soundA, soundB, safeBase);
    Serial.printf("BASS -> activeBassChannels: %d/%d\n\r", soundA, soundB);
#else
    Serial.printf("BASS -> activeBassChannel: %d\n\r", activeBassChannel);
#endif

}

void randomSynthChannel() {
    if (activeSynthChannel >= SYNTH_START && activeSynthChannel <= SYNTH_END) {
        midiMsg(0xB0 | (activeSynthChannel - 1), 123, 0); // Crucial: Kill hanging notes on old voice
    }
    activeSynthChannel = random(SYNTH_START, SYNTH_END + 1);
    
#ifdef DISPLAY_ENABLED
    displayValue(activeSynthChannel, 4);
#endif

#ifdef SYNTH_XFM
    uint8_t soundA = random(0,16);
    uint8_t soundB = random(0,16);
    changeXFMSynths(soundA, soundB, SYNTH_START - 1);
    Serial.printf("SYNT -> activeSynthChannels: %d/%d\n\r", soundA, soundB);
#else
    Serial.printf("SYNT -> activeSynthChannel: %d\n\r", activeSynthChannel);
#endif
}

void randomHatChannel() {
    if (activeHatChannel >= HAT_START && activeHatChannel <= HAT_END) {
        midiMsg(0xB0 | (activeHatChannel - 1), 123, 0); // Crucial: Kill hanging notes on old voice
    }
    activeHatChannel = random(HAT_START, HAT_END + 1);
    
#ifdef DISPLAY_ENABLED
    displayValue(activeHatChannel, 7);
#endif

#ifdef SYNTH_XFM
    uint8_t soundA = random(0,16);
    uint8_t soundB = random(0,16);
    changeXFMSynths(soundA, soundB, HAT_START - 1);
    Serial.printf("SYNT -> activeHatChannels: %d/%d\n\r", soundA, soundB);
#else
    Serial.printf("HATS -> activeHatChannel: %d\n\r", activeHatChannel);
#endif
}

void randomDrumChannel() {
    if (activeDrumChannel >= DRUM_START && activeDrumChannel <= DRUM_END) {
        midiMsg(0xB0 | (activeDrumChannel - 1), 123, 0); // Crucial: Kill hanging notes on old voice
    }
    activeDrumChannel = random(DRUM_START, DRUM_END + 1);

#ifdef DISPLAY_ENABLED
    displayValue(activeDrumChannel, 6);
#endif
    
#ifdef SYNTH_XFM
    uint8_t soundA = random(0,16);
    uint8_t soundB = random(0,16);
    changeXFMSynths(soundA, soundB, DRUM_START - 1);
    Serial.printf("SYNT -> activeDrumChannels: %d/%d\n\r", soundA, soundB);
#else
    Serial.printf("DRUM -> activeDrumChannel: %d\n\r", activeDrumChannel);
#endif
}

void changeMarkovMatrix(uint8_t mode) {
    switch (mode) {
        case 0:
        case 1:
        case 5: // ONE ARC DEGREE 5/5 EXCLUSIVE (Melancholic Infinite Cycle)
            // Transitions step smoothly through all 5 chord zones linearly
            markovMatrix[0][0] = 10; markovMatrix[0][1] = 80; markovMatrix[0][2] = 10; markovMatrix[0][3] = 0;  markovMatrix[0][4] = 0;
            markovMatrix[1][0] = 0;  markovMatrix[1][1] = 10; markovMatrix[1][2] = 80; markovMatrix[1][3] = 10; markovMatrix[1][4] = 0;
            markovMatrix[2][0] = 0;  markovMatrix[2][1] = 0;  markovMatrix[2][2] = 10; markovMatrix[2][3] = 80; markovMatrix[2][4] = 10;
            markovMatrix[3][0] = 10; markovMatrix[3][1] = 0;  markovMatrix[3][2] = 0;  markovMatrix[3][3] = 10; markovMatrix[3][4] = 80;
            markovMatrix[4][0] = 80; markovMatrix[4][1] = 0;  markovMatrix[4][2] = 10; markovMatrix[4][3] = 0;  markovMatrix[4][4] = 10;
            Serial.println("Markov Mood -> 5/5 Glacial Aeolian Cycle Loaded");
            break;

        case 2: { // THE MATHEMATICAL MULTIVERSE (Scalable Non-Human Generator)
            randomSeed(analogRead(35) + micros());
            
            // Explicitly clip dynamic size loop boundaries to prevent stack overflow crashes
            uint8_t targetGridSize = currentTimeSignature;
            if (targetGridSize > 5) targetGridSize = 5; 
            
            for (int i = 0; i < targetGridSize; i++) {
                long rowTotal = 0;
                long rawWeights[5]; // Safe fixed room for up to 5x5 grids
                
                // 1. Roll safe mathematical weights
                for (int j = 0; j < targetGridSize; j++) {
                    rawWeights[j] = random(10, 100);
                    rowTotal += rawWeights[j];
                }
                
                // 2. Scale values cleanly into exact percentages
                int assignedSum = 0;
                for (int j = 0; j < targetGridSize - 1; j++) {
                    markovMatrix[i][j] = (rawWeights[j] * 100) / rowTotal;
                    assignedSum += markovMatrix[i][j];
                }
                
                // 3. Absolute mathematical correction on the final structural cell
                markovMatrix[i][targetGridSize - 1] = 100 - assignedSum;
            }

            Serial.print("Markov Mood -> Non-Euclidean Grid Loaded for size: ");
            Serial.print(currentTimeSignature);
            Serial.println("x");
            Serial.print(currentTimeSignature);
            break;
        }

        default: // Failsafe fallback mapping
            changeMarkovMatrix(2);
            break;
    }
}

void transitionToNextArtist() {
    Serial.println("------------------------------");
    silenceAllChannels();
#ifndef SYNTH_XFM
    randomizeElectribePattern();
#endif
    uint8_t nextStyle = (currentArtistStyle + random(1, 5)) % 5;
    currentArtistStyle = nextStyle;
    uint8_t currentMarkov = random(0,3);
    if (currentMarkov > 2)
        currentMarkov = 2;
    changeMarkovMatrix(currentMarkov);

    silenceAllChannels();

    switch (currentArtistStyle) {
        case 0: // AES DANA MODE: Deep, dark, slow, cinematic breathing spaces
            currentTimeSignature = random(3,6);
            currentBpm = random(90, 100);
            dynamicArpChance = 40;       
            dynamicHatSkip = 6;          
            globalDrumDensity = 20;      
            breakrollBase = random(15, 30);
            ghostKickChance = 0;         
            activePanIntensity = 25;
            styleExpirationBar = random(300, 400);
            changeRate = random(5, 10);
            goaMotiveChance = random(0,2);
            goaMotiveDuration = random(0,2); 
            break;

        case 1: // SOLAR FIELDS MODE: Cinematic, liquid, floating space-glides
            currentTimeSignature = random(3,6);
            currentBpm = random(78, 80);
            dynamicArpChance = 65;
            dynamicHatSkip = 3;          
            globalDrumDensity = 30;
            breakrollBase = random(10, 25); 
            ghostKickChance = 10;        
            activePanIntensity = 45;
            styleExpirationBar = random(300, 400);
            changeRate = random(10, 20);
            goaMotiveChance = random(0,2);
            goaMotiveDuration = random(0,2); 
            break;

        case 2: // CARBON BASED LIFEFORMS MODE: Warm, sun-drenched, acid-bubble textures
            currentTimeSignature = random(3,6);
            currentBpm = random(80, 100);
            dynamicArpChance = 55;
            dynamicHatSkip = 2;          
            globalDrumDensity = 35;
            breakrollBase = random(10, 25); 
            ghostKickChance = 15;        
            activePanIntensity = 30;
            styleExpirationBar = random(300, 400);
            changeRate = random(5, 10);
            goaMotiveChance = random(0,3);
            goaMotiveDuration = random(0,3); 
            break;

        case 3: // THE FUTURE SOUND OF LONDON MODE: Abstract, slow, psychedelic industrial drones
            currentTimeSignature = random(3,6);
            currentBpm = random(80, 90); 
            dynamicArpChance = 35;       
            dynamicHatSkip = 8;          
            globalDrumDensity = 15;      
            breakrollBase = random(20, 30);
            ghostKickChance = 5;
            activePanIntensity = 64;     
            styleExpirationBar = random(300, 400);
            changeRate = random(10, 20);
            goaMotiveChance = random(0,4);
            goaMotiveDuration = random(0,4); 
            break;
	default:
        case 4: // ONE ARC DEGREE MODE: Glacial, deep, cinematic melancholia
            currentTimeSignature = 5;    // --- FORCED ODD-METER 5/5 ATMOSPHERE ---
            currentBpm = random(60, 68); // Glacial slow cinematic drift
            dynamicArpChance = 20;       
            dynamicHatSkip = 8;          
            globalDrumDensity = 0;       // Silences all standard heavy drum hits
            breakrollBase = 0;           
            ghostKickChance = 5;         
            activePanIntensity = 64;     
            styleExpirationBar = random(300, 400); 
            changeRate = 4;              
            goaMotiveChance = 0;         // Clean cinematic focus
            goaMotiveDuration = 4;       // Long sustained overlapping legato lines
	    currentMarkov = 5;
            changeMarkovMatrix(currentMarkov);       // Loads the custom 5x5 Melancholic loop profile
            break;

    }

#ifdef DISPLAY_ENABLED
    displayValue(currentArtistStyle, 3);
    displayValue(currentBpm, 1);
#endif

    Serial.printf("Artist Style: %d | Time Signature: %d/%d | BPM: %d\n\r", 
                  currentArtistStyle, currentTimeSignature, currentTimeSignature, currentBpm);
    Serial.printf("Markov Matrix: %d\n\r", currentMarkov);

    // Dynamic initial channel shuffles
    randomDrumChannel();
    randomHatChannel();
    randomSynthChannel();
    randomBassChannel();

    for (int i = 0; i <= 16; i++) systemMuteArray[i] = false;
    stepInterval = (uint32_t)((60000000ULL / currentBpm) / currentTimeSignature);
}

void processMicroEvolution() {
    if (longBarCounter >= styleExpirationBar) {
        longBarCounter = 0;
        transitionToNextArtist();
        return;
    }

    // Mid-phrase single voice morph triggers
    if (random(0, 100) < changeRate) { randomSynthChannel(); return; }
    if (random(0, 100) < changeRate) { randomHatChannel(); return; }
    if (random(0, 100) < changeRate) { randomBassChannel(); return; }
    if (random(0, 100) < changeRate) { randomDrumChannel(); return; }

    // Structural Mute Breaks
    if (longBarCounter % 4 == 0) {
        uint8_t breakRoll = random(0, 100);

        if (breakRoll < breakrollBase) {
            systemMuteArray[activeDrumChannel] = true;  
            systemMuteArray[activeSynthChannel] = false;
        } else if (breakRoll >= breakrollBase && breakRoll < (2 * breakrollBase)) {
            systemMuteArray[activeDrumChannel] = false;
            systemMuteArray[activeSynthChannel] = true; 
        } else if (breakRoll >= (2 * breakrollBase) && breakRoll < (3 * breakrollBase)) {
            systemMuteArray[activeHatChannel] = true;   
        } else {
            systemMuteArray[activeDrumChannel] = false;
            systemMuteArray[activeHatChannel] = false;
            systemMuteArray[activeSynthChannel] = false;
        }
    }
#ifdef DISPLAY_ENABLED
    displayValue(longBarCounter, 8);
#endif
    Serial.printf("longBarCounter: %d\n\r", longBarCounter);
}

void runPsybientEngine() {
    // --- DYNAMIC TIME SIGNATURE RESOLUTION ---
    uint8_t beatsPerBar = currentTimeSignature;
    uint8_t doubleBar   = beatsPerBar * 2;
    uint8_t quadBar     = beatsPerBar * 4;

    uint8_t stepBase   = tickCount % beatsPerBar;
    uint8_t stepDouble = tickCount % doubleBar;
    uint8_t stepQuad   = tickCount % quadBar;

    // --- MULTI-BAR MACRO REWRITE WINDOW ---
    if (stepQuad == (quadBar - 1)) {
        processMicroEvolution();

        if (longBarCounter % 16 == 0) {
            bassMelodyInherit = (random(0, 100) < 30);
            currentMelodyStyle = random(0, 4);
        }

        if (longBarCounter % 32 == 0) {
            uint8_t keyRoll = random(0, 100);
            if (keyRoll < 50)       globalKeyTransposition = 0;
            else if (keyRoll < 75)  globalKeyTransposition = 5;
            else                    globalKeyTransposition = 7;
        }
    }

    // --- MARKOV GENERATOR STEP ---
    if (stepQuad == 0) {
        uint8_t roll = random(0, 100); uint8_t sum = 0;
        for (uint8_t target = 0; target < currentTimeSignature; target++) {
            sum += markovMatrix[currentChordIndex][target];
            if (roll <= sum) { currentChordIndex = target; break; }
        }
    }

    // --- AUTOMATION TRACK ---
    if (tickCount % (beatsPerBar == 4 ? 4 : 3) == 0) { 
        uint8_t ccValue = generateFractalNoise();
        uint8_t longWaveDrift = (longBarCounter % 128) / 4;
        midiMsg(0xB0 | (activeSynthChannel - 1), 74, ccValue + 5 + longWaveDrift);
        midiMsg(0xB0 | (activeBassChannel - 1),  83, ccValue - 20 + (longWaveDrift / 2));
    }

    // --- EVOLVING MULTI-STYLE MELODIC ENGINE ---
    uint8_t synthIdx = activeSynthChannel - SYNTH_START;

    if (!systemMuteArray[activeSynthChannel]) {
        // Map Chance Value (0-4) to actual percentage probability
        uint8_t actualGoaPercent = 0;
        if (goaMotiveChance == 1)      actualGoaPercent = 10;
        else if (goaMotiveChance == 2) actualGoaPercent = 25;
        else if (goaMotiveChance == 3) actualGoaPercent = 50;
        else if (goaMotiveChance == 4) actualGoaPercent = 85;

        bool triggerGoaMotive = (random(0, 100) < actualGoaPercent);

        uint8_t dynamicTriggerChance = dynamicArpChance;
        if (currentMelodyStyle == 2)      dynamicTriggerChance = dynamicArpChance + 15;
        else if (currentMelodyStyle == 3) dynamicTriggerChance = 25;

        if (stepQuad >= doubleBar && currentMelodyStyle != 2) {
            dynamicTriggerChance = dynamicTriggerChance / 3;
        }

        if (currentArtistStyle == 4) {
            if (stepBase != 0) {
                dynamicTriggerChance = 0; // Ambient pad hold intercept
            } else {
                dynamicTriggerChance = 95; 
            }
        }

        // --- EVALUATE NOTE TRIGGER EVENT ---
        if (random(0, 100) < dynamicTriggerChance) {
            
            // Clean up previous active note immediately before playing a new one
            if (lastArpNotes[synthIdx] > 0) {
                midiMsg(0x80 | (activeSynthChannel - 1), lastArpNotes[synthIdx], 0);
                lastArpNotes[synthIdx] = 0;
                goaNoteDurationCounter = 0;
            }

            // --- DEEPER DIATONIC MELODY PATH GENERATION (0-6 INDEX POOL) ---
            if (triggerGoaMotive) {
                if (stepBase % 2 == 0) {
                    lastScalePositionIndex = 0; // Low tension root anchor point
                } else {
                    lastScalePositionIndex = random(0, 7); // Open up to full 7-step chaotic Goa scale climbs
                }
            } else {
                // Melodic Call-and-Response Structural Resolution
                bool phraseTailResolution = (stepQuad >= (quadBar - beatsPerBar)); // Is this the last bar of the phrase?
                
                if (phraseTailResolution && random(0, 100) < 75) {
                    // Gravitational magnetic pull to stable chord tones: Root (0), Third (2), or Fifth (4)
                    uint8_t stableNotes[3] = {0, 2, 4};
                    lastScalePositionIndex = stableNotes[random(0, 3)];
                } else {
                    // Running generative performance algorithms
                    if (currentMelodyStyle == 0) {
                        // Directional Momentum Rule (Avoids nervous back-and-forth micro jitter)
                        static int8_t melodyDirection = 1; // 1 = Upwards, -1 = Downwards
                        if (random(0, 100) < 20) melodyDirection *= -1; // 20% chance to flip phrase vector

                        int8_t proposedIndex = lastScalePositionIndex + melodyDirection;
                        if (proposedIndex < 0) { proposedIndex = 1; melodyDirection = 1; }
                        if (proposedIndex > 6) { proposedIndex = 5; melodyDirection = -1; }
                        lastScalePositionIndex = (uint8_t)proposedIndex;
                    }
                    else if (currentMelodyStyle == 1) {
                        // Wide interval skipped arpeggios
                        lastScalePositionIndex = (lastScalePositionIndex <= 2) ? random(4, 7) : random(0, 3);
                    }
                    else if (currentMelodyStyle == 2) {
                        // Balanced alternating step structure
                        lastScalePositionIndex = (tickCount % 2 == 0) ? 2 : 4;
                    }
                }
            }

            // --- SCALE LOOKUP SELECTION ---
            uint8_t baseNote = 0;
         
            if (triggerGoaMotive) {
                baseNote = scaleGoa[currentChordIndex][lastScalePositionIndex];
            } else {
                if (currentArtistStyle == 1)      baseNote = scaleDorian[currentChordIndex][lastScalePositionIndex];
                else if (currentArtistStyle == 2) baseNote = scaleMajor[currentChordIndex][lastScalePositionIndex];
                else if (currentArtistStyle == 3) baseNote = scaleAbstract[currentChordIndex][lastScalePositionIndex];
                else if (currentArtistStyle == 4) baseNote = scaleAeolian[currentChordIndex][lastScalePositionIndex];
                else                              baseNote = scaleMinor[currentChordIndex][lastScalePositionIndex];
            }

            baseNote = constrain(baseNote + globalKeyTransposition, 24, 110);
            activeMelodyBaseNote = baseNote;

            // --- OCTAVE CONFIGURATION LAYER ---
            uint8_t octaveShift = 12;
            if (currentMelodyStyle == 1) {
                octaveShift = (lastScalePositionIndex <= 2) ? 12 : 36;
            } else if (currentMelodyStyle == 3) {
                octaveShift = 12;
            } else {
                if (stepQuad < beatsPerBar) {
                    octaveShift = 24; 
                } else if (stepQuad < (beatsPerBar * 2)) {
                    octaveShift = 36; 
                } else {
                    octaveShift = 12; 
                }
            }	

            uint8_t targetMidiNote = constrain(baseNote + octaveShift, 24, 127);

            // --- EXPRESSIVE LEAD ACCENTS ---
            uint8_t expressiveVelocity = 55 + (stepBase * 3) + random(0, 12);
            if (triggerGoaMotive)        expressiveVelocity += 25; 
            if (currentMelodyStyle == 3) expressiveVelocity = 45;

            // Automation message firing
            if (currentMelodyStyle == 3) {
                midiMsg(0xB0 | (activeSynthChannel - 1), 72, random(80, 115));
            } else if (stepBase == 0) {
                midiMsg(0xB0 | (activeSynthChannel - 1), 72, 55);
            }

            // --- DYNAMIC PANNING DRIFT ---
            uint8_t synthPan = 64 + random(-activePanIntensity, activePanIntensity + 1);
            midiMsg(0xB0 | (activeSynthChannel - 1), 10, constrain(synthPan, 10, 118));

            // --- TRIGGER NEW NOTE-ON ---
            midiMsg(0x90 | (activeSynthChannel - 1), targetMidiNote, expressiveVelocity);
            lastArpNotes[synthIdx] = targetMidiNote;
            goaNoteDurationCounter = 0;

        } else {
            // --- NO NOTE TRIGGERED THIS STEP: PROCESS EXPIRATION DECAY OVERRIDE ---
            if (lastArpNotes[synthIdx] > 0) {
                goaNoteDurationCounter++;
                bool shouldTurnOff = false;
                switch (goaMotiveDuration) {
                    case 0: shouldTurnOff = true; break;  
                    case 1: if (goaNoteDurationCounter >= 1) shouldTurnOff = true; break; 
                    case 2: if (goaNoteDurationCounter >= 2) shouldTurnOff = true; break; 
                    case 3: if (goaNoteDurationCounter >= 4) shouldTurnOff = true; break; 
                    case 4: shouldTurnOff = false; break; // Legato slide ignore
                }

                if (shouldTurnOff) {
                    midiMsg(0x80 | (activeSynthChannel - 1), lastArpNotes[synthIdx], 0);
                    lastArpNotes[synthIdx] = 0;
                    goaNoteDurationCounter = 0;
                }
            }
        }
    }

    // --- GROUP 4: OSCILLATORS / LOW BASS DRONES ---
    if (!systemMuteArray[activeBassChannel]) {
        uint8_t matrixIdx = activeBassChannel - BASS_START;
    
        // --- EVALUATE NOTE-OFF TRANSITIONS ---
        // Signature-aware dynamic gates to keep the psybient low-end tight
        bool shouldCutNote = false;
        if (currentTimeSignature == 3 && stepBase == 2) shouldCutNote = true; // Tight 3/3 cut
        else if (stepBase == (currentTimeSignature - 1)) shouldCutNote = true; // Natural bar-end gate
    
        if (lastDroneNotes[matrixIdx] > 0 && shouldCutNote) {
            midiMsg(0x80 | (activeBassChannel - 1), lastDroneNotes[matrixIdx], 0);
            lastDroneNotes[matrixIdx] = 0;
        }

        // --- GENERATIVE RHYTHMIC PLACEMENT ---
        bool dynamicBassTrigger = false;
    
        // Sweet spots for syncopated psybient bass grouping based on active signatures
        if (stepBase == 0) {
            dynamicBassTrigger = (random(0, 100) < 85); // High downbeat anchor probability
        } else if (currentTimeSignature == 3 && stepBase == 1) {
            dynamicBassTrigger = (random(0, 100) < 40); // 3/3 Syncopated pocket
        } else if (currentTimeSignature == 5 && (stepBase == 2 || stepBase == 3)) {
            dynamicBassTrigger = (random(0, 100) < 50); // 5/5 Odd-meter groove variance
        } else if (currentTimeSignature == 4 && stepBase == 2) {
            dynamicBassTrigger = (random(0, 100) < 30); // Standard psychedelic off-beat push
        }

        if (dynamicBassTrigger) {
            // Guard against overlap: Cut old bass note immediately before triggering new one
            if (lastDroneNotes[matrixIdx] > 0) {
                midiMsg(0x80 | (activeBassChannel - 1), lastDroneNotes[matrixIdx], 0);
            }

            uint8_t targetBassNote = 0;

            if (bassMelodyInherit && activeMelodyBaseNote > 0) {
                // --- VARIATION 1: SMART VOICE LEADING & COUNTERPOINT ---
                uint8_t voiceLeadRoll = random(0, 100);
                if (voiceLeadRoll < 60)       targetBassNote = activeMelodyBaseNote - 24; // Standard Sub-Octave Down
                else if (voiceLeadRoll < 85)  targetBassNote = activeMelodyBaseNote - 17; // Drop to Perfect Fifth below
                else                          targetBassNote = activeMelodyBaseNote - 12; // High-register counter-melody
            } else {
                // --- VARIATION 2: ROOT OR MODAL EXTENSIONS ---
                uint8_t rawNote = scaleMinor[currentChordIndex][0];
                if (currentArtistStyle == 1)      rawNote = scaleDorian[currentChordIndex][0];
                else if (currentArtistStyle == 2) rawNote = scaleMajor[currentChordIndex][0];
                else if (currentArtistStyle == 3) rawNote = scaleAbstract[currentChordIndex][0];
                else if (currentArtistStyle == 4) rawNote = scaleAeolian[currentChordIndex][0];

                // 25% chance to wander to the scale's minor third note for walking harmonic movement
                if (random(0, 100) < 25) {
                    rawNote = scaleMinor[currentChordIndex][1]; 
                }

                targetBassNote = constrain(rawNote + globalKeyTransposition - 36, 12, 90);
            }

            // --- VARIATION 3: GHOST CONTEXTUAL OCTAVE JUMPS ---
            // Breaks up heavy low-end stasis with upward pitch bounces on inner ticks
            if (longBarCounter % 2 == 0 && stepBase > 0 && random(0, 100) < 35) {
                targetBassNote += 12;
            }

            // --- VELOCITY EXPANSION ---
            uint8_t bassVelocity = (bassMelodyInherit) ? 58 : 42;
            if (stepBase == 0) bassVelocity += random(5, 12); // Extra physical accent on downbeats

            lastDroneNotes[matrixIdx] = targetBassNote;
            midiMsg(0x90 | (activeBassChannel - 1), lastDroneNotes[matrixIdx], bassVelocity);

            // --- VARIATION 4: TIMBRAL AUTOMATION DRIFT ---
            uint8_t bassFilterAccent = 40 + (stepBase * 10) + random(0, 15);
            midiMsg(0xB0 | (activeBassChannel - 1), 83, constrain(bassFilterAccent, 20, 110));
        }
    }

    // --- GROUP 1: DRUMS / PERCUSSION (DYNAMIC SYNC) ---
    if (!systemMuteArray[activeDrumChannel]) {
        // Dynamic Downbeat Kick: Always strikes on step 0, regardless of time signature
        if (stepBase == 0) {
            midiMsg(0xB0 | (activeDrumChannel - 1), 10, 64);
            midiMsg(0x90 | (activeDrumChannel - 1), 36, 105);
        } 
        // Dynamic Mid-measure Upbeat Accents
        else if (stepBase == (beatsPerBar / 2) && currentTimeSignature > 3) {
            if (random(0,100) < 50) {
                midiMsg(0xB0 | (activeDrumChannel - 1), 10, 64);
                midiMsg(0x90 | (activeDrumChannel - 1), 36, 105);
            }
        }
        // Dynamic Ghost Notes: Strikes dynamically on the second-to-last step of a measure
        else if (stepBase == (beatsPerBar - 1) && (random(0, 100) < ghostKickChance)) {
            midiMsg(0xB0 | (activeDrumChannel - 1), 10, 64);
            midiMsg(0x90 | (activeDrumChannel - 1), 36, random(45, 65));
        }

        // Structural Percussion Tail Accent
        if (stepBase == (beatsPerBar - 1) && random(0, 100) < globalDrumDensity) {
            midiMsg(0xB0 | (activeDrumChannel - 1), 10, 64);
            midiMsg(0x90 | (activeDrumChannel - 1), 41, random(45, 68));
        }
    }


    // --- GROUP 2: HI-HATS / SHAKERS ENGINE (DYNAMIC TIME MATRIX) ---
    if (!systemMuteArray[activeHatChannel]) {
        uint8_t hatRoll = random(0, 100);
        uint8_t targetTriggerThreshold = 30; // Baseline fallback

        // Math-driven accent tracking that shifts based on the signature
        if (stepBase == 0) {
            targetTriggerThreshold = 15; // Keep downbeats empty for heavy kick headroom
        } else if (stepBase == 1 || stepBase == (beatsPerBar - 1)) {
            targetTriggerThreshold = 65; // Heavily emphasize the syncopated micro-edges
        } else if (stepBase % 2 == 0) {
            targetTriggerThreshold = 45; // Moderate shuffle on inner even nodes
        }

        if (currentArtistStyle == 0 || currentArtistStyle == 3) {
            targetTriggerThreshold = targetTriggerThreshold / 2;
        }

        if (hatRoll < targetTriggerThreshold) {
            uint8_t softVelocity = 28 + random(0, 20) + (stepBase * 2);
            uint8_t targetHatNote = 42;
            if (softVelocity < 38) {
                targetHatNote = 44;
            } else if (currentArtistStyle == 2 && hatRoll < 5) {
                targetHatNote = 46;
            }

            uint8_t hatPan = 64 + random(-(activePanIntensity / 2), (activePanIntensity / 2) + 1);
            midiMsg(0xB0 | (activeHatChannel - 1), 10, constrain(hatPan, 20, 108));

            // Dynamic decay envelope modulation mapped to velocity updates (CC 72)
            uint8_t dynamicDecayValue = 20 + (softVelocity / 2) + random(0, 10);
            midiMsg(0xB0 | (activeHatChannel - 1), 72, constrain(dynamicDecayValue, 15, 80));
            
            midiMsg(0x90 | (activeHatChannel - 1), targetHatNote, softVelocity);
        }
    }

    // --- BAR COUNT & SYSTEM TICK ADVANCEMENT ---
    if (stepBase == (beatsPerBar - 1)) {
        longBarCounter++; 
    }

    tickCount++;
    if (tickCount >= quadBar) {
        tickCount = 0; 
    }
}

void setup() {
    MIDI_SERIAL.begin(31250, SERIAL_8N1, RX_PIN, TX_PIN);
    Serial.begin(115200);
    randomSeed(analogRead(35));

#ifdef DISPLAY_ENABLED
    tft.init();
    //tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_PURPLE, TFT_BLACK);
    tft.setTextSize(1);

    displayArray("bpm", 1);
    displayArray("pattern", 2);
    displayArray("style", 3);
    displayArray("synth", 4);
    displayArray("bass", 5);
    displayArray("drum", 6);
    displayArray("hats", 7);
    displayArray("bar", 8);
#endif

    currentArtistStyle = random(0, 5);
    transitionToNextArtist();
    delay(1000);
}

void loop() {
    static uint32_t nextTick = micros();
    uint32_t now = micros();
    if (now >= nextTick) { 
        nextTick += stepInterval;
       	runPsybientEngine();
    }
    yield();
}

