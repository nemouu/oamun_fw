/*
 * CDX - Clean Clock Divider/Multiplier for OAM Uncertainty
 * 
 * Outputs: /2, /4, /8, /12, /16, x2, x3, x4
 * Input: Clock/trigger signals only
 * 
 * Control: Gate length the length of the gates that are put out
 * - Short gates (5-50ms) = Punchy, staccato divisions
 * - Long gates (50-200ms) = Sustained, legato divisions
 */

// Pin definitions
#define CV_INPUT_PIN 26
const int GATE_PINS[8] = {27, 28, 29, 0, 3, 4, 2, 1};

// Configuration
const int TRIGGER_THRESHOLD = 1000;   // Threshold for clock detection
const int MIN_GATE_LENGTH = 5;        // Minimum output gate length
const int MAX_GATE_LENGTH = 200;      // Maximum output gate length
const int DEBOUNCE_TIME = 5;          // Debounce time in milliseconds

// Division counters
unsigned long division_counters[5] = {0, 0, 0, 0, 0}; // For /2, /4, /8, /12, /16

// Gate timing
unsigned long gate_timers[8] = {0, 0, 0, 0, 0, 0, 0, 0}; // When each gate should turn off

// Clock detection and gate length measurement
bool last_clock_state = false;
bool trigger_active = false;
unsigned long trigger_start_time = 0;
unsigned long input_gate_length = MIN_GATE_LENGTH;
unsigned long last_clock_time = 0;
unsigned long clock_interval = 500; // Default interval (120 BPM)
unsigned long last_debounce_time = 0;

// Multiplication timing
unsigned long x2_timer = 0;
unsigned long x3_timers[2] = {0, 0}; // For x3 multiplication
unsigned long x4_timers[3] = {0, 0, 0}; // For x4 multiplication

void setup() {
  Serial.begin(115200);
  
  // Set up pins
  pinMode(CV_INPUT_PIN, INPUT);
  for (int i = 0; i < 8; i++) {
    pinMode(GATE_PINS[i], OUTPUT);
    digitalWrite(GATE_PINS[i], LOW);
  }
  
  Serial.println("CDX Clock Divider/Multiplier");
  Serial.println("Outputs: /2, /4, /8, /12, /16, x2, x3, x4");
  Serial.println("Input gate length controls output gate length");
}

void loop() {
  unsigned long current_time = millis();
  int cv_value = analogRead(CV_INPUT_PIN);
  bool gate_high = cv_value > TRIGGER_THRESHOLD;
  
  // Measure input gate length
  measureGateLength(gate_high, current_time);
  
  // Detect clock edges
  if (detectClockEdge(gate_high, current_time)) {
    processDivisions();
    setupMultiplications(current_time);
  }
  
  // Handle multiplication timing
  handleMultiplications(current_time);
  
  // Update gate timing (turn off gates when time expires)
  updateGateTiming(current_time);
  
  delayMicroseconds(100);
}

void measureGateLength(bool gate_high, unsigned long current_time) {
  if (gate_high && !trigger_active) {
    // Gate just went high - start measuring
    trigger_active = true;
    trigger_start_time = current_time;
  } else if (!gate_high && trigger_active) {
    // Gate just went low - calculate length
    trigger_active = false;
    unsigned long measured_length = current_time - trigger_start_time;
    
    // Map measured length to output gate length
    input_gate_length = constrain(measured_length, MIN_GATE_LENGTH, MAX_GATE_LENGTH);
    
    Serial.print("Input gate: ");
    Serial.print(measured_length);
    Serial.print("ms -> Output gates: ");
    Serial.print(input_gate_length);
    Serial.println("ms");
  }
}

bool detectClockEdge(bool gate_high, unsigned long current_time) {
  
  // Detect rising edge with debouncing
  if (gate_high && !last_clock_state && 
      (current_time - last_debounce_time) > DEBOUNCE_TIME) {
    
    // Calculate clock interval
    if (last_clock_time > 0) {
      clock_interval = current_time - last_clock_time;
      // Keep interval in reasonable range
      clock_interval = constrain(clock_interval, 30, 2000);
    }
    
    last_clock_time = current_time;
    last_clock_state = true;
    last_debounce_time = current_time;
    
    // Debug output
    int bpm = (clock_interval > 0) ? (60000 / clock_interval) : 0;
    Serial.print("Clock - BPM: ");
    Serial.println(bpm);
    
    return true;
  } else if (!gate_high) {
    last_clock_state = false;
  }
  
  return false;
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
  
  // x3 multiplication (Gate 6)
  if (clock_interval > 150) {
    x3_timers[0] = current_time + (clock_interval / 3);     // 1/3
    x3_timers[1] = current_time + (2 * clock_interval / 3); // 2/3
  }
  
  // x4 multiplication (Gate 7)
  if (clock_interval > 200) {
    x4_timers[0] = current_time + (clock_interval / 4);     // 1/4
    x4_timers[1] = current_time + (clock_interval / 2);     // 1/2
    x4_timers[2] = current_time + (3 * clock_interval / 4); // 3/4
  }
}

void handleMultiplications(unsigned long current_time) {
  // x2 multiplication (Gate 5)
  if (x2_timer > 0 && current_time >= x2_timer) {
    triggerGate(5);
    x2_timer = 0;
  }
  
  // x3 multiplication (Gate 6)
  for (int i = 0; i < 2; i++) {
    if (x3_timers[i] > 0 && current_time >= x3_timers[i]) {
      triggerGate(6);
      x3_timers[i] = 0;
    }
  }
  
  // x4 multiplication (Gate 7)
  for (int i = 0; i < 3; i++) {
    if (x4_timers[i] > 0 && current_time >= x4_timers[i]) {
      triggerGate(7);
      x4_timers[i] = 0;
    }
  }
}

void triggerGate(int gate_index) {
  if (gate_index < 8) {
    digitalWrite(GATE_PINS[gate_index], HIGH);
    gate_timers[gate_index] = millis() + input_gate_length; // Use measured input gate length
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