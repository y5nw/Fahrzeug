/* Motoren */
#define MOTOR_A 5
#define MOTOR_B 6
#define FORWARD(x) (x)
#define REVERSE(x) (-(x))
#define BRAKE 0

/* Sensoren */
#define PIR  2
#define ECHO 4
#define TRIG 3

/* LEDs */
#define HLIGHT  7  
#define BLIGHT  8  /* Bremsindikator */
#define RLIGHT1 12 /* 1. Rueckfahrtsindikator */
#define RLIGHT2 13 /* 2. Rueckfahrtsindikator */

/* Steuerung */
#define C_MANUAL 0x1
#define C_REVERSE 0x2

/* Anderes */
#define NOP {} /* No-op (nichts tun) */
#define FAR 4500

/* "Stack" (als eine Liste) */
struct stackEntry {
  int value;
  struct stackEntry *next;
};
typedef struct stackEntry stackEntry;
static stackEntry *STACK_HEAD = NULL;

static int s_checkdepth(unsigned int depth) {
  stackEntry *p = STACK_HEAD;
  while (depth--) {
    if (p == NULL)
      return 0;
    p = p->next;
  }
  return 1;
}

static int s_peekint() {
  if (STACK_HEAD == NULL)
    return 0;
  return STACK_HEAD->value;
}

static int s_popint() {
  if (STACK_HEAD == NULL)
    return 0;
  stackEntry *p = STACK_HEAD;
  int n = p->value;
  STACK_HEAD = p->next;
  free(p);
  return n;
}

static int s_pushint(const int n) {
  stackEntry *p = (stackEntry*) malloc(sizeof(stackEntry));
  if (p == NULL) return -1; /* Fehler */
  p->value = n;
  p->next = STACK_HEAD;
  STACK_HEAD = p;
  return 0;
}

static void s_pop(unsigned int depth) {
  stackEntry *p = STACK_HEAD;
  stackEntry *q;
  while (depth-- && p) {
    q = p;
    p = p->next;
    free(q);
  }
  STACK_HEAD = p;
}

/* Diese Funktion stellt den Motor.
 * Wenn mode > 0, faehrt das Fahrzeug vorwaerts.
 * Wenn mode < 0, faehrt das Fahrzeug rueckwaerts.
 * Wenn mode == 0, haelt das Fahrzeug an.
 */
static void setMotor(const int mode) {
  analogWrite(MOTOR_A, mode>0?mode:0);
  analogWrite(MOTOR_B, mode<0?-mode:0);
  digitalWrite(BLIGHT, mode==0);
  digitalWrite(RLIGHT1, mode<0);
  digitalWrite(RLIGHT2, mode<0);
}

/* Diese Funktion gibt den (meistens gemessenen) Abstand zwischen dem Fahrzeug
 * und dem naehsten Objekt zurueck. Die geschwindigkeit des Fahrzeugs wird
 * hier nicht beruecksichtigt.
 */
static unsigned int getDistance() {
  /* Wenn 0 gemessen wird, wird der Wert von "last" zurueckgegeben. */
  static unsigned int last = FAR;
  // Hier wird das Signal gesendet
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(50);
  digitalWrite(TRIG, LOW);
  /* Hier wartet das Fahrzeug, bis es ein Signal bekommt.
   * Wenn nach 25ms kein Signal kommt, dann hoert das Fahrzeug auf, auf das
   * Signal zu warten. In diesem Fall gibt die Funktion 0 zurueck.
   */
  unsigned int s = pulseIn(ECHO, HIGH, 25000)*17UL/100;
  if (s==0) {
    /* Wenn die Messung nur einmal null ist,
     * a) handelt es sich um eine falsche Messung, oder
     * b) ist das Fahrzeug zu weit von einem Objekt.
     * Wenn die Messung nur einmal null ist, dann ist a) wahrscheinlich der
     * Fall. Bei b) wird null von pulseIn staendig zurueckgegeben.
     */
    s = last;
    last = FAR;
  } else {
    last = s;
  }
  return s;
}

void setup() {
  pinMode(MOTOR_A, OUTPUT);
  pinMode(MOTOR_B, OUTPUT);

  pinMode(PIR,  INPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(HLIGHT, OUTPUT);
  pinMode(BLIGHT, OUTPUT);
  pinMode(RLIGHT1, OUTPUT);
  pinMode(RLIGHT2, OUTPUT);

  Serial.begin(9600);
  /* Warten, bis eine Bewegung gemeldet wird */
  while (digitalRead(PIR) == LOW) NOP;
  digitalWrite(HLIGHT, HIGH);
}

void loop() {
  static unsigned int threshold = 100;
  static unsigned int speed = 255;
  static unsigned int mode = 0;

  /* Wenn es einen Befehl gibt, soll der Befehl gelesen werden */
  while (Serial.available()) {
    char c = Serial.peek(); /* Es gibt leider keine ungetc() fuer Arduino */
    if (isdigit(c)) {
      int n = Serial.parseInt(SKIP_ALL);
      s_pushint(n);
      continue;
    } else {
      Serial.read();
      switch(c) {
        case 'D': {
          Serial.print("Modus: ");
          Serial.println(mode);
          Serial.print("Zielgeschwindigkeit: ");
          Serial.println(speed);
          Serial.println(F("=== Stack ==="));
          for (stackEntry *p = STACK_HEAD; p != NULL; p = p->next)
            Serial.println(p->value);
          Serial.println(F("=== Ende ==="));
          break;
        }
        case 'M': {
          if (s_checkdepth(1)) {
            unsigned int m = s_popint();
            mode = m;
          }
          break;
        }
        case 'S': {
          if (s_checkdepth(1)) {
            unsigned int s = s_popint();
            speed = s%256;
          }
          break;
        }
        case 'T': {
          Serial.print(F("Der Grenzwert fuer den Abstandskontrolle wurde "));
          if (s_checkdepth(1)) {
            int t = s_popint();
            if (t < 0 || t >= 4500) {
              Serial.println(F("wegen ungueltiger Eingabe nicht geaendert"));
            } else if (t == 0) {
              Serial.println(F("zurueckgesetzt"));
              threshold = 100;
            } else {
              threshold = t;
              Serial.print(F("auf "));
              Serial.print(t);
              Serial.println(F("mm gesetzt"));
            }
          } else {
            Serial.println(F("wegen fehlender Eingabe nicht geaendert"));
          }
          break;
        }
      }
    }
  }

  if (mode&C_MANUAL) {
    setMotor(mode&C_REVERSE?REVERSE(speed):FORWARD(speed));
  } else {
    unsigned int s = getDistance();
    if (s <= 9*threshold/10) {
      setMotor(REVERSE(speed));
    } else if (s <= 11*threshold/10) {
      setMotor(BRAKE);
    } else {
      setMotor(FORWARD(speed));
    }
  }
  delay(50);
}
