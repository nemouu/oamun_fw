/*
 * Trigger Length Burst Generator for OAM Uncertainty
 * 
 * Gates 1-3: Always fire bursts (foundation)
 * Gates 4-7: Probability-based bursts  
 * 
 * Control: Gate length determines burst speed
 * - Short gates (10-50ms) = Fast bursts (80ms intervals)
 * - Long gates (200ms+) = Slow bursts (250ms intervals)
 */

#define CV_INPUT_PIN 26
const int GATE_PINS[8] = {27, 28, 29, 0, 3, 4, 2, 1};

// Configuration
const int BURST_COUNTS[8] = {2, 3, 4, 5, 6, 7, 8, 12}; // 8 outputs
const int BASE_PROBABILITIES[8] = {100, 100, 100, 50, 30, 15, 8, 3}; // Clean, predictable probabilities
const int TRIGGER_THRESHOLD = 1000;
const int GATE_PULSE_LENGTH = 15;

// Trigger length detection
const int MIN_BURST_INTERVAL = 80;   // Fast bursts for short gates
const int MAX_BURST_INTERVAL = 250;  // Slow bursts for long gates
const int MIN_GATE_LENGTH = 10;      // Minimum detectable gate length
const int MAX_GATE_LENGTH = 300;     // Maximum gate length to consider

// State variables
bool trigger_active = false;
unsigned long trigger_start_time = 0;
unsigned long trigger_length = 0;
int current_burst_interval = MIN_BURST_INTERVAL;
bool burst_triggered = false;

// Burst state
struct BurstState {
  bool active;
  int count;
  unsigned long next_time;
  unsigned long gate_off_time;
};

BurstState bursts[8]; // All 8 outputs now

void setup() {
  Serial.begin(115200);
  
  pinMode(CV_INPUT_PIN, INPUT);
  for (int i = 0; i < 8; i++) {
    pinMode(GATE_PINS[i], OUTPUT);
    digitalWrite(GATE_PINS[i], LOW);
  }
  
  // Initialize burst states
  for (int i = 0; i < 8; i++) { // All 8 outputs
    bursts[i].active = false;
    bursts[i].count = 0;
    bursts[i].next_time = 0;
    bursts[i].gate_off_time = 0;
  }
  
  randomSeed(analogRead(A0));
  
  Serial.println("Trigger Length Burst Generator");
  Serial.println("Short gates = Fast bursts | Long gates = Slow bursts");
  Serial.println("Gates 1-3: Always | Gates 4-8: Probability-based");
}

void loop() {
  unsigned long now = millis();
  int cv_value = analogRead(CV_INPUT_PIN);
  bool gate_high = cv_value > TRIGGER_THRESHOLD;
  
  // Detect gate start/end and measure length
  detectGateLength(gate_high, now);
  
  // Process active bursts
  for (int i = 0; i < 8; i++) { // All 8 outputs
    processBurst(i, now);
  }
  
  delay(1);
}

void detectGateLength(bool gate_high, unsigned long now) {
  if (gate_high && !trigger_active) {
    // Gate just went high - start measuring
    trigger_active = true;
    trigger_start_time = now;
    burst_triggered = false;
    
    Serial.println("Gate START");
    
  } else if (!gate_high && trigger_active) {
    // Gate just went low - calculate length and trigger bursts
    trigger_active = false;
    trigger_length = now - trigger_start_time;
    
    // Map gate length to burst interval
    current_burst_interval = map(trigger_length, MIN_GATE_LENGTH, MAX_GATE_LENGTH, 
                                MIN_BURST_INTERVAL, MAX_BURST_INTERVAL);
    current_burst_interval = constrain(current_burst_interval, MIN_BURST_INTERVAL, MAX_BURST_INTERVAL);
    
    Serial.print("Gate END - Length: ");
    Serial.print(trigger_length);
    Serial.print("ms -> Burst interval: ");
    Serial.print(current_burst_interval);
    Serial.println("ms");
    
    // Trigger the bursts based on gate length
    startAllBursts(now);
  }
  
  // Update current trigger length while gate is active
  if (trigger_active) {
    trigger_length = now - trigger_start_time;
  }
}

void startAllBursts(unsigned long now) {
  Serial.print("Gate length: ");
  Serial.print(trigger_length);
  Serial.print("ms -> Burst interval: ");
  Serial.print(current_burst_interval);
  Serial.println("ms");
  
  for (int i = 0; i < 8; i++) {
    bool should_start = false;
    
    if (i < 3) {
      // Gates 1-3: Always fire
      should_start = true;
      Serial.print("Gate ");
      Serial.print(i + 1);
      Serial.println(": Always -> GO");
    } else {
      // Gates 4-8: Fixed probability
      int probability = BASE_PROBABILITIES[i]; // No boost, just pure probability
      int roll = random(100);
      should_start = (roll < probability);
      
      Serial.print("Gate ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(probability);
      Serial.print("% (rolled ");
      Serial.print(roll);
      Serial.print(") -> ");
      Serial.println(should_start ? "GO" : "skip");
    }
    
    if (should_start) {
      bursts[i].active = true;
      bursts[i].count = 0;
      bursts[i].next_time = now;
    }
  }
}

void processBurst(int gate_index, unsigned long now) {
  BurstState* burst = &bursts[gate_index];
  
  // Turn off gate if pulse time is up
  if (burst->gate_off_time > 0 && now >= burst->gate_off_time) {
    digitalWrite(GATE_PINS[gate_index], LOW);
    burst->gate_off_time = 0;
  }
  
  // Check if it's time for next pulse in the burst
  if (burst->active && now >= burst->next_time) {
    // Fire the gate
    digitalWrite(GATE_PINS[gate_index], HIGH);
    burst->gate_off_time = now + GATE_PULSE_LENGTH;
    burst->count++;
    
    // Check if burst is complete
    if (burst->count >= BURST_COUNTS[gate_index]) {
      burst->active = false;
    } else {
      // Schedule next pulse using the interval determined by gate length
      burst->next_time = now + current_burst_interval;
    }
  }
}