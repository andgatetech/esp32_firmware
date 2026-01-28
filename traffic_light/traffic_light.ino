#define RED_LED     16
#define YELLOW_LED  17
#define GREEN_LED   18
#define BUZZER_PIN  19

// Timing (milliseconds)
#define RED_TIME     5000
#define YELLOW_TIME  2000
#define GREEN_TIME   5000

enum State {
  RED,
  YELLOW1,
  GREEN,
  YELLOW2
};

State currentState = RED;
unsigned long lastTime = 0;

void allOff() {
  digitalWrite(RED_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(RED_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  allOff();

  // Initial state
  digitalWrite(RED_LED, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);

  Serial.println("STATE: RED | BUZZER: ON");

  lastTime = millis();
}

void loop() {
  unsigned long now = millis();

  switch (currentState) {

    case RED:
      if (now - lastTime >= RED_TIME) {
        allOff();
        digitalWrite(YELLOW_LED, HIGH);
        Serial.println("STATE: YELLOW | BUZZER: OFF");
        currentState = YELLOW1;
        lastTime = now;
      }
      break;

    case YELLOW1:
      if (now - lastTime >= YELLOW_TIME) {
        allOff();
        digitalWrite(GREEN_LED, HIGH);
        Serial.println("STATE: GREEN | BUZZER: OFF");
        currentState = GREEN;
        lastTime = now;
      }
      break;

    case GREEN:
      if (now - lastTime >= GREEN_TIME) {
        allOff();
        digitalWrite(YELLOW_LED, HIGH);
        Serial.println("STATE: YELLOW | BUZZER: OFF");
        currentState = YELLOW2;
        lastTime = now;
      }
      break;

    case YELLOW2:
      if (now - lastTime >= YELLOW_TIME) {
        allOff();
        digitalWrite(RED_LED, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);
        Serial.println("STATE: RED | BUZZER: ON");
        currentState = RED;
        lastTime = now;
      }
      break;
  }
}
