# PCB2 — Kompletná špecifikácia zapojenia

## Prehľad

PCB2 je hlavná riadiaca doska systému MagLev Throttle. Obsahuje Arduino Nano v sockete, filtračné a bezpečnostné obvody, a konektory pre všetky externé moduly. Doska je napájaná externým 5V zdrojom (z datalogera alebo DC-DC meniča).

## Arduino Nano — piny

| Arduino Pin | Funkcia | Smer | Pripojené na | Externé súčiastky |
|-------------|---------|------|--------------|-------------------|
| A0 | Hall senzor analógový vstup | INPUT | J_hall pin 2 (SIG) | R6 (10kΩ pull-down na GND), C5 (100nF na GND) |
| A1 | Potenciometer ramp-up čas | INPUT | J_pot1 pin 2 (stredný vývod) | žiadne (pot je externý) |
| A2 | Potenciometer PWM limiter | INPUT | J_pot2 pin 2 (stredný vývod) | žiadne (pot je externý) |
| D2 | Prepínač FWD | INPUT_PULLUP | J_switch pin 2 (FWD) | žiadne externé rezistory |
| D3 | Prepínač REV | INPUT_PULLUP | J_switch pin 3 (REV) | žiadne externé rezistory |
| D4 | Motor DIR výstup | OUTPUT | J_motor pin 2 (DIR) | žiadne externé rezistory |
| D5 | Motor PWM výstup | OUTPUT | J_motor pin 1 (PWM) | R4 (10kΩ pull-down na GND), R5 (10kΩ pull-down na GND) |
| D9 | NeoPixel DATA výstup | OUTPUT | J_led pin 2 (DATA) | R7 (470Ω v sérii) |
| D13 | Vstavaná LED | OUTPUT | — | vstavaná na Arduino module |
| 5V | Napájanie +5V | POWER | +5V rail | C4 (470µF), C3 (100nF bypass) |
| GND | Zem | POWER | GND rail | spoločná zem všetkých obvodov |
| VIN | NEPOUŽITÝ | — | NEPRIPOJENÝ | **MUSÍ byť floating — nepripájať nikam!** |

## Konektory — pinout

### J_power (B2B-XH-A, JST-XH 2P) — Napájanie 5V

| Pin | Signál | Popis |
|-----|--------|-------|
| 1 | +5V | Napájanie z datalogera/DC-DC meniča |
| 2 | GND | Spoločná zem |

### J_hall (B3B-XH-A, JST-XH 3P) — Hall senzor (PCB1)

| Pin | Signál | Popis |
|-----|--------|-------|
| 1 | VCC | +5V napájanie pre SS49E senzor |
| 2 | SIG | Analógový výstup senzora → Arduino A0 |
| 3 | GND | Zem |

### J_motor (B3B-XH-A, JST-XH 3P) — Motor driver (Cytron MD30C)

| Pin | Signál | Popis |
|-----|--------|-------|
| 1 | PWM | PWM signál z Arduino D5. Rýchlosť motora. |
| 2 | DIR | Smer otáčania z Arduino D4. LOW=CW, HIGH=CCW. |
| 3 | GND | Spoločná zem. POVINNÁ — bez nej signály nemajú referenciu. |

### J_led (B3B-XH-A, JST-XH 3P) — LED pásik (PCB3, 8× WS2812B)

| Pin | Signál | Popis |
|-----|--------|-------|
| 1 | VCC | +5V napájanie pre LED pásik |
| 2 | DATA | Dátový signál z Arduino D9, cez R7 (470Ω) |
| 3 | GND | Zem |

### J_switch (B3B-XH-A, JST-XH 3P) — Prepínač smeru (3-polohový koliskový)

| Pin | Signál | Popis |
|-----|--------|-------|
| 1 | COM | Spoločný kontakt prepínača → GND |
| 2 | FWD | Dopredu → Arduino D2. Zopnutý = D2 spojený na GND cez prepínač. |
| 3 | REV | Dozadu → Arduino D3. Zopnutý = D3 spojený na GND cez prepínač. |

Logika (active LOW, interné pull-upy v Arduine):
- NEUTRAL: D2=HIGH, D3=HIGH (prepínač v strede, nič nie je zopnuté)
- FORWARD: D2=LOW, D3=HIGH (prepínač spojí pin 2 na COM=GND)
- REVERSE: D2=HIGH, D3=LOW (prepínač spojí pin 3 na COM=GND)

**Žiadne externé pull-up ani pull-down rezistory na D2/D3.** Firmvér aktivuje INPUT_PULLUP (~20-50kΩ interný pull-up v ATmega328P). Počas 500ms bootloadera piny plávajú, ale bezpečnosť je zabezpečená pull-downami na PWM (D5) — motor nemôže naštartovať bez ohľadu na stav prepínača.

