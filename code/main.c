/**
 * AT89S52 Buzzer Controller - Final Working Version
 * All patterns working with proper static variable declarations
 */

#include <reg52.h>
#include <intrins.h>

/*----- Hardware Connections -----*/
// Status LEDs (active low)
sbit POWER_LED = P2^6;    // Red
sbit SPEED_LED = P2^6;    // Blue
sbit RANGE_LED = P2^7;    // White

// Pattern LEDs
sbit UP_LED     = P2^0;   // Green 
sbit DOWN_LED   = P2^1;   // Yellow
sbit ZIGZAG_LED = P2^2;   // Cyan
sbit RAND_LED   = P2^3;   // Purple
sbit PULSE_LED  = P2^4;   // White

// Additional LEDs (P1)
sbit STEP_LED    = P2^5;  // Pink
sbit TRIANGLE_LED= P2^0;  // Orange
sbit HEART_LED   = P2^1;  // Red
sbit SIREN_LED   = P2^2;  // Blue
sbit CHIRP_LED   = P2^3;  // Green
sbit WALK_LED    = P2^4;  // Yellow


// Audio outputs
sbit BUZZER = P1^2;       // Main buzzer output
sbit BUZZER_INV = P1^3;   // Inverted buzzer output 

// Buzzer and buttons
sbit BTN_POWER   = P3^2;  // Power button
sbit BTN_PATTERN = P3^3;  // Pattern button
sbit BTN_SPEED   = P3^4;  // Speed button
sbit BTN_RANGE   = P3^5;  // Range button

/*----- System State -----*/
bit isActive = 0;                // Power state
bit currentRange = 0;            // 0=5-10kHz, 1=18-27kHz
unsigned char currentPattern = 0; // Current pattern (0-10)
unsigned char currentSpeed = 0;   // Speed setting (0-4)
bit sweepDirection = 0;           // For zigzag pattern

/*----- Sound Parameters -----*/
unsigned int currentFreqDelay;    // Current delay value

// Frequency range parameters [min, max, initial]
const unsigned int rangeParams[2][3] = {
    {25, 50, 37},  // 5-10kHz range
    {9, 18, 13}    // 18-27kHz range
};

// Speed multipliers
const unsigned char speedSteps[5] = {1, 2, 3, 5, 8};

/*----- Function Prototypes -----*/
void delay_ms(unsigned int ms);
void updateStatusLEDs(void);
bit checkButton_PWR(void);
bit checkButton_PAT(void);
bit checkButton_SPD(void);
bit checkButton_RNG(void);
void handleButtons(void);
void generate_tone(void);
void update_sweep(void);
unsigned char simple_rand(void);

/*----- Delay Function -----*/
void delay_ms(unsigned int ms) {
    unsigned int i, j;
    for(i=0; i<ms; i++)
        for(j=0; j<120; j++);  // ~1ms at 12MHz
}

/*----- Timer 0 ISR -----*/
void Timer0_ISR() interrupt 1 {
    static unsigned int msCount = 0;
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
    POWER_LED = !isActive;  // Active low
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
unsigned char simple_rand() {
    static unsigned int seed = 12345;
    seed = (seed * 1103515245 + 12345) % 32768;
    return (unsigned char)(seed & 0xFF);
}

/*----- Button Check Functions -----*/
bit checkButton_PWR() {
    static bit lastState = 1;
    bit current = BTN_POWER;
    if(current != lastState) {
        delay_ms(20);  // Debounce delay
        if(BTN_POWER == current) {
            lastState = current;
            return !current;  // Active low
        }
    }
    return 0;
}

bit checkButton_PAT() {
    static bit lastState = 1;
    bit current = BTN_PATTERN;
    if(current != lastState) {
        delay_ms(20);
        if(BTN_PATTERN == current) {
            lastState = current;
            return !current;
        }
    }
    return 0;
}

bit checkButton_SPD() {
    static bit lastState = 1;
    bit current = BTN_SPEED;
    if(current != lastState) {
        delay_ms(20);
        if(BTN_SPEED == current) {
            lastState = current;
            return !current;
        }
    }
    return 0;
}

bit checkButton_RNG() {
    static bit lastState = 1;
    bit current = BTN_RANGE;
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
    static unsigned int toneCounter = 0;
    if(++toneCounter >= currentFreqDelay) {
        toneCounter = 0;
        BUZZER = !BUZZER;
        BUZZER_INV = !BUZZER;  // Inverted output
    }
}

/*----- Pattern Implementations -----*/
void update_sweep() {
    unsigned int minDelay = rangeParams[currentRange][0];
    unsigned int maxDelay = rangeParams[currentRange][1];
    
    // Declare all static variables at function start
    static unsigned int pulseCount = 0;       // For pulse pattern
    static unsigned int stepCount = 0;        // For stepped pattern
    static int freqStep = 1;                 // For triangle pattern
    static unsigned int hbCount = 0;         // For heartbeat pattern
    static unsigned int sirenCount = 0;       // For siren pattern
    static unsigned int chirpState = 0;       // For chirps pattern
    static unsigned int chirpCount = 0;       // For chirps pattern
    static unsigned int walkCount = 0;        // For random walk pattern
    
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
    P0 = P1 = P2 = P3 = 0xFF; // All LEDs off (active hige)
    TMOD = 0x01;               // Timer 0 mode 1
    TH0 = 0xFC; TL0 = 0x66;    // 1ms timer @12MHz
    ET0 = TR0 = EA = 1;        // Enable timer and interrupts
    
	  BUZZER = 0;
    BUZZER_INV = 1;  // Start with inverted state
	
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