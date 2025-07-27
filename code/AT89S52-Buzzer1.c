/**
 * AT89S52 Buzzer Controller - Final Working Version
 * All patterns working with proper static variable declarations
 * Adapted for SDCC compilation
 */

#include <8051.h>
#include <stdint.h>

/*----- Hardware Connections -----*/
// Status LEDs (active low)
__sbit __at (0xA0 + 6) SPEED_LED;    // Blue (P2.6)
__sbit __at (0xA0 + 7) RANGE_LED;    // green (P2.7)

// Pattern LEDs
__sbit __at (0xA0 + 0) UP_LED;       // red (P2.0)
__sbit __at (0xA0 + 1) DOWN_LED;     // red (P2.1)
__sbit __at (0xA0 + 2) ZIGZAG_LED;   // red (P2.2)
__sbit __at (0xA0 + 3) RAND_LED;     // red (P2.3)
__sbit __at (0xA0 + 4) PULSE_LED;    // red (P2.4)
__sbit __at (0x90 + 5) TRIANGLE_LED; // red (P1.5)

// Additional LEDs (P1)
__sbit __at (0xA0 + 0) STEP_LED;     // red (P2.0) - Note: Same as UP_LED
__sbit __at (0xA0 + 1) HEART_LED;    // red (P2.1) - Note: Same as DOWN_LED
__sbit __at (0xA0 + 2) SIREN_LED;    // red (P2.2) - Note: Same as ZIGZAG_LED
__sbit __at (0xA0 + 3) CHIRP_LED;    // red (P2.3) - Note: Same as RAND_LED
__sbit __at (0xA0 + 4) WALK_LED;     // red (P2.4) - Note: Same as PULSE_LED



// Buzzer outputs
__sbit __at (0xB0 + 0) BUZZER;      // P3.0 (normal)
__sbit __at (0xB0 + 1) BUZZER_COMP; // P3.1 (complement)

//  buttons
__sbit __at (0xB0 + 2) BTN_POWER;    // Power button (P3.2)
__sbit __at (0xB0 + 3) BTN_PATTERN;  // Pattern button (P3.3)
__sbit __at (0xB0 + 4) BTN_SPEED;    // Speed button (P3.4)
__sbit __at (0xB0 + 5) BTN_RANGE;    // Range button (P3.5)

/*----- System State -----*/
__bit isActive = 0;                // Power state
__bit currentRange = 0;            // 0=5-10kHz, 1=18-27kHz
uint8_t currentPattern = 0;        // Current pattern (0-10)
uint8_t currentSpeed = 0;          // Speed setting (0-4)
__bit sweepDirection = 0;          // For zigzag pattern

/*----- Sound Parameters -----*/
uint16_t currentFreqDelay;         // Current delay value

// Frequency range parameters [min, max, initial]
const uint16_t rangeParams[2][3] = {
    {25, 50, 37},  // 5-10kHz range
    {9, 18, 13}    // 18-27kHz range
};

// Speed multipliers
const uint8_t speedSteps[5] = {1, 2, 3, 5, 8};

/*----- Function Prototypes -----*/
void delay_ms(uint16_t ms);
void updateStatusLEDs(void);
__bit checkButton_PWR(void);
__bit checkButton_PAT(void);
__bit checkButton_SPD(void);
__bit checkButton_RNG(void);
void handleButtons(void);
void generate_tone(void);
void update_sweep(void);
uint8_t simple_rand(void);

/*----- Delay Function -----*/
void delay_ms(uint16_t ms) {
    uint16_t i, j;
    for(i=0; i<ms; i++)
        for(j=0; j<120; j++);  // ~1ms at 12MHz
}

/*----- Timer 0 ISR -----*/
void Timer0_ISR() __interrupt(1) {
    static uint16_t msCount = 0;
    TH0 = 0xFC; TL0 = 0x66;  // Reload for 1ms
    
    if(isActive) {
        if(++msCount >= 100) {  // 5Hz blink
            SPEED_LED = !SPEED_LED;
            msCount = 0;
        }
    } else {
        SPEED_LED = 1;  // Turn off (active low)
    }
}

/*----- Update Status LEDs -----*/
void updateStatusLEDs() {
    // Power and range indicators
    // Note: POWER_LED not defined in new pin config, using SPEED_LED and RANGE_LED
    RANGE_LED = !currentRange;
    
    // Pattern indicators (only one active at a time)
    UP_LED      = (currentPattern != 0);
    DOWN_LED    = (currentPattern != 1);
    ZIGZAG_LED  = (currentPattern != 2);
    RAND_LED    = (currentPattern != 3);
    PULSE_LED   = (currentPattern != 4);
    STEP_LED    = (currentPattern != 5);
    TRIANGLE_LED= (currentPattern != 6);
    HEART_LED   = (currentPattern != 7);
    SIREN_LED   = (currentPattern != 8);
    CHIRP_LED   = (currentPattern != 9);
    WALK_LED    = (currentPattern != 10);
}

/*----- Random Number Generator -----*/
uint8_t simple_rand() {
    static uint16_t seed = 12345;
    seed = (seed * 1103515245 + 12345) % 32768;
    return (uint8_t)(seed & 0xFF);
}

/*----- Button Check Functions -----*/
__bit checkButton_PWR() {
    static __bit lastState = 1;
    __bit current = BTN_POWER;
    if(current != lastState) {
        delay_ms(20);  // Debounce delay
        if(BTN_POWER == current) {
            lastState = current;
            return !current;  // Active low
        }
    }
    return 0;
}

__bit checkButton_PAT() {
    static __bit lastState = 1;
    __bit current = BTN_PATTERN;
    if(current != lastState) {
        delay_ms(20);
        if(BTN_PATTERN == current) {
            lastState = current;
            return !current;
        }
    }
    return 0;
}

__bit checkButton_SPD() {
    static __bit lastState = 1;
    __bit current = BTN_SPEED;
    if(current != lastState) {
        delay_ms(20);
        if(BTN_SPEED == current) {
            lastState = current;
            return !current;
        }
    }
    return 0;
}

__bit checkButton_RNG() {
    static __bit lastState = 1;
    __bit current = BTN_RANGE;
    if(current != lastState) {
        delay_ms(20);
        if(BTN_RANGE == current) {
            lastState = current;
            return !current;
        }
    }
    return 0;
}

/*----- Tone Generation -----*/
void generate_tone() {
    static uint16_t toneCounter = 0;
    if(++toneCounter >= currentFreqDelay) {
        toneCounter = 0;
        BUZZER = !BUZZER;
        BUZZER_COMP = !BUZZER_COMP;
    }
}