### J_pot1 (B3B-XH-A, JST-XH 3P) — Potenciometer ramp-up čas

| Pin | Signál | Popis |
|-----|--------|-------|
| 1 | +5V | Horný vývod potenciometra |
| 2 | A1 | Stredný vývod (wiper) → Arduino A1 |
| 3 | GND | Dolný vývod potenciometra |

Potenciometer: 10kΩ lineárny. Rozsah vo FW: 2s – 4s ramp-up čas.
Ak potenciometer nie je pripojený a `USE_RAMPUP_POT` je zakomentovaný vo FW, A1 sa nečíta a použije sa konštanta `DEFAULT_RAMP_UP_TIME = 2000ms`.

### J_pot2 (B3B-XH-A, JST-XH 3P) — Potenciometer PWM limiter

| Pin | Signál | Popis |
|-----|--------|-------|
| 1 | +5V | Horný vývod potenciometra |
| 2 | A2 | Stredný vývod (wiper) → Arduino A2 |
| 3 | GND | Dolný vývod potenciometra |

Potenciometer: 10kΩ lineárny. Rozsah vo FW: 30% – 100% max PWM.
Ak potenciometer nie je pripojený a `USE_PWM_LIMIT_POT` je zakomentovaný vo FW, A2 sa nečíta a použije sa konštanta 70% (178/255).

## Pasívne súčiastky — hodnoty a zdôvodnenie

### R4 — 10kΩ ±5% (0805 SMD) — Pull-down #1 na PWM (D5)

**Zapojenie:** Medzi Arduino D5 a GND.
**Účel:** Bezpečnostný pull-down. Počas resetu/bootu Arduina (~500ms) sú piny v stave tri-state (vysoká impedancia, pull-upy vypnuté). Bez pull-downu pin D5 pláva → šum z motora môže vybudiť PWM vstup Cytronu nad 0.5V HIGH prah → motor sa roztočí nekontrolovane.
**Zdôvodnenie hodnoty:** ATmega328P datasheet — počas resetu piny tri-state. Cytron MD30C manuál — LOW prah 0-0.5V. Pri 10kΩ treba šumový prúd >50µA na prekročenie 0.5V. V paralele s R5 (5kΩ efektívne) treba >100µA. Prúd pri Arduino HIGH: 5V/5kΩ = 1mA, hlboko pod 20mA limitom ATmega328P.
**Zdroje:** ATmega328P datasheet sekcia 14.2.6 (Unconnected Pins), Cytron MD30C User Manual V1.4.

### R5 — 10kΩ ±5% (0805 SMD) — Pull-down #2 na PWM (D5)

**Zapojenie:** Medzi Arduino D5 a GND, paralelne s R4.
**Účel:** REDUNDANTNÝ bezpečnostný pull-down. Ak R4 zlyhá (otvorený obvod), R5 stále drží PWM na LOW. Efektívna hodnota R4‖R5 = 5kΩ.
**Zdôvodnenie:** Redundancia pre kritický bezpečnostný obvod. Motor driver riadi 24V/30A motor — nekontrolovaný rozbeh je nebezpečný.

### R6 — 10kΩ ±5% (0805 SMD) — Pull-down na Hall vstupe (A0)

**Zapojenie:** Medzi Arduino A0 a GND.
**Účel:**
1. **Bezpečnosť:** Ak sa odpojí kábel Hall senzora, A0 pláva → ADC by čítal náhodnú hodnotu → motor by sa mohol roztočiť na plný výkon. Pull-down stiahne A0 na 0V = žiadny plyn = motor stojí.
2. **RC filter:** Spolu s C5 (100nF) tvorí dolnopriepustný filter. f_cutoff = 1/(2π × 10kΩ × 100nF) = 159 Hz. PWM šum z motora (976 Hz) je filtrovaný, signál z throttle (~10 Hz) prechádza.
**Zdôvodnenie hodnoty:** ATmega328P ADC datasheet — maximálna odporúčaná source impedancia pre optimálnu presnosť ADC je 10kΩ. SS49E sensor má výstupnú impedanciu <1kΩ. Keď je senzor pripojený, paralelná impedancia R6‖senzor ≈ 900Ω, hlboko pod ADC limitom. Pull-down mierne ovplyvňuje signál (deliaci pomer 10k/(10k+1k) = 91%), kompenzované EEPROM kalibráciou.

### R7 — 470Ω ±5% (0805 SMD) — Séria na NeoPixel DATA (D9)

