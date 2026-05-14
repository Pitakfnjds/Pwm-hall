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
#include <avr/wdt.h>

// === KONFIGURÁCIA POTENCIOMETROV ===
// Zakomentuj #define ak potenciometer NIE JE fyzicky pripojený.
// Keď je zakomentovaný, použije sa DEFAULT konštanta nižšie.

//#define USE_RAMPUP_POT      // A1: potenciometer pre ramp-up čas
//#define USE_PWM_LIMIT_POT   // A2: potenciometer pre PWM limiter

// === DEFAULT HODNOTY (keď pot nie je pripojený) ===
const unsigned long DEFAULT_RAMP_UP_TIME = 2000;   // 2 sekundy (0→100%)
const int DEFAULT_MAX_PWM_PERCENT = 70;            // 70% — strop pre target

// === PINY ===
const int HALL_PIN = A0;      // Hall senzor vstup (HW: R6 10kΩ pull-down + C5 100nF RC filter na PCB2)
const int PWM_PIN = 5;        // PWM výstup (HW: R4||R5 = 2× 10kΩ pull-down na PCB2)
const int DIR_PIN = 4;        // DIR výstup (smer otáčania)
const int NEOPIXEL_PIN = 9;   // NeoPixel DIN
const int LED_PIN = 13;       // Vstavaná LED na Nano
const int RAMP_POT_PIN = A1;       // potenciometer pre nastavenie rampup času
const int PWM_LIMIT_POT = A2;      // potenciometer pre PWM limiter (strop výstupu)

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
const int HALL_MARGIN = 30;    // ADC tolerancia za hranicami kalibrácie pred fault

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
const int EEPROM_ADDR_CRC = 5;      // Adresa: XOR checksum cal-dát (1 bajt)
const byte EEPROM_MAGIC = 0xCA;     // "CA" = CAlibration marker

// === KALIBRÁCIA ===
int cal_min = 1023;
int cal_max = 0;
bool calibrated = false;
unsigned long cal_start = 0;
const int CAL_TIME = 5000;  // 5 sekúnd

// === SLEW RATE LIMITER (LINEÁRNY ROZBEH + EXPONENCIÁLNY DOBEH) ===
unsigned long RAMP_UP_TIME = DEFAULT_RAMP_UP_TIME;  // ms z 0% na 100% (lineárny ramp-up)
float currentOutput = 0.0f;                // Aktuálny výstup po slew rate limiteri [%] — float kvôli presnosti ramp-u
unsigned long lastLoopTime = 0;            // Pre výpočet delta time

// Exponenciálny dobeh — SPOJITÁ exponenciála: currentOutput *= DECAY^(dt/INTERVAL_MS)
// Pri DECAY=0.85 a INTERVAL=300ms: 80% → 10% za ~3.9 s, 100% → 0 za ~5.4 s
const float RAMP_DOWN_DECAY = 0.85f;
const unsigned long RAMP_DOWN_INTERVAL_MS = 300;

// PWM limiter — strop pre target (aplikuje sa PRED slew rate limiterom)
int maxAllowedThrottle = DEFAULT_MAX_PWM_PERCENT;  // 0–100%, prepisuje sa v loop()

// === OCHRANA PROTI REVERZU ZA CHODU ===
SwitchState activeDirection = SW_STOP;       // Aktuálny aktívny smer motora

// === DETEKCIA PORUCHY HALL SENZORA ===
// Latch — ak raz ADC vyletí mimo platný rozsah, motor sa zablokuje až do
// reštartu. Záchytí short na +5V/GND, odpadnutý vodič na rail.
bool sensorFault = false;

// === EEPROM FUNKCIE ===

bool eepromHasCalibration() {
    return EEPROM.read(EEPROM_ADDR_MAGIC) == EEPROM_MAGIC;
}

// XOR checksum cal-dát — chytí náhodné bit-flipy aj 1/256 false-positive
// magic-bytu pri neinicializovanej EEPROM (kde by sa náhodne nakreslilo 0xCA).
byte calculateChecksum(int minVal, int maxVal) {
    uint16_t a = (uint16_t)minVal;
    uint16_t b = (uint16_t)maxVal;
    return (byte)(a ^ (a >> 8) ^ b ^ (b >> 8));
}

void eepromSaveCalibration(int minVal, int maxVal) {
    EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
    EEPROM.put(EEPROM_ADDR_CAL_MIN, minVal);
    EEPROM.put(EEPROM_ADDR_CAL_MAX, maxVal);
    EEPROM.write(EEPROM_ADDR_CRC, calculateChecksum(minVal, maxVal));
}

