/*
 * Clock Divider/Multiplier for OAM Uncertainty
 * Arduino C++ Version
 * 
 * Outputs: /1, /2, /4, /8, /16, /32, x2, x4
 * CV input: Clock signal + Reset (high voltage)
 * 
 * Hardware: Raspberry Pi Pico (RP2040) in Uncertainty module
 */

// Pin definitions for OAM Uncertainty
#define CV_INPUT_PIN 26
const int GATE_PINS[8] = {27, 28, 29, 0, 3, 4, 2, 1};
// Note: LEDs are connected to the same pins as gates, so they light up automatically

// Timing constants
const int GATE_LENGTH = 10;        // Gate pulse length in milliseconds (shorter for better timing)
const int CLOCK_THRESHOLD = 820;   // ADC threshold for +1V clock detection (~1V in 0-3.3V range)
const int RESET_THRESHOLD = 2730;  // ADC threshold for reset trigger (~2.2V)
const int DEBOUNCE_TIME = 2;       // Debounce time in milliseconds

// State variables
unsigned long division_counters[5] = {0, 0, 0, 0, 0}; // For /2, /4, /8, /12, /16
unsigned long gate_timers[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // When each gate should turn off
unsigned long last_clock_time = 0;
unsigned long clock_interval = 500; // Default interval (120 BPM)
bool last_clock_state = false;
bool last_reset_state = false;
unsigned long last_debounce_time = 0;

// Multiplication timing
unsigned long x2_timer = 0;
unsigned long x3_timers[2] = {0, 0}; // For the 2 extra pulses in x3
unsigned long x4_timers[3] = {0, 0, 0}; // For the 3 extra pulses in x4

void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  
  // Set up pins
  pinMode(CV_INPUT_PIN, INPUT);
  for (int i = 0; i < 8; i++) {
    pinMode(GATE_PINS[i], OUTPUT);
    digitalWrite(GATE_PINS[i], LOW);
  }
  
  Serial.println("Clock Divider/Multiplier Started");
  Serial.println("Outputs: /2, /4, /8, /12, /16, x2, x3, x4");
  Serial.println("CV Input: Clock + Reset (high voltage)");
}

void loop() {
  unsigned long current_time = millis();
  
  // Read CV input
  int cv_value = analogRead(CV_INPUT_PIN);
  
  // Print CV value every 500ms for debugging
  static unsigned long last_debug_time = 0;
  if (current_time - last_debug_time > 500) {
    Serial.print("CV ADC value: ");
    Serial.println(cv_value);
    last_debug_time = current_time;
  }
  
  // Check for reset (high CV voltage)
  checkReset(cv_value, current_time);
  
  // Detect clock edges
  if (detectClockEdge(cv_value, current_time)) {
    processDivisions();
    setupMultiplications(current_time);
  }
  
  // Handle multiplication timing
  handleMultiplications(current_time);
  
  // Update gate timing (turn off gates when time expires)
  updateGateTiming(current_time);
  
  // Small delay to prevent overwhelming the processor
  delayMicroseconds(100);
}

bool detectClockEdge(int cv_value, unsigned long current_time) {
  bool clock_present = cv_value > CLOCK_THRESHOLD;
  
  // Detect rising edge with debouncing
  if (clock_present && !last_clock_state && 
      (current_time - last_debounce_time) > DEBOUNCE_TIME) {
    
    // Calculate clock interval
    if (last_clock_time > 0) {
      clock_interval = current_time - last_clock_time;
      // Limit clock interval to reasonable range (30ms to 2000ms = 30-2000 BPM)
      if (clock_interval < 30) clock_interval = 30;
      if (clock_interval > 2000) clock_interval = 2000;
    }
    
    last_clock_time = current_time;
    last_clock_state = true;
    last_debounce_time = current_time;
    
    // Debug output
    int bpm = (clock_interval > 0) ? (60000 / clock_interval) : 0;
    Serial.print("Clock detected - ADC: ");
    Serial.print(cv_value);
    Serial.print(", BPM: ");
    Serial.println(bpm);
    
    return true;
  } else if (!clock_present) {
    last_clock_state = false;
  }
  
  return false;
}

