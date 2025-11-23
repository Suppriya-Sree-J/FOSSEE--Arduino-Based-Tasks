/*
  PotMatch
  Hardware mapping:
    A0  -> potentiometer
    D2  -> button (INPUT_PULLUP)
    D3  -> LED red 1 (leftmost red)
    D4  -> LED red 2
    D5  -> LED red 3
    D6  -> LED green 1
    D7  -> LED green 2
    D8  -> LED green 3 (rightmost green)
    D9  -> passive buzzer
    LCD I2C at A4(A4=SDA), A5(SCL) -> LiquidCrystal_I2C 0x27 (try 0x3F if no text)
  Behavior:
    - Pot reading accepted only after 500 ms stable
    - Click of a button causes level selection; long press for 800 ms within 2000ms after click will enter
    - Levels 1..5 with timers/tries
    - LEDs reflect state; buzzer + LCD react to LEDs (not raw pot due to objective being simple sensor actuator)
    - Idle: after 36s display goes off, button click wakes again
    - Win: LED rapid blink + win melody for 5000ms then return to welcome 
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // change to 0x3F if your module is different

// pins
const int PIN_BTN    = 2;
const int LED_PINS[6] = {3,4,5,6,7,8}; // 0..5
const int PIN_BUZZER = 9;
const int PIN_POT    = A0;

// timing constants
const unsigned long POT_STABLE_MS = 500UL;
const unsigned long IDLE_TIMEOUT_MS = 36000UL; // 36s
const unsigned long CLICK_WINDOW_MS = 700UL;
const unsigned long LONG_PRESS_MS = 800UL;
const unsigned long WIN_DISPLAY_MS = 5000UL; // 5s

// level configs
struct LevelConfig { unsigned long timerMs; int winWindow; int triesAllowed; };
LevelConfig lvl[6];

// state variables
int selectedLevel = 1;
int unlockedLevel = 1;
int currentScreen = 0; // 0=welcome,1=levelSelect,2=inGame,3=winAnim,4=sleep
unsigned long lastActivity = 0;

// pot stable detection
int lastPotRaw = 0;
unsigned long potChangedAt = 0;
bool potStable = false;
int stableValue = 0;

// game
int roundTarget = 0;
unsigned long levelStartedAt = 0;
int triesLeft = 0;

// button detection non-blocking
int btnState = HIGH;
int btnLast = HIGH;
unsigned long btnChangedAt = 0;
unsigned long btnDownAt = 0;
int clickCount = 0;
unsigned long lastClickTime = 0;

// non-blocking tone system
unsigned long toneEndAt = 0;
int tonePin = PIN_BUZZER;
int toneOnFreq = 0;

// non-blocking melody (win) state
// short melody arrays
const int winNotes[] = {1047,1319,1568,2093};
const int winDur[]   = {160,160,200,340};
const int winCount = sizeof(winNotes)/sizeof(winNotes[0]);
int melodyIndex = 0;
unsigned long melodyNoteEnd = 0;
bool melodyPlaying = false;
unsigned long winAnimEnd = 0;
bool winAnimActive = false;

// LCD optimization
String lcdLine0 = "";
String lcdLine1 = "";

// helper declarations
void initLevels();
void pickNewTarget();
void startLevel(int L);
void stopToWelcome();
void acceptStablePot(int value);
int diffToState(int diff, int winW);
void setLEDsForState(int state);
void triggerLoseTone(int state);
void startToneNB(int freq, unsigned long dur);
void checkToneNB();
void startWinMelodyNonBlocking();
void checkMelody();
void lcdPrintIfChanged(const String &l0, const String &l1);
void updatePot();
void handleButton();
void idleCheck();
void goToSleep();
void wakeFromSleep();

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN, INPUT_PULLUP);
  for (int i=0;i<6;i++) pinMode(LED_PINS[i], OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_POT, INPUT);

  lcd.init();
  lcd.backlight();
  initLevels();
  pickNewTarget(); // random initial target
  lcdPrintIfChanged("Play PotMatch :)","Press to select");
  lastActivity = millis();
}

void loop() {
  unsigned long now = millis();

  handleButton();
  updatePot();
  checkToneNB();
  checkMelody();
  idleCheck();

  // in-game timer management
  if (currentScreen == 2) {
    unsigned long elapsed = now - levelStartedAt;
    unsigned long tm = lvl[selectedLevel].timerMs;
    if (tm > 0 && elapsed >= tm) {
      // time up -> lose
      lcdPrintIfChanged("Time up!","Returning...");
      triggerLoseTone(0); // worst-case tone
      delay(700); // brief blocking to make message audible & visible then move on
      stopToWelcome();
    } else {
      // update status line periodically (but only if changed)
      unsigned long remain = (tm>0) ? (tm - elapsed)/1000 : 0;
      String l0 = String("L") + String(selectedLevel) + " ";
      if (tm>0) l0 += String(remain) + "s ";
      else l0 += "    ";
      l0 += "Tr:" + String(min(triesLeft,99));
      // second line will be set on stable pot accepts, keep l1 same
      lcdPrintIfChanged(l0, lcdLine1);
    }
  }

  // win animation auto-check
  if (winAnimActive && millis() >= winAnimEnd) {
    winAnimActive = false;
    melodyPlaying = false;
    noTone(tonePin);
    // unlock next level if needed
    if (selectedLevel == unlockedLevel && unlockedLevel < 5) unlockedLevel++;
    stopToWelcome();
  }
}

/* -------------------- implementation -------------------- */

void initLevels() {
  lvl[1] = {0UL, 60, 0};      // level1: no timer, wide window
  lvl[2] = {20000UL, 42, 0};  // 25s
  lvl[3] = {15000UL, 36, 0};   // 16s
  lvl[4] = {15000UL, 16, 5};   // 16s, 5 tries
  lvl[5] = {13000UL, 16, 3};   // 13s, 3 tries
  randomSeed(analogRead(A5));
}

void pickNewTarget() {
  roundTarget = random(20, 1003); // avoid physical extremes
}

void startLevel(int L) {
  selectedLevel = constrain(L,1,5);
  pickNewTarget();
  levelStartedAt = millis();
  if (lvl[selectedLevel].triesAllowed > 0) triesLeft = lvl[selectedLevel].triesAllowed;
  else triesLeft = 9999;
  currentScreen = 2;
  // show initial
  String l0 = "Level " + String(selectedLevel);
  String l1;
  if (lvl[selectedLevel].timerMs > 0) l1 = "Timer:" + String(lvl[selectedLevel].timerMs/1000) + "s";
  else l1 = "Find the value";
  lcdPrintIfChanged(l0,l1);
  lastActivity = millis();
  // reset pot stable tracking so user must stop
  potStable = false;
  // reset LEDs
  for (int i=0;i<6;i++) digitalWrite(LED_PINS[i], LOW);
}

void stopToWelcome() {
  currentScreen = 0;
  lcdPrintIfChanged("Play PotMatch :)", "Press to select");
  lastActivity = millis();
}

// pot handling (non-blocking stable detection)
void updatePot() {
  int raw = analogRead(PIN_POT);
  unsigned long now = millis();

  if (abs(raw - lastPotRaw) > 3) {
    potChangedAt = now;
    potStable = false;
    lastPotRaw = raw;
  }

  if (!potStable && (now - potChangedAt >= POT_STABLE_MS)) {
    potStable = true;
    stableValue = lastPotRaw;
    lastActivity = now;
    // only accept stable inputs while in-game
    if (currentScreen == 2) {
      acceptStablePot(stableValue);
    }
  }
}

void acceptStablePot(int value) {
  // compute diff and state
  int diff = abs(value - roundTarget);
  int state = diffToState(diff, lvl[selectedLevel].winWindow);
  // set LEDs (immediately)
  setLEDsForState(state);

  // LCD message per state
  String msg;
  switch(state) {
    case 0: msg = "Way off, try harder!"; break;
    case 1: msg = "Still far, >-<!"; break;
    case 2: msg = "Getting closer :D"; break;
    case 3: msg = "Close!!!!"; break;
    case 4: msg = "Just a little! YAY"; break;
    case 5: msg = "Correct! YAY YAY YAYYY"; break;
    default: msg = "??"; break;
  }
  String l0 = "L" + String(selectedLevel) + " ";
  if (lvl[selectedLevel].timerMs > 0) {
    unsigned long remain = lvl[selectedLevel].timerMs - (millis()-levelStartedAt);
    l0 += String((remain+500)/1000) + "s ";
  }
  String l1 = msg;
  if (state < 5) l1 += " D:" + String(diff);
  else l1 += " Unlocked";

  lcdPrintIfChanged(l0,l1);

  // buzzer reacts to LED-state (not pot raw)
  if (state < 5) {
    triggerLoseTone(state);
    // decrement try if level has tries limit
    if (lvl[selectedLevel].triesAllowed > 0) {
      triesLeft--;
      if (triesLeft <= 0) {
        lcdPrintIfChanged("No tries left","Returning...");
        triggerLoseTone(0);
        delay(700);
        stopToWelcome();
        return;
      }
    }
  } else {
    // WIN - start non-blocking win animation + melody for WIN_DISPLAY_MS
    winAnimActive = true;
    winAnimEnd = millis() + WIN_DISPLAY_MS;
    melodyPlaying = true;
    melodyIndex = 0;
    // start first note
    startToneNB(winNotes[0], winDur[0]);
    melodyNoteEnd = millis() + winDur[0];
    // also begin a fast LED flash pattern by toggling in checkMelody()
    lastActivity = millis();
  }
}

int diffToState(int diff, int winW) {
  if (diff <= winW) return 5;
  if (diff <= winW*2) return 4;
  if (diff <= winW*4) return 3;
  if (diff <= winW*8) return 2;
  if (diff <= winW*15) return 1;
  return 0;
}

void setLEDsForState(int state) {
  // clear
  for (int i=0;i<6;i++) digitalWrite(LED_PINS[i], LOW);
  switch(state) {
    case 0:
      digitalWrite(LED_PINS[0], HIGH);
      digitalWrite(LED_PINS[1], HIGH);
      digitalWrite(LED_PINS[2], HIGH);
      break;
    case 1:
      digitalWrite(LED_PINS[0], HIGH);
      digitalWrite(LED_PINS[1], HIGH);
      break;
    case 2:
      digitalWrite(LED_PINS[1], HIGH);
      break;
    case 3:
      digitalWrite(LED_PINS[3], HIGH);
      break;
    case 4:
      digitalWrite(LED_PINS[3], HIGH);
      digitalWrite(LED_PINS[4], HIGH);
      break;
    case 5:
      digitalWrite(LED_PINS[3], HIGH);
      digitalWrite(LED_PINS[4], HIGH);
      digitalWrite(LED_PINS[5], HIGH);
      break;
  }
}

// non-blocking single tone start
void startToneNB(int freq, unsigned long dur) {
  if (freq <= 0) return;
  tone(tonePin, freq);
  toneOnFreq = freq;
  toneEndAt = millis() + dur;
}

// check & stop tone when time passes
void checkToneNB() {
  if (toneEndAt != 0 && millis() >= toneEndAt) {
    noTone(tonePin);
    toneEndAt = 0;
    toneOnFreq = 0;
  }
}

// lose tones - start non-blocking short tones
void triggerLoseTone(int state) {
  switch(state) {
    case 0: startToneNB(160,250); break;
    case 1: startToneNB(220,200); break;
    case 2: startToneNB(300,150); break;
    case 3: startToneNB(600,120); break;
    case 4: startToneNB(800,100); break;
    default: startToneNB(200,120); break;
  }
}

// melody handling (non-blocking) - called by loop via checkMelody()
void checkMelody() {
  if (!melodyPlaying) return;

  unsigned long now = millis();

  // toggle fast LED blink during melody
  static unsigned long lastBlink = 0;
  static bool blinkState = false;
  if (now - lastBlink >= 120) {
    lastBlink = now;
    blinkState = !blinkState;
    for (int i=0;i<6;i++) digitalWrite(LED_PINS[i], blinkState ? HIGH : LOW);
  }

  // progress notes
  if (now >= melodyNoteEnd) {
    // move to next note
    melodyIndex++;
    if (melodyIndex >= winCount) {
      // if still within winAnimEnd, restart melody (loop) or stop
      if (winAnimActive && millis() < winAnimEnd) {
        melodyIndex = 0;
      } else {
        // stop melody
        melodyPlaying = false;
        noTone(tonePin);
        return;
      }
    }
    // start next note
    startToneNB(winNotes[melodyIndex], winDur[melodyIndex]);
    melodyNoteEnd = now + winDur[melodyIndex];
  }
}

// button handling (click/long press, level selection)
void handleButton() {
  unsigned long now = millis();
  int r = digitalRead(PIN_BTN);

  if (r != btnLast) {
    btnChangedAt = now;
    btnLast = r;
  }
  if (now - btnChangedAt > 20) {
    if (r != btnState) {
      btnState = r;
      if (btnState == LOW) { // pressed
        btnDownAt = now;
        // if sleeping -> wake
        if (currentScreen == 4) {
          wakeFromSleep();
          return;
        }
        // if welcome/level screen, increment click count
        if (currentScreen == 0 || currentScreen == 1) {
          clickCount++;
          lastClickTime = now;
          // show immediate feedback
          lcdPrintIfChanged("Selecting...", "Clicks:" + String(clickCount));
        }
      } else { // released
        unsigned long pressDur = now - btnDownAt;
        if (pressDur >= LONG_PRESS_MS) {
          // long press actions
          if (currentScreen == 1) {
            // long press enters level if unlocked
            if (selectedLevel <= unlockedLevel) {
              startLevel(selectedLevel);
            } else {
              lcdPrintIfChanged("Level locked", "Long press denied");
            }
          } else if (currentScreen == 0) {
            // long press on welcome - reset? (spec allowed long press after idle to reset)
            if (now - lastActivity >= 6000UL) {
              stopToWelcome();
            }
          }
        } else {
          // short press handled via clickCount window below
        }
      }
    }
  }

  // handle click window expiration
  if ((currentScreen == 0 || currentScreen == 1) && clickCount > 0 && (now - lastClickTime > CLICK_WINDOW_MS)) {
    // interpret clicks as level selection number (1..5)
    int choice = clickCount;
    if (choice < 1) choice = 1;
    if (choice > 5) choice = 5;
    selectedLevel = choice;
    currentScreen = 1; // level select screen
    // display chosen level and lock status
    String l1 = (selectedLevel <= unlockedLevel) ? "Long press to enter" : "Level locked";
    lcdPrintIfChanged("Select Level: " + String(selectedLevel), l1);
    clickCount = 0;
    lastActivity = now;
  }
}

// lcd print only when content changed (reduces flicker)
void lcdPrintIfChanged(const String &l0, const String &l1) {
  if (l0 != lcdLine0 || l1 != lcdLine1) {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print(l0);
    lcd.setCursor(0,1);
    lcd.print(l1);
    lcdLine0 = l0;
    lcdLine1 = l1;
  }
}

// idle detection
void idleCheck() {
  if (millis() - lastActivity >= IDLE_TIMEOUT_MS && currentScreen != 4) {
    goToSleep();
  }
}

void goToSleep() {
  lcd.noDisplay(); // hide text (backlight left on)
  currentScreen = 4;
  // keep processing button to wake
}

void wakeFromSleep() {
  lcd.display();
  currentScreen = 0;
  lcdPrintIfChanged("Play PotMatch :)","Press to select");
  lastActivity = millis();
}