**Zapojenie:** V sérii medzi Arduino D9 a J_led pin 2 (DATA). D9 → R7 → J_led.
**Účel:** Tlmí odrazy signálu na konci kábla spôsobené impedančným nesúladom. Bez rezistora napäťové špičky z odrazov môžu zničiť vstupný obvod prvého WS2812B LED čipu.
**Zdôvodnenie hodnoty:** Adafruit NeoPixel Überguide (oficiálna dokumentácia): "Place a 300 to 500 Ohm resistor between the Arduino data output pin and the input to the first NeoPixel." 470Ω je štandardná odporúčaná hodnota. Prúd v dátovom vodiči je zanedbateľný (CMOS vstup WS2812B, rádovo pF), rezistor nespôsobuje merateľný úbytok napätia na signáli.

### C4 — 470µF/16V elektrolytický — Bulk stabilizácia napájania

**Zapojenie:** Medzi +5V rail a GND, čo najbližšie k J_power konektoru.
**POZOR POLARITA:** + pin na +5V, − pin na GND. Otočená polarita → výbuch!
**Účel:** Napájací kábel z datalogera je dlhý (~1m). Má odpor a indukčnosť. Keď NeoPixel pásik náhle odoberie prúd (8 LED × 60mA = až 480mA špičkovo), napätie na konci kábla klesne. C4 poskytuje lokálny zásobník energie a kompenzuje tieto poklesy. Tiež filtruje nízkofrekvenčný šum z napájacieho zdroja.
**Zdôvodnenie hodnoty:** Adafruit NeoPixel Überguide odporúča 500-1000µF pre napájanie NeoPixel. 470µF je na spodnej hranici ale pre 8 LED postačujúce. 16V rating poskytuje bezpečný margin nad 5V prevádzkovým napätím.

### C3 — 100nF keramický (0805 SMD) — Bypass pri Arduino 5V pine

**Zapojenie:** Medzi Arduino 5V pin a GND pin, čo najbližšie k pinom na sockete.
**Účel:** Štandardný vysokofrekvenčný bypass kondenzátor pre mikrokontrolér. Filtruje HF šum na napájacom pine ATmega328P. Bez neho môže digitálny šum z prepínania interných obvodov spôsobiť nestabilné správanie ADC a logiky.
**Zdôvodnenie hodnoty:** ATmega328P datasheet, sekcia "Power Supply": "It is recommended to use a 0.1µF (100nF) ceramic capacitor between VCC and GND." Toto je štandardná prax pre všetky mikrokontroléry.

### C5 — 100nF keramický (0805 SMD) — RC filter na Hall senzor vstupe (A0)

**Zapojenie:** Medzi Arduino A0 a GND, paralelne s R6.
**Účel:** Spolu s R6 (10kΩ) tvorí RC dolnopriepustný filter. Filtruje vysokofrekvenčný šum (hlavne PWM motora pri 976 Hz) z analógového signálu Hall senzora pred vstupom do ADC.
**Zdôvodnenie hodnoty:** f_cutoff = 1/(2π × R6 × C5) = 1/(2π × 10000 × 0.0000001) = 159 Hz. Signál z throttle (ruka jazdca) sa mení maximálne pri ~10 Hz, takže prechádza bez útlmu. PWM šum motora pri 976 Hz je tlmený o ~15 dB. 100nF je dostatočne veľká na efektívnu filtráciu, ale dostatočne malá aby neovplyvnila rýchlosť odozvy (RC = 1ms, ADC vzorkovanie 10× averaging je ~1ms).

## Zapojenie per-komponent (ASCII schémy)

### Napájanie:

```
J_power pin1 (+5V) ──┬──── C4+ (470µF) ──┬──── C3 (100nF) ──┬── Arduino 5V
                     │     C4−            │                   │
J_power pin2 (GND) ──┴────────────────────┴───────────────────┴── Arduino GND
```

### Hall senzor → A0:

```
J_hall pin1 (VCC) ──── +5V
J_hall pin2 (SIG) ──────────┬──── Arduino A0
                            │
                         R6 (10kΩ)     C5 (100nF)
                            │               │
J_hall pin3 (GND) ────── GND ──────────── GND
```
R6 a C5 sú oba medzi A0 a GND (paralelne k sebe).

### Motor driver (PWM + DIR):

```
Arduino D5 (PWM) ──┬─────────────── J_motor pin1 (PWM)
                   │
                ┌──┴──┐
                │     │
              R4(10k) R5(10k)       ← dva paralelné pull-downy
                │     │
                └──┬──┘
                   │
                  GND

Arduino D4 (DIR) ────────────────── J_motor pin2 (DIR)

                            GND ─── J_motor pin3 (GND)
```
D4 (DIR) nemá žiadny externý rezistor. Keď PWM=LOW, motor brzdí bez ohľadu na DIR.