void checkReset(int cv_value, unsigned long current_time) {
  bool reset_present = cv_value > RESET_THRESHOLD;
  
  if (reset_present && !last_reset_state) {
    resetAllCounters();
    last_reset_state = true;
    Serial.println("Reset triggered");
  } else if (!reset_present) {
    last_reset_state = false;
  }
}

void resetAllCounters() {
  // Reset all division counters
  for (int i = 0; i < 5; i++) {
    division_counters[i] = 0;
  }
  
  // Turn off all gates immediately
  for (int i = 0; i < 8; i++) {
    digitalWrite(GATE_PINS[i], LOW);
    gate_timers[i] = 0;
  }
  
  // Reset multiplication timers
  x2_timer = 0;
  for (int i = 0; i < 2; i++) {
    x3_timers[i] = 0;
  }
  for (int i = 0; i < 3; i++) {
    x4_timers[i] = 0;
  }
}

void processDivisions() {
  // Gate 0: /2 (every 2nd clock)
  division_counters[0] = (division_counters[0] + 1) % 2;
  if (division_counters[0] == 0) {
    triggerGate(0);
  }
  
  // Gate 1: /4 (every 4th clock)
  division_counters[1] = (division_counters[1] + 1) % 4;
  if (division_counters[1] == 0) {
    triggerGate(1);
  }
  
  // Gate 2: /8 (every 8th clock)
  division_counters[2] = (division_counters[2] + 1) % 8;
  if (division_counters[2] == 0) {
    triggerGate(2);
  }
  
  // Gate 3: /12 (every 12th clock)
  division_counters[3] = (division_counters[3] + 1) % 12;
  if (division_counters[3] == 0) {
    triggerGate(3);
  }
  
  // Gate 4: /16 (every 16th clock)
  division_counters[4] = (division_counters[4] + 1) % 16;
  if (division_counters[4] == 0) {
    triggerGate(4);
  }
}

void setupMultiplications(unsigned long current_time) {
  // x2 multiplication (Gate 5)
  if (clock_interval > 100) { // Only if clock isn't too fast
    x2_timer = current_time + (clock_interval / 2);
  }
  
  // x3 multiplication (Gate 6) - set up 2 additional pulses
  if (clock_interval > 150) { // Only if clock isn't too fast
    x3_timers[0] = current_time + (clock_interval / 3);     // 1/3
    x3_timers[1] = current_time + (2 * clock_interval / 3); // 2/3
  }
  
  // x4 multiplication (Gate 7) - set up 3 additional pulses
  if (clock_interval > 200) { // Only if clock isn't too fast
    x4_timers[0] = current_time + (clock_interval / 4);     // 1/4
    x4_timers[1] = current_time + (clock_interval / 2);     // 1/2  
    x4_timers[2] = current_time + (3 * clock_interval / 4); // 3/4
  }
}

void handleMultiplications(unsigned long current_time) {
  // x2 multiplication (Gate 5)
  if (x2_timer > 0 && current_time >= x2_timer) {
    triggerGate(5);
    x2_timer = 0; // Reset timer
  }
  
  // x3 multiplication (Gate 6)
  for (int i = 0; i < 2; i++) {
    if (x3_timers[i] > 0 && current_time >= x3_timers[i]) {
      triggerGate(6);
      x3_timers[i] = 0; // Reset this timer
    }
  }
  
  // x4 multiplication (Gate 7)
  for (int i = 0; i < 3; i++) {
    if (x4_timers[i] > 0 && current_time >= x4_timers[i]) {
      triggerGate(7);
      x4_timers[i] = 0; // Reset this timer
    }
  }
}

void triggerGate(int gate_index) {
  if (gate_index < 8) {
    digitalWrite(GATE_PINS[gate_index], HIGH);
    gate_timers[gate_index] = millis() + GATE_LENGTH;
  }
}

void updateGateTiming(unsigned long current_time) {
  for (int i = 0; i < 8; i++) {
    if (gate_timers[i] > 0 && current_time >= gate_timers[i]) {
      digitalWrite(GATE_PINS[i], LOW);
      gate_timers[i] = 0;
    }
  }
}