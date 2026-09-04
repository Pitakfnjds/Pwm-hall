# Inštrukcie pre Claude Code — Firmvérové zmeny pre efektivitu závodu

## Kontext

MagLev Throttle je elektronický plynový pedál pre elektrickú motokáru (Greenpower F24 závod). Cieľ závodu: prejsť na batériu čo najďalej. Tieto zmeny optimalizujú spotrebu energie. Jazda je na hladkom povrchu, jednoduchý okruh.

Firmware súbor: `hall_throttle_tzw/hall_throttle_tzw.ino`

Motor driver: Cytron MD30C (sign-magnitude: PWM=rýchlosť, DIR=smer). Keď PWM=0, motor aktívne brzdí (oba low-side FETy ON) + čiastočne regeneruje do batérie. Žiadny "coast" režim neexistuje.

## Aktuálny stav firmvéru

- A0 = Hall senzor (analógový vstup)
- A1 = potenciometer na nastavenie ramp-up času (UŽ EXISTUJE)
- D5 = PWM výstup (0–255)
- D4 = DIR výstup
- D2 = prepínač FWD (INPUT_PULLUP, active LOW)
- D3 = prepínač REV (INPUT_PULLUP, active LOW)
- D9 = NeoPixel (8 LED)
- D13 = vstavaná LED
- Lineárny ramp-up a ramp-down (slew rate limiter)
- EEPROM kalibrácia
- Anti-plugging ochrana

## Zmeny na implementáciu

### ZMENA 0: Compile-time prepínače pre potenciometre (IMPLEMENTOVAŤ AKO PRVÉ)

**Čo:** Každý potenciometer (A1 ramp-up, A2 PWM limiter) má `#define` prepínač. Keď je vypnutý, namiesto čítania potu sa použije hardkódovaná konštanta. Umožňuje nahrať FW bez fyzicky pripojených potenciometrov.

**Prečo:** PCB2 ešte nemá osadené potenciometre. Chceme testovať nový FW ihneď s rozumnými default hodnotami. Neskôr stačí zmeniť `#define` a rekompilovať.

**Implementácia — pridať na začiatok súboru (za includes, pred pin definície):**

```cpp
// === KONFIGURÁCIA POTENCIOMETROV ===
// Zakomentuj #define ak potenciometer NIE JE fyzicky pripojený.
// Keď je zakomentovaný, použije sa DEFAULT konštanta nižšie.

//#define USE_RAMPUP_POT      // A1: potenciometer pre ramp-up čas
//#define USE_PWM_LIMIT_POT   // A2: potenciometer pre PWM limiter

// === DEFAULT HODNOTY (keď pot nie je pripojený) ===
const unsigned long DEFAULT_RAMP_UP_TIME = 2000;   // 2 sekundy (0→100%)
const int DEFAULT_MAX_PWM = 178;                    // 70% z 255 = 178
```

**Implementácia — čítanie A1 (ramp-up) v hlavnej slučke:**

```cpp
// Nahradiť existujúce čítanie A1 potu:
#ifdef USE_RAMPUP_POT
    int rampPotValue = analogRead(RAMP_UP_POT);  // A1
    unsigned long rampUpTime = map(rampPotValue, 0, 1023, 3000, 10000);
#else
    unsigned long rampUpTime = DEFAULT_RAMP_UP_TIME;  // 2000ms
#endif
```

**Implementácia — čítanie A2 (PWM limiter) v hlavnej slučke:**

```cpp
#ifdef USE_PWM_LIMIT_POT
    int potValue = analogRead(PWM_LIMIT_POT);  // A2
    maxAllowedPWM = map(potValue, 0, 1023, 77, 255);  // 30% – 100%
#else
    maxAllowedPWM = DEFAULT_MAX_PWM;  // 178 = 70%
#endif
```

**Aktuálna konfigurácia na nahratie BEZ potenciometrov:**
- `USE_RAMPUP_POT` = zakomentovaný → ramp-up fixne 2000ms
- `USE_PWM_LIMIT_POT` = zakomentovaný → PWM limit fixne 70% (178/255)

**Keď sa potenciometre osadia na PCB, stačí odkomentovať príslušný riadok a nahrať znova.**

**Sériový monitor — pridať na koniec setup():**

```cpp
Serial.println("=== KONFIGURACIA ===");
#ifdef USE_RAMPUP_POT
    Serial.println("Ramp-up: POTENCIOMETER A1 (3-10s)");
#else
    Serial.print("Ramp-up: FIXNA HODNOTA ");
    Serial.print(DEFAULT_RAMP_UP_TIME);
    Serial.println("ms");
#endif

#ifdef USE_PWM_LIMIT_POT
    Serial.println("PWM limit: POTENCIOMETER A2 (30-100%)");
#else
    Serial.print("PWM limit: FIXNA HODNOTA ");
    Serial.print((DEFAULT_MAX_PWM * 100) / 255);
    Serial.println("%");
#endif
```

---

### ZMENA 1: PWM Limiter s potenciometrom na A2

**Čo:** Nový potenciometer na analógovom pine A2 nastavuje maximálny povolený PWM výstup. Aj keď jazdec stlačí plyn naplno, motor nedostane viac ako nastavenú hodnotu.

**Prečo:** Brushed DC motor má najvyššiu účinnosť pri ~60–70% PWM. Plný plyn plytvá energiou (I²R straty rastú s kvadrátom prúdu). Limiter zabraňuje neefektívnej jazde.

**Implementácia:**

```cpp
// Pridať pin definíciu
const int PWM_LIMIT_POT = A2;  // Potenciometer pre PWM limiter

// Pridať premennú
int maxAllowedPWM = 255;  // Aktuálny limit (aktualizuje sa z potu)

// V hlavnej slučke (loop), PRED aplikáciou ramp limitera:
// Čítaj potenciometer a mapuj na rozsah 30–100% (77–255)
// Minimum 30% aby motor vôbec ťahal, maximum 100%
int potValue = analogRead(PWM_LIMIT_POT);
maxAllowedPWM = map(potValue, 0, 1023, 77, 255);  // 30% – 100%

// Aplikuj limit na target PWM (PRED ramp limiterom):
targetPWM = min(targetPWM, maxAllowedPWM);
```

**Dôležité:** Limit sa aplikuje na `targetPWM` PRED slew rate limiterom. Ramp stále funguje normálne, len nikdy neprekročí limit.

**LED spätná väzba:** Keď jazdec stlačí plný plyn ale limit je aktívny (target bol orezaný), posledná LED na pásku by mala blikať (signalizuje "limit aktívny"). Jazdec tak vie, že nedostáva plný výkon.

### ZMENA 2: Potenciometer na A1 — ramp-up čas (UŽ EXISTUJE, len overiť rozsah)

**Čo:** A1 potenciometer už nastavuje ramp-up čas. Overiť/upraviť rozsah na 3s – 10s (namiesto pôvodného rozsahu ak bol menší).

**Prečo:** Pomalší ramp-up = nižší špičkový prúd = menej I²R strát. Straty rastú s KVADRÁTOM prúdu: 10A počas 2s = 200J strát, 5A počas 4s = 100J strát. Rovnaká práca, polovičné straty.

**Implementácia:**

```cpp
// Upraviť mapovanie A1 potenciometra (ak ešte nemá tento rozsah):
int rampPotValue = analogRead(RAMP_UP_POT);  // A1
unsigned long rampUpTime = map(rampPotValue, 0, 1023, 3000, 10000);  // 3s – 10s

// Použiť rampUpTime v slew rate výpočte
// rampUpTime je v milisekundách, definuje čas od 0% do 100%
```

### ZMENA 3: Exponenciálny ramp-down namiesto lineárneho

**Čo:** Keď jazdec pustí tlačidlo, PWM klesá exponenciálne (20% z aktuálnej hodnoty každých 200ms) namiesto lineárne. Motor spomaľuje plynulo a regeneruje energiu do batérie celú dobu.

**Prečo:** 
- Lineárny ramp-down: PWM klesá o konštantný krok → na začiatku príliš prudké brzdenie (vysoké back-EMF + nízky PWM = veľký brzdný moment)
- Exponenciálny ramp-down: PWM klesá úmerne aktuálnej hodnote → brzdná sila je rovnomernejšia, jazdec necíti prudký prechod
- Motor regeneruje celú dobu pokým PWM klesá → energia sa vracia do batérie
- Dlhšie spomaľovanie = viac regenerovanej energie

**Implementácia:**

```cpp
// Nová konštanta
const float RAMP_DOWN_DECAY = 0.80;  // Každý krok zachová 80% aktuálnej hodnoty (= pokles o 20%)
const unsigned long RAMP_DOWN_INTERVAL = 200;  // Krok každých 200ms

// Nová premenná
unsigned long lastRampDownTime = 0;

// V hlavnej slučke — nahradiť lineárny ramp-down:
// Pôvodný lineárny kód (ODSTRÁNIŤ alebo nahradiť):
//   currentOutput -= rampDownStep;
//
// Nový exponenciálny kód:
if (targetPWM < currentOutput) {
    // Ramp DOWN - exponenciálny
    unsigned long now = millis();
    if (now - lastRampDownTime >= RAMP_DOWN_INTERVAL) {
        lastRampDownTime = now;
        currentOutput = (int)(currentOutput * RAMP_DOWN_DECAY);
        
        // Keď klesne pod 5 (2% PWM), skoč na 0
        // Motor pri takto nízkom PWM aj tak neťahá
        if (currentOutput < 5) {
            currentOutput = 0;
        }
    }
} else if (targetPWM > currentOutput) {
    // Ramp UP - ponechať LINEÁRNY (riadený A1 potom)
    // Existujúci kód pre ramp-up zostáva
}
```

**Časový priebeh exponenciálneho ramp-downu (príklad z 200/255 = 78%):**

```
Čas 0.0s: PWM = 200 (78%)
Čas 0.2s: PWM = 160 (63%)
Čas 0.4s: PWM = 128 (50%)
Čas 0.6s: PWM = 102 (40%)
Čas 0.8s: PWM = 82  (32%)
Čas 1.0s: PWM = 65  (26%)
Čas 1.2s: PWM = 52  (20%)
Čas 1.4s: PWM = 42  (16%)
Čas 1.6s: PWM = 33  (13%)
Čas 1.8s: PWM = 27  (11%)
Čas 2.0s: PWM = 21  (8%)
Čas 2.2s: PWM = 17  (7%)
Čas 2.4s: PWM = 14  (5%)
Čas 2.6s: PWM = 11  (4%)
Čas 2.8s: PWM = 9   (3.5%)
Čas 3.0s: PWM = 7   (2.7%)
Čas 3.2s: PWM = 6   (2.3%)
Čas 3.4s: PWM = 4   → skok na 0
```

Celkový čas z plného výkonu na 0: cca 3.5 sekundy. Prispôsobiť cez RAMP_DOWN_DECAY:
- 0.85 = pomalšie (viac regen, ~5s)
- 0.75 = rýchlejšie (menej regen, ~2.5s)

### ZMENA 4: Ramp-up zostáva lineárny

**Čo:** Ramp-up NEZMENIŤ na exponenciálny. Ponechať lineárny, riadený A1 potenciometrom.

**Prečo:** Lineárny ramp-up je pre jazdca intuitívny — rovnomerné zrýchľovanie. Exponenciálny ramp-up by bol na začiatku príliš pomalý a na konci príliš prudký.

### ZMENA 5: Aktualizovať LED spätnú väzbu

**Čo:** Pridať vizuálnu indikáciu pre nové funkcie.

```
Normálny režim (existujúci):
  0%   = všetky zhasnuté
  25%  = zelená → žltá (2 LED)
  50%  = zelená → oranžová (4 LED)
  100% = zelená → červená (8 LED)

Nové indikácie:
  PWM limiter aktívny = posledná LED bliká bielo
    → Keď targetPWM bol orezaný limiterom (jazdec dal viac plynu
       ako je povolené). Signalizuje "si na limite, viac nedostaneš"
    → Podmienka: rawTargetPWM > maxAllowedPWM
  
  Exponenciálny ramp-down (regenerácia) = LED sa postupne zhášajú
    → Aktuálne správanie by malo byť automatické, lebo LED zobrazujú
       currentOutput a ten exponenciálne klesá
    → Žiadna špeciálna zmena potrebná
```

## Aktualizácia CLAUDE.md

Po implementácii zmien aktualizuj CLAUDE.md — pridaj do Pin Assignments:

```markdown
| A1 | Potentiometer: ramp-up time (3s–10s) |
| A2 | Potentiometer: PWM limiter (30%–100%) |
```

A do Firmware Key Concepts pridaj:

```markdown
- **PWM limiter:** Adjustable via potentiometer on A2 (30%–100%). Prevents inefficient full-throttle operation. Last LED blinks white when limit active.
- **Exponential ramp-down:** When throttle released, PWM decays by 20% every 200ms (configurable via RAMP_DOWN_DECAY). Provides smooth regenerative braking. Motor reaches 0 in ~3.5s from full power.
- **Ramp-up pot (A1):** Adjustable ramp-up time 3s–10s. Slower ramp = lower peak current = less I²R loss.
```

## Aktualizácia PCB2 schémy

Na PCB2 treba pridať 2 konektory (alebo priamo potenciometre):
- A1: potenciometer 10kΩ lineárny — stredný pin na A1, krajné na +5V a GND (UŽ EXISTUJE)
- A2: potenciometer 10kΩ lineárny — stredný pin na A2, krajné na +5V a GND (NOVÝ)

Ak potenciometre nebudú priamo na PCB ale na kábli, pridaj na PCB2 ďalší JST-XH 3P konektor:
```
J_pot2: JST-XH 3P
  pin 1 = +5V
  pin 2 = A2 (stredný vývod potenciometra)
  pin 3 = GND
```

## Poradie implementácie

1. **ZMENA 0** (compile-time define prepínače) — najprv, aby všetko ostatné fungovalo bez potov
2. **ZMENA 1** (PWM limiter) — najväčší dopad na spotrebu, použije DEFAULT_MAX_PWM=178 (70%)
3. **ZMENA 3** (exponenciálny ramp-down) — regenerácia
4. **ZMENA 2** (rozsah A1 potu) — len ak je USE_RAMPUP_POT aktívny, inak DEFAULT_RAMP_UP_TIME=2000
5. **ZMENA 5** (LED) — vizuálna spätná väzba

## Testovanie

Po každej zmene:
1. Sériový monitor 115200 — overiť výpis konfigurácie pri štarte (FIXNA HODNOTA / POTENCIOMETER)
2. Overiť že bez potenciometrov (oba define zakomentované) FW funguje s default hodnotami
3. PWM limiter: cez Serial monitor overiť že currentOutput nikdy neprekročí 178 (70%)
4. Exponenciálny ramp-down: pustiť plyn a sledovať currentOutput v Serial Plotter — má klesať exponenciálne, nie lineárne
5. Overiť že anti-plugging ochrana stále funguje s novým ramp-downom
6. Overiť že EEPROM kalibrácia nie je ovplyvnená
7. Neskôr keď budú poty: odkomentovať define, rekompilovať, overiť že pot nastavuje hodnoty v správnom rozsahu

## Bezpečnostné upozornenia

**AKTUÁLNA KONFIGURÁCIA NA OKAMŽITÉ NAHRATIE (bez HW zmien):**
```
//#define USE_RAMPUP_POT      ← zakomentované = NEAKTÍVNE
//#define USE_PWM_LIMIT_POT   ← zakomentované = NEAKTÍVNE
DEFAULT_RAMP_UP_TIME = 2000   → ramp-up 2 sekundy
DEFAULT_MAX_PWM = 178         → max PWM 70%
RAMP_DOWN_DECAY = 0.80        → exponenciálny ramp-down 20%/200ms
```
Toto je všetko čo treba. Arduino Nano + existujúci kábel k Cytron MD30C. Žiadne nové súčiastky.

---

### Ostatné bezpečnostné upozornenia

- PWM limiter sa aplikuje PRED ramp limiterom, nie po ňom
- Exponenciálny ramp-down MUSÍ vždy dôjsť na 0 (nie zastaviť na RAMP_DOWN_DECAY)
- Keď prepínač je v NEUTRAL, okamžitý ramp na 0 (existujúce správanie) — NEZMENIŤ
- Anti-plugging ochrana MUSÍ stále fungovať — zmena smeru len keď currentOutput == 0
- Pri PWM=0 motor aktívne brzdí (Cytron MD30C) — toto je správne a bezpečné správanie