### NeoPixel LED pásik:

```
                                +5V ─── J_led pin1 (VCC)
Arduino D9 ──── R7 (470Ω) ──────────── J_led pin2 (DATA)
                                GND ─── J_led pin3 (GND)
```

### Prepínač smeru:

```
                            GND ─── J_switch pin1 (COM)
Arduino D2 (INPUT_PULLUP) ──────── J_switch pin2 (FWD)
Arduino D3 (INPUT_PULLUP) ──────── J_switch pin3 (REV)
```
Žiadne externé rezistory. Interné pull-upy v ATmega328P (~20-50kΩ).

### Potenciometre:

```
+5V ─── J_pot1 pin1          +5V ─── J_pot2 pin1
A1  ─── J_pot1 pin2          A2  ─── J_pot2 pin2
GND ─── J_pot1 pin3          GND ─── J_pot2 pin3
```
Externé 10kΩ lineárne potenciometre. Keď nie sú pripojené, príslušný `#define` vo FW je zakomentovaný a pin sa nečíta.

## Kompletný BOM

| # | Ref | Hodnota | Puzdro | Popis | LCSC |
|---|-----|---------|--------|-------|------|
| 1 | C4 | 470µF/16V | elektrolyt SMD | Bulk stabilizácia, POZOR polarita | C129477 |
| 2 | C3 | 100nF/50V | 0805 keramický | Bypass Arduino 5V pin | C49678 |
| 3 | C5 | 100nF/50V | 0805 keramický | RC filter A0 (s R6) | C49678 |
| 4 | R4 | 10kΩ ±5% | 0805 | Pull-down D5 (PWM) #1 | C84376 |
| 5 | R5 | 10kΩ ±5% | 0805 | Pull-down D5 (PWM) #2 | C84376 |
| 6 | R6 | 10kΩ ±5% | 0805 | Pull-down A0 + RC filter | C84376 |
| 7 | R7 | 470Ω ±5% | 0805 | Séria D9 → NeoPixel DATA | C17710 |
| 8 | J_power | B2B-XH-A(LF)(SN) | THT | Napájanie 5V, JST-XH 2P | C158012 |
| 9 | J_hall | B3B-XH-A(LF)(SN) | THT | Hall senzor, JST-XH 3P | C144394 |
| 10 | J_motor | B3B-XH-A(LF)(SN) | THT | Motor driver, JST-XH 3P | C144394 |
| 11 | J_led | B3B-XH-A(LF)(SN) | THT | LED pásik, JST-XH 3P | C144394 |
| 12 | J_switch | B3B-XH-A(LF)(SN) | THT | Prepínač, JST-XH 3P | C144394 |
| 13 | J_pot1 | B3B-XH-A(LF)(SN) | THT | Pot ramp-up (A1), JST-XH 3P | C144394 |
| 14 | J_pot2 | B3B-XH-A(LF)(SN) | THT | Pot PWM limit (A2), JST-XH 3P | C144394 |
| 15 | U1 | 2× KH-2.54FH-1X15P-H8.5 | THT | Arduino Nano socket, female headers | — |

**Celkom: 15 súčiastok + Arduino Nano modul**

## Zdroje overenia

| Súčiastka | Zdroj | Čo bolo overené |
|-----------|-------|-----------------|
| R4, R5 (10kΩ pull-down) | ATmega328P datasheet §14.2.6, Cytron MD30C Manual V1.4 | Piny tri-state počas resetu, LOW prah 0-0.5V |
| R6 (10kΩ A0) | ATmega328P datasheet ADC characteristics | Max source impedancia 10kΩ, SS49E output <1kΩ |
| R7 (470Ω) | Adafruit NeoPixel Überguide – Best Practices | 300-500Ω séria na DATA |
| C3 (100nF bypass) | ATmega328P datasheet – Power Supply | 100nF medzi VCC a GND |
| C5 (100nF filter) | Výpočet RC: 1/(2π×10k×100n) = 159Hz | Filtruje motor PWM (976Hz), prepúšťa throttle (~10Hz) |
| C4 (470µF bulk) | Adafruit NeoPixel Überguide | 500-1000µF pre NeoPixel napájanie |
| D4 (DIR bez rezistora) | Cytron MD30C Manual truth table | PWM=LOW → BRAKE bez ohľadu na DIR |
| D2,D3 (bez ext. rezistorov) | ATmega328P datasheet, FW analýza | INPUT_PULLUP v FW, bezpečnosť cez PWM pull-downy |