bool eepromLoadCalibration(int &minVal, int &maxVal) {
    if (!eepromHasCalibration()) return false;

    EEPROM.get(EEPROM_ADDR_CAL_MIN, minVal);
    EEPROM.get(EEPROM_ADDR_CAL_MAX, maxVal);

    // CRC verifikácia PRED range checkmi — chytí bit-flip aj falošný magic.
    byte storedCrc = EEPROM.read(EEPROM_ADDR_CRC);
    if (storedCrc != calculateChecksum(minVal, maxVal)) return false;

    // Kontrola validity (rozumné hodnoty pre 10-bit ADC)
    if (minVal < 0 || minVal > 1023) return false;
    if (maxVal < 0 || maxVal > 1023) return false;
    if (maxVal - minVal < 50) return false;  // Príliš malý rozsah

    return true;
}

void setup() {
    // !!! KRITICKÉ: WDT vypnúť hneď po reštarte !!!
    // Po WDT-reset môže byť watchdog stále zapnutý — ak je timeout
    // kratší ako bootloader (Old Bootloader Optiboot), vznikne reset loop.
    // wdt_disable() musí byť absolútne prvý, pred akoukoľvek prácou.
    wdt_disable();

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

    // Potenciometre (A1 = ramp-up, A2 = PWM limiter)
    pinMode(RAMP_POT_PIN, INPUT);
    pinMode(PWM_LIMIT_POT, INPUT);
    
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
    Serial.println();
    Serial.println("=== KONFIGURACIA ===");
#ifdef USE_RAMPUP_POT
    Serial.println("Ramp-up: POT A1 (2-4s)");
#else
    Serial.print("Ramp-up: FIXNA ");
    Serial.print(DEFAULT_RAMP_UP_TIME);
    Serial.println("ms");
#endif
#ifdef USE_PWM_LIMIT_POT
    Serial.println("PWM limit: POT A2 (30-100%)");
#else
    Serial.print("PWM limit: FIXNA ");
    Serial.print(DEFAULT_MAX_PWM_PERCENT);
    Serial.println("%");
#endif
    Serial.println("Ramp-down: EXP spojita (DECAY=0.85, INTERVAL=300ms, ~5.4s plne->0)");
    Serial.println();

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
        // NEMAŽEME EEPROM teraz — ak user vypne počas cal alebo cal zlyhá,
        // stará kalibrácia ostane platná. Nová cal ju prepíše atomicky cez
        // eepromSaveCalibration() až po úspešnom dokončení.

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

    // Resetuj dt-base AŽ TERAZ — všetky štartové animácie (cal blik, EEPROM
    // green/fialova) sú za nami. Inak by prvá loop() iter dostala dt=600-1400ms
    // a slew-rate limiter by skočil výstup, ak by user držal plyn pri zapnutí.
    lastLoopTime = millis();

    // Zapni watchdog — od teraz loop() musí volať wdt_reset() pred uplynutím 2s,
    // inak sa MCU reštartuje (a PWM=0 znova na začiatku setup()).
    // WDTO_2S (nie 1s) je úmyselne — pri Old Bootloader (ATmegaBOOT, ~1s timeout)
    // by tesné WDTO_1S mohlo skončiť v reset-loope (race s bootloader handoff).
    wdt_enable(WDTO_2S);
}

// Čítanie ramp-up času z potenciometra A1 (2 – 4 s, IIR vyhladené)
unsigned long readRampTime() {
    static bool initialized = false;
    static float filtered;
    int raw = analogRead(RAMP_POT_PIN);
    if (!initialized) {
        filtered = raw;
        initialized = true;
    }
    filtered += 0.12f * (raw - filtered);
    return map((int)filtered, 0, 1023, 2000, 4000);
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
    
    // Škálovanie na aktuálny PWM limit — 100% LED stĺpca = maxAllowedThrottle.
    // Vďaka tomu jazdec aj pri zníženom limite vidí "plnú červenú" pri max. dosiahnuteľnom
    // výkone a vie rozlíšiť, že obmedzovač (pot A2 / DEFAULT_MAX_PWM_PERCENT) je aktívny.
    int displayPercent;
    if (maxAllowedThrottle > 0) {
        displayPercent = (int)((long)percent * 100 / maxAllowedThrottle);
        if (displayPercent > 100) displayPercent = 100;
    } else {
        displayPercent = 0;
    }

    int numLit = (displayPercent * NUM_LEDS) / 100;
    int partial = ((displayPercent * NUM_LEDS) % 100) * 255 / 100;
    uint32_t color = getColor(displayPercent);

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
    // Watchdog reset — musí padnúť pred uplynutím WDTO_2S, inak MCU reštart.
    wdt_reset();

    // Čítaj ADC (priemer z 10 vzoriek). analogRead() sám trvá ~104µs (13 ADC
    // clockov pri prescaler 128), S/H acquisition je interná — žiadny extra
    // delay netreba.
    long sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += analogRead(HALL_PIN);
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
                    wdt_reset();  // blokujúca animácia 5×200ms — preventívny reset
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
                wdt_reset();  // blokujúca animácia 3×300ms — preventívny reset
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
        // Resetuj dt-base — bez tohto by prvá normálna iter po cal dostala
        // dt=5s+ (celá cal-doba), čo by slew-rate limiter okamžite preskočil
        // a motor by pri prvom plyne škubol.
        lastLoopTime = millis();
        return;
    }
    
    // === NORMÁLNA PREVÁDZKA ===

    // === DETEKCIA PORUCHY HALL SENZORA ===
    // ADC mimo kalibračného rozsahu (s marginom) ALEBO úplne na hrane (short na rail).
    // Druhá podmienka je istota aj keď je kalibrácia veľmi voľná.
    if (raw < 5 || raw > 1018 ||
        raw < cal_min - HALL_MARGIN || raw > cal_max + HALL_MARGIN) {
        if (!sensorFault) {
            sensorFault = true;
            Serial.println();
            Serial.println(F("!!! HALL SENZOR PORUCHA - motor zablokovany !!!"));
            Serial.print(F("ADC="));
            Serial.print(raw);
            Serial.print(F(" mimo ["));
            Serial.print(cal_min);
            Serial.print(F(","));
            Serial.print(cal_max);
            Serial.print(F("] +/- "));
            Serial.println(HALL_MARGIN);
            Serial.println(F("Restartuj na obnovenie cinnosti."));
            Serial.println();
        }
    }

    // Mapovanie na 0-100%
    int raw_percent = map(raw, cal_min, cal_max, 0, 100);
    raw_percent = constrain(raw_percent, 0, 100);

    // Aplikácia deadzone (fault prepise throttle na 0 nizsie)
    int throttle;
    if (raw_percent <= DEADZONE_LOW) {
        throttle = 0;
    } else if (raw_percent >= DEADZONE_HIGH) {
        throttle = 100;
    } else {
        throttle = map(raw_percent, DEADZONE_LOW, DEADZONE_HIGH, 0, 100);
    }

    // Fail-safe: pri detegovanej poruche force throttle na 0
    if (sensorFault) {
        throttle = 0;
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

    // Sensor fault prepise vsetko ostatne (throttle uz je 0 zhora)
    if (sensorFault) {
        target = 0;
        stateStr = "SENS";
    }

    // === ČÍTANIE POTENCIOMETROV (A1 ramp-up, A2 PWM limit) ===
#ifdef USE_RAMPUP_POT
    RAMP_UP_TIME = readRampTime();
#else
    RAMP_UP_TIME = DEFAULT_RAMP_UP_TIME;
#endif

#ifdef USE_PWM_LIMIT_POT
    int potValue = analogRead(PWM_LIMIT_POT);
    maxAllowedThrottle = map(potValue, 0, 1023, 30, 100);  // 30–100%
#else
    maxAllowedThrottle = DEFAULT_MAX_PWM_PERCENT;
#endif

    // === PWM LIMITER — strop PRED slew rate limiterom ===
    if (target > maxAllowedThrottle) target = maxAllowedThrottle;

    // === SLEW RATE LIMITER ===
    unsigned long now = millis();
    unsigned long dt = now - lastLoopTime;
    lastLoopTime = now;

    if (currentOutput < target) {
        // Rozbeh — LINEÁRNE stúpanie k cieľu (čas riadi RAMP_UP_TIME)
        // Float zachová zlomky — pri RAMP_UP_TIME=2000ms a dt=50ms je krok presne 2.5%/iter.
        currentOutput += 100.0f * dt / RAMP_UP_TIME;
        if (currentOutput > target) currentOutput = target;
    } else if (currentOutput > target) {
        // Dobeh — SPOJITÁ exponenciála: currentOutput *= DECAY^(dt/INTERVAL_MS)
        // Žiadne skoky, vyhladzuje sa cez existujúci dt z hlavného slew rate timeru.
        float factor = pow(RAMP_DOWN_DECAY, (float)dt / RAMP_DOWN_INTERVAL_MS);
        currentOutput *= factor;
        if (currentOutput < 2.0f) currentOutput = 0.0f;  // pod 2% už motor netiahne — snap na presnú 0 pre anti-plug check
        if (currentOutput < target) currentOutput = target;
    }

    int output = (int)currentOutput;

    // Nastav výstupy
    setPWMOutput(output);
    digitalWrite(DIR_PIN, direction);

    // Zobraz na NeoPixel (priorita: sensor fault > reverse block > normal)
    if (sensorFault) {
        // Kriticka porucha - rychle cervene blikanie (200ms)
        bool blink = (millis() / 200) % 2;
        for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, blink ? strip.Color(255, 0, 0) : 0);
        }
        strip.show();
    } else if (reverseBlocked) {
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