/*----- Pattern Implementations -----*/
void update_sweep() {
    uint16_t minDelay = rangeParams[currentRange][0];
    uint16_t maxDelay = rangeParams[currentRange][1];
    
    // Declare all static variables at function start
    static uint16_t pulseCount = 0;       // For pulse pattern
    static uint16_t stepCount = 0;        // For stepped pattern
    static int16_t freqStep = 1;          // For triangle pattern
    static uint16_t hbCount = 0;          // For heartbeat pattern
    static uint16_t sirenCount = 0;       // For siren pattern
    static uint8_t chirpState = 0;        // For chirps pattern
    static uint16_t chirpCount = 0;       // For chirps pattern
    static uint16_t walkCount = 0;        // For random walk pattern
    
    switch(currentPattern) {
        case 0: // Up Sweep
            if(currentFreqDelay > minDelay) 
                currentFreqDelay -= speedSteps[currentSpeed];
            else 
                currentFreqDelay = maxDelay;
            break;
            
        case 1: // Down Sweep
            if(currentFreqDelay < maxDelay) 
                currentFreqDelay += speedSteps[currentSpeed];
            else 
                currentFreqDelay = minDelay;
            break;
            
        case 2: // Zig-Zag
            if(sweepDirection) {
                if(currentFreqDelay < maxDelay) 
                    currentFreqDelay += speedSteps[currentSpeed];
                else 
                    sweepDirection = 0;
            } else {
                if(currentFreqDelay > minDelay) 
                    currentFreqDelay -= speedSteps[currentSpeed];
                else 
                    sweepDirection = 1;
            }
            break;
            
        case 3: // Random
            if(simple_rand() < 20) // ~8% chance to change
                currentFreqDelay = minDelay + (simple_rand() % (maxDelay-minDelay+1));
            break;
            
        case 4: // Pulse
            if(++pulseCount >= 500) pulseCount = 0;
            BUZZER = (pulseCount < 50); // 10% duty cycle
            break;
            
        case 5: // Stepped
            if(++stepCount >= 100) {
                stepCount = 0;
                currentFreqDelay = minDelay + 
                    ((currentFreqDelay + speedSteps[currentSpeed] - minDelay) % 
                    (maxDelay-minDelay+1));
            }
            break;
            
        case 6: // Triangle
            currentFreqDelay += freqStep * speedSteps[currentSpeed];
            if(currentFreqDelay <= minDelay || currentFreqDelay >= maxDelay)
                freqStep = -freqStep;
            break;
            
        case 7: // Heartbeat
            if(++hbCount >= 600) hbCount = 0;
            
            if(hbCount < 100)       // First beat
                currentFreqDelay = minDelay + 2;
            else if(hbCount < 150)  // First pause
                currentFreqDelay = maxDelay;
            else if(hbCount < 250)  // Second beat
                currentFreqDelay = minDelay + 1;
            else                    // Second pause
                currentFreqDelay = maxDelay;
            break;
            
        case 8: // Siren
            if(++sirenCount >= 100) {
                sirenCount = 0;
                currentFreqDelay = (currentFreqDelay == minDelay) ? maxDelay : minDelay;
            }
            break;
            
        case 9: // Chirps
            if(chirpState == 0) {
                if(currentFreqDelay > minDelay)
                    currentFreqDelay -= speedSteps[currentSpeed]*3;
                else
                    chirpState = 1;
            } else if(++chirpCount > 300) {
                chirpCount = 0;
                chirpState = 0;
                currentFreqDelay = maxDelay;
            }
            break;
            
        case 10: // Random Walk
            if(++walkCount >= 20) {
                walkCount = 0;
                currentFreqDelay += (simple_rand() % 5) - 2;
                if(currentFreqDelay < minDelay) currentFreqDelay = minDelay;
                if(currentFreqDelay > maxDelay) currentFreqDelay = maxDelay;
            }
            break;
    }
}

/*----- Main Program -----*/
void main() {
    // Initialize hardware
    P0 = P1 = P2 = P3 = 0xFF; // All LEDs off (active low)
    TMOD = 0x01;               // Timer 0 mode 1
    TH0 = 0xFC; TL0 = 0x66;    // 1ms timer @12MHz
    ET0 = 1;                   // Enable Timer 0 interrupt
    TR0 = 1;                   // Start Timer 0
    EA = 1;                    // Enable global interrupts
    
    // Initial state
    currentRange = 0;
    currentFreqDelay = rangeParams[currentRange][2];
    updateStatusLEDs();
    
    // Main loop
    while(1) {
        // Check buttons
        if(checkButton_PWR()) {
            isActive = !isActive;
            if(!isActive) BUZZER = 0;
            updateStatusLEDs();
        }
        
        if(checkButton_PAT()) {
            if(++currentPattern >= 11) currentPattern = 0;
            updateStatusLEDs();
        }
        
        if(checkButton_SPD()) {
            if(++currentSpeed >= 5) currentSpeed = 0;
            updateStatusLEDs();
        }
        
        if(checkButton_RNG()) {
            currentRange = !currentRange;
            currentFreqDelay = rangeParams[currentRange][2];
            updateStatusLEDs();
        }
        
        // Generate sound if active
        if(isActive) {
            generate_tone();
            update_sweep();
        }
    }
}