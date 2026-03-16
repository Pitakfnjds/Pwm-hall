// hall_throttle_cytron.ino
// MagLev tlačidlo pre Arduino Nano + Cytron MD30C
// 3-polohový prepínač: 1=dopredu, 0=stop, 2=dozadu
// PWM + DIR ovládanie (sign-magnitude mode)
// OCHRANA: Blokácia reverzu za chodu (anti-plugging)
//
// EEPROM kalibrácia:
//   - Prvý štart: automatická kalibrácia, uloží do EEPROM
//   - Ďalšie štarty: načíta z EEPROM (bez kalibrácie)
//   - Vynútená rekalibrácia: prepínač v polohe 2 pri zapnutí

#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>

// === PINY ===
const int HALL_PIN = A0;      // Hall senzor vstup
const int PWM_PIN = 5;        // PWM výstup (potrebuje 2kΩ pull-down!)
const int DIR_PIN = 4;        // DIR výstup (smer otáčania)
const int NEOPIXEL_PIN = 9;   // NeoPixel DIN
const int LED_PIN = 13;       // Vstavaná LED na Nano
const int RAMP_POT_PIN = A1;       // potenciometer pre nastavenie rampup času

// === PREPÍNAČ (ENABLE + SMER) ===
const int SW_FWD_PIN = 2;     // Poloha 1 = dopredu (A1, červený)
const int SW_REV_PIN = 3;     // Poloha 2 = dozadu (A2, žltý)
                              // COM (A, čierny) → GND

// === NEOPIXEL ===
const int NUM_LEDS = 8;
Adafruit_NeoPixel strip(NUM_LEDS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// === NASTAVITEĽNÉ PARAMETRE ===
const int DEADZONE_LOW = 5;    // Dolná mŕtva zóna [%]
const int DEADZONE_HIGH = 95;  // Horná mŕtva zóna [%]
const int BRIGHTNESS = 50;     // Jas LED (0-255)

// === PWM VÝSTUP ===
// Cytron MD30C: PWM rozsah 0-100%
const int PWM_MIN = 0;         // = 0% (motor stop)
const int PWM_MAX = 255;       // = 100%

// === SMER MOTORA ===
const bool MOTOR_FORWARD = HIGH;
const bool MOTOR_REVERSE = LOW;

// === STAVY PREPÍNAČA ===
enum SwitchState {
    SW_STOP,      // Poloha 0 = stop
    SW_FORWARD,   // Poloha 1 = dopredu
    SW_REVERSE    // Poloha 2 = dozadu
};

// === EEPROM ===
const int EEPROM_ADDR_MAGIC = 0;    // Adresa: magický bajt (1 bajt)
const int EEPROM_ADDR_CAL_MIN = 1;  // Adresa: cal_min (2 bajty, int)
const int EEPROM_ADDR_CAL_MAX = 3;  // Adresa: cal_max (2 bajty, int)
const byte EEPROM_MAGIC = 0xCA;     // "CA" = CAlibration marker

// === KALIBRÁCIA ===
int cal_min = 1023;
int cal_max = 0;
bool calibrated = false;
unsigned long cal_start = 0;
const int CAL_TIME = 5000;  // 5 sekúnd

// === SLEW RATE LIMITER (POSTUPNÝ ROZBEH + DOBEH) ===
unsigned long RAMP_UP_TIME = 800;     // ms z 0% na 100%
const unsigned long RAMP_DOWN_TIME = 800;   // ms zo 100% na 0%
const unsigned long RAMP_MIN_TIME = 200;  // Min. čas pro rozběh/doběh
const unsigned long RAMP_MAX_TIME = 2000; // Max. čas pro rozběh/doběh
int currentOutput = 0;                       // Aktuálny výstup po slew rate limiteri
unsigned long lastLoopTime = 0;              // Pre výpočet delta time

// === OCHRANA PROTI REVERZU ZA CHODU ===
SwitchState activeDirection = SW_STOP;       // Aktuálny aktívny smer motora

// === EEPROM FUNKCIE ===

bool eepromHasCalibration() {
    return EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC;
}

void eepromSaveCalibration(int minVal, int maxVal) {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.put(EEPROM_ADDR_CAL_MIN, minVal);
    EEPROM.put(EEPROM_ADDR_CAL_MAX, maxVal);
}

bool eepromLoadCalibration(int &minVal, int &maxVal) {
    if (!eepromHasCalibration()) return false;
    
    EEPROM.get(EEPROM_ADDR_CAL_MIN, minVal);
    EEPROM.get(EEPROM_ADDR_CAL_MAX, maxVal);
    
    // Kontrola validity (rozumné hodnoty pre 10-bit ADC)
    if (minVal < 0 || minVal > 1023) return false;
    if (maxVal < 0 || maxVal > 1023) return false;
    if (maxVal - minVal < 50) return false;  // Príliš malý rozsah
    
    return true;
}

void eepromClearCalibration() {
    EEPROM.write(EEPROM_ADDR_MAGIC, 0xFF);
}

void setup() {
    // !!! KRITICKÉ: PWM = 0 HNEĎ PRI ŠTARTE !!!
    pinMode(PWM_PIN, OUTPUT);
    analogWrite(PWM_PIN, 0);  // Motor STOP okamžite!
    
    pinMode(DIR_PIN, OUTPUT);
    digitalWrite(DIR_PIN, MOTOR_FORWARD);
    
    pinMode(LED_PIN, OUTPUT);
    
    // Prepínač piny s interným pull-up
    pinMode(SW_FWD_PIN, INPUT_PULLUP);
    pinMode(SW_REV_PIN, INPUT_PULLUP);
    
    Serial.begin(115200);

    // Potenciometer pre nastavenie ramp-up času
    pinMode(RAMP_POT_PIN, INPUT);
    
    // Inicializácia NeoPixel
    strip.begin();
    strip.setBrightness(BRIGHTNESS);
    strip.clear();
    strip.show();
    
    Serial.println();
    Serial.println("===========================================");
    Serial.println("  MagLev Throttle - Cytron MD30C");
    Serial.println("  s 3-polohovym prepinacom + EEPROM");
    Serial.println("===========================================");
    Serial.println();
    Serial.println("Prepinac: 1=DOPREDU, 0=STOP, 2=DOZADU");
    Serial.println("Rekalibracia: prepinac v polohe 2 pri starte");
    Serial.print("Rozbeh: ");
    Serial.print(RAMP_UP_TIME / 1000.0, 1);
    Serial.print("s  Dobeh: ");
    Serial.print(RAMP_DOWN_TIME / 1000.0, 1);
    Serial.println("s");
    Serial.println();

    lastLoopTime = millis();
    
    // Animácia pri štarte
    /*for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 255, 0));
        strip.show();
        delay(100);
    }*/
    delay(200);
    strip.clear();
    strip.show();
    
    // === ROZHODNUTIE: EEPROM alebo KALIBRÁCIA ===
    
    // Prečítaj prepínač pri štarte
    bool forceRecalibrate = (digitalRead(SW_REV_PIN) == LOW);  // Poloha 2
    
    if (forceRecalibrate) {
        Serial.println(">>> Prepinac v polohe 2 = VYNUTENA REKALIBRACIA");
        Serial.println();
        eepromClearCalibration();
        
        // Fialová animácia = rekalibrácia
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < NUM_LEDS; j++) {
                strip.setPixelColor(j, strip.Color(128, 0, 128));
            }
            strip.show();
            delay(200);
            strip.clear();
            strip.show();
            delay(200);
        }
    }
    
    if (!forceRecalibrate && eepromLoadCalibration(cal_min, cal_max)) {
        // EEPROM má platné dáta → použiť
        calibrated = true;
        
        Serial.println("=========== EEPROM KALIBRACIA ===========");
        Serial.print("MIN: ");
        Serial.print(cal_min);
        Serial.print(" (");
        Serial.print(cal_min * 5.0 / 1023.0, 3);
        Serial.println(" V)");
        Serial.print("MAX: ");
        Serial.print(cal_max);
        Serial.print(" (");
        Serial.print(cal_max * 5.0 / 1023.0, 3);
        Serial.println(" V)");
        Serial.print("Rozsah ADC: ");
        Serial.println(cal_max - cal_min);
        Serial.println("==========================================");
        Serial.println();
        Serial.println("Pripraveny! (Pre rekalibraciu: prepni na 2 a restartuj)");
        Serial.println();
        
        // Zelený blik = hotovo
        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < NUM_LEDS; j++) {
                strip.setPixelColor(j, strip.Color(0, 255, 0));
            }
            strip.show();
            delay(150);
            strip.clear();
            strip.show();
            delay(150);
        }
        
    } else {
        // Žiadna kalibrácia v EEPROM → spustiť kalibráciu
        calibrated = false;
        cal_min = 1023;
        cal_max = 0;
        cal_start = millis();
        
        Serial.println("-------------------------------------------");
        Serial.println("KALIBRACIA: Stlacaj tlacidlo 5 sekund!");
        Serial.println("-------------------------------------------");
        Serial.println();
    }
}

// Čítanie ramp-up času z potenciometra alpha je sila filtra
unsigned long readRampTime() {
    static bool initialized = false;
    static float filtered;
    int raw = analogRead(POT_PIN);
    if (!initialized) {
        filtered = raw;
        initialized = true;
    }
    const float alpha = 0.12;
    filtered += alpha * (raw - filtered);
    unsigned long ramp = map((int)filtered, 0, 1023, RAMP_TIME_MIN, RAMP_TIME_MAX);
    return constrain(ramp, RAMP_TIME_MIN, RAMP_TIME_MAX);
}

// Čítanie stavu prepínača
SwitchState readSwitch() {
    bool fwd = digitalRead(SW_FWD_PIN);  // LOW = aktívne (COM spojený)
    bool rev = digitalRead(SW_REV_PIN);  // LOW = aktívne (COM spojený)
    
    if (fwd == LOW && rev == HIGH) {
        return SW_FORWARD;  // Poloha 1
    } else if (fwd == HIGH && rev == LOW) {
        return SW_REVERSE;  // Poloha 2
    } else {
        return SW_STOP;     // Poloha 0 (alebo neurčitý stav)
    }
}

// Farba podľa percenta (zelená → oranžová → červená)
uint32_t getColor(int percent) {
    int r, g, b;
    
    if (percent <= 50) {
        r = map(percent, 0, 50, 0, 255);
        g = map(percent, 0, 50, 255, 165);
        b = 0;
    } else {
        r = 255;
        g = map(percent, 50, 100, 165, 0);
        b = 0;
    }
    
    return strip.Color(r, g, b);
}

// Zobrazenie na NeoPixel páse
void showOnStrip(int percent, SwitchState state) {
    // Ak je STOP, ukáž modrú
    if (state == SW_STOP) {
        for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, strip.Color(0, 0, 50));  // Modrá = STOP
        }
        strip.show();
        return;
    }
    
    int numLit = (percent * NUM_LEDS) / 100;
    int partial = ((percent * NUM_LEDS) % 100) * 255 / 100;
    uint32_t color = getColor(percent);
    
    // Ak je REVERSE, ukáž fialovú namiesto zelenej-červenej
    if (state == SW_REVERSE) {
        color = strip.Color(128, 0, 128);  // Fialová = dozadu
    }
    
    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < numLit) {
            strip.setPixelColor(i, color);
        } else if (i == numLit && partial > 0) {
            int r = ((color >> 16) & 0xFF) * partial / 255;
            int g = ((color >> 8) & 0xFF) * partial / 255;
            int b = (color & 0xFF) * partial / 255;
            strip.setPixelColor(i, strip.Color(r, g, b));
        } else {
            strip.setPixelColor(i, 0);
        }
    }
    
    strip.show();
}

// Kalibračná animácia
void showCalibration() {
    static bool toggle = false;
    toggle = !toggle;
    
    digitalWrite(LED_PIN, toggle ? HIGH : LOW);
    
    for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, toggle ? strip.Color(0, 0, 100) : 0);
    }
    strip.show();
}

// Nastavenie PWM výstupu
void setPWMOutput(int percent) {
    int pwmValue = map(percent, 0, 100, PWM_MIN, PWM_MAX);
    pwmValue = constrain(pwmValue, PWM_MIN, PWM_MAX);
    analogWrite(PWM_PIN, pwmValue);
}

void loop() {
    // Čítaj ADC (priemer z 10 vzoriek)
    long sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(HALL_PIN);
        delayMicroseconds(100);
    }
    int raw = sum / 10;
    
    // Čítaj stav prepínača
    SwitchState switchState = readSwitch();
    
    // === KALIBRÁCIA ===
    if (!calibrated) {
        if (raw < cal_min) cal_min = raw;
        if (raw > cal_max) cal_max = raw;
        
        int remaining = (CAL_TIME - (millis() - cal_start)) / 1000 + 1;
        
        showCalibration();
        setPWMOutput(0);  // Motor STOP počas kalibrácie
        
        Serial.print("Kalibracia... ");
        Serial.print(remaining);
        Serial.print("s | MIN:");
        Serial.print(cal_min);
        Serial.print(" MAX:");
        Serial.print(cal_max);
        Serial.print(" | Teraz:");
        Serial.println(raw);
        
        if (millis() - cal_start >= CAL_TIME) {
            int range = cal_max - cal_min;
            
            if (range < 50) {
                Serial.println();
                Serial.println("!!! VAROVANIE: Maly rozsah !!!");
                Serial.println("Stlacal si tlacidlo pocas kalibracie?");
                Serial.println("Restartujem kalibraciu...");
                Serial.println();
                
                // Červený blik = chyba
                for (int i = 0; i < 5; i++) {
                    for (int j = 0; j < NUM_LEDS; j++) {
                        strip.setPixelColor(j, strip.Color(255, 0, 0));
                    }
                    strip.show();
                    delay(100);
                    strip.clear();
                    strip.show();
                    delay(100);
                }
                
                // Reset kalibrácie a skús znova
                cal_min = 1023;
                cal_max = 0;
                cal_start = millis();
                return;
            }
            
            calibrated = true;
            
            // Uložiť do EEPROM
            eepromSaveCalibration(cal_min, cal_max);
            
            // Potvrdenie - zelený blik
            digitalWrite(LED_PIN, LOW);
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < NUM_LEDS; j++) {
                    strip.setPixelColor(j, strip.Color(0, 255, 0));
                }
                strip.show();
                delay(150);
                strip.clear();
                strip.show();
                delay(150);
            }
            
            Serial.println();
            Serial.println("====== KALIBRACIA OK + ULOZENA ======");
            Serial.print("MIN: ");
            Serial.print(cal_min);
            Serial.print(" (");
            Serial.print(cal_min * 5.0 / 1023.0, 3);
            Serial.println(" V)");
            Serial.print("MAX: ");
            Serial.print(cal_max);
            Serial.print(" (");
            Serial.print(cal_max * 5.0 / 1023.0, 3);
            Serial.println(" V)");
            Serial.print("Rozsah ADC: ");
            Serial.println(range);
            Serial.println("Ulozene do EEPROM!");
            Serial.println("======================================");
            Serial.println();
        }
        
        delay(200);
        return;
    }
    
    // === NORMÁLNA PREVÁDZKA ===
    
    // Mapovanie na 0-100%
    int raw_percent = map(raw, cal_min, cal_max, 0, 100);
    raw_percent = constrain(raw_percent, 0, 100);
    
    // Aplikácia deadzone
    int throttle;
    if (raw_percent <= DEADZONE_LOW) {
        throttle = 0;
    } else if (raw_percent >= DEADZONE_HIGH) {
        throttle = 100;
    } else {
        throttle = map(raw_percent, DEADZONE_LOW, DEADZONE_HIGH, 0, 100);
    }
    
    // === URČENIE CIEĽOVEJ HODNOTY PODĽA PREPÍNAČA ===
    // Koliskový prepínač: 1(FWD) ↔ 0(STOP) ↔ 2(REV), vždy cez STOP
    int target = 0;
    bool direction = (activeDirection == SW_REVERSE) ? MOTOR_REVERSE : MOTOR_FORWARD;
    const char* stateStr = "STOP";
    bool reverseBlocked = false;

    // Detekcia pokusu o zmenu smeru (FWD↔REV, vždy prechádza cez STOP)
    bool oppositeRequested = (switchState == SW_FORWARD && activeDirection == SW_REVERSE) ||
                              (switchState == SW_REVERSE && activeDirection == SW_FORWARD);

    if (oppositeRequested) {
        if (currentOutput == 0) {
            // Motor stojí → bezpečná zmena smeru
            activeDirection = switchState;
            direction = (switchState == SW_REVERSE) ? MOTOR_REVERSE : MOTOR_FORWARD;
            target = throttle;
            stateStr = (switchState == SW_FORWARD) ? "FWD " : "REV ";
        } else {
            // Motor ešte dobieha → čakáme na zastavenie
            target = 0;
            reverseBlocked = true;
            stateStr = "BRK!";
        }
    } else if (switchState == SW_STOP) {
        target = 0;
        stateStr = "STOP";
    } else {
        // Normálna prevádzka (FWD alebo REV)
        direction = (switchState == SW_REVERSE) ? MOTOR_REVERSE : MOTOR_FORWARD;
        activeDirection = switchState;
        target = throttle;
        stateStr = (switchState == SW_FORWARD) ? "FWD " : "REV ";
    }

    unsigned long rampTime = readRampTime();
    RAMP_UP_TIME = rampTime;
    // === SLEW RATE LIMITER ===
    unsigned long now = millis();
    unsigned long dt = now - lastLoopTime;
    lastLoopTime = now;

    if (currentOutput < target) {
        // Rozbeh — stúpanie k cieľu
        int maxIncrease = (int)((long)100 * dt / RAMP_UP_TIME);
        if (maxIncrease < 1) maxIncrease = 1;
        currentOutput += maxIncrease;
        if (currentOutput > target) currentOutput = target;
    } else if (currentOutput > target) {
        // Dobeh — klesanie k cieľu
        int maxDecrease = (int)((long)100 * dt / RAMP_DOWN_TIME);
        if (maxDecrease < 1) maxDecrease = 1;
        currentOutput -= maxDecrease;
        if (currentOutput < target) currentOutput = target;
    }

    int output = currentOutput;

    // Nastav výstupy
    setPWMOutput(output);
    digitalWrite(DIR_PIN, direction);

    // Zobraz na NeoPixel
    if (reverseBlocked) {
        // Varovné blikanie - oranžová = čakanie na zastavenie motora
        bool blink = (millis() / 150) % 2;
        for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, blink ? strip.Color(255, 100, 0) : 0);
        }
        strip.show();
    } else {
        // Pri STOP s dobehom (output > 0) ukáž klesajúci výstup v farbe posledného smeru
        SwitchState displayState = switchState;
        if (switchState == SW_STOP && output > 0) {
            displayState = activeDirection;
        }
        showOnStrip(output, displayState);
    }
    
    // LED indikátor
    if (switchState == SW_STOP) {
        // Blikaj pri STOP
        static unsigned long lastBlink = 0;
        static bool blinkState = false;
        if (millis() - lastBlink > 500) {
            blinkState = !blinkState;
            lastBlink = millis();
        }
        digitalWrite(LED_PIN, blinkState ? HIGH : LOW);
    } else {
        // Svieti pri > 50%
        digitalWrite(LED_PIN, output > 50 ? HIGH : LOW);
    }
    
    // Výpis do konzoly
    int pwmValue = map(output, 0, 100, PWM_MIN, PWM_MAX);
    float duty_cycle = pwmValue * 100.0 / 255.0;
    
    Serial.print("[");
    Serial.print(stateStr);
    Serial.print("] ");
    Serial.print(output);
    Serial.print("%");
    if (output != target) {
        Serial.print(output < target ? "^" : "v");
        Serial.print(target);
        Serial.print("%");
    }
    Serial.print("\t-> PWM:");
    Serial.print(pwmValue);
    Serial.print("/255 (");
    Serial.print(duty_cycle, 1);
    Serial.print("%)");
    Serial.print("\t(ADC:");
    Serial.print(raw);
    Serial.println(")");
    
    delay(50);
}
