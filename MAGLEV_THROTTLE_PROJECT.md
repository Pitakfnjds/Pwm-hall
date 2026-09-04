# MagLev Throttle – Kompletná technická dokumentácia projektu

> **Účel tohto dokumentu:** Slúži ako knowledge base / memory pre Claude Code na pokračovanie vo vývoji projektu. Obsahuje všetky technické rozhodnutia, špecifikácie, architektúru, firmware, PCB dizajn a otvorené otázky z celého doterajšieho vývoja.

---

## 1. Prehľad projektu

### 1.1 Čo je MagLev Throttle
Bezkontaktný ovládač plynu (throttle) pre elektrickú motokáru. Využíva Hall efektový senzor umiestnený medzi dvoma neodýmovými magnetmi s opačnou polaritou. Stlačením tlačidla sa mení vzdialenosť horného magnetu od senzora, čím sa mení magnetické pole a tým aj analógový výstup senzora → riadenie otáčok motora cez PWM.

### 1.2 Kontext projektu
- **Typ:** Edukačný / súťažný projekt
- **Cieľová skupina:** Študenti od 16 rokov
- **Dôraz:** Bezpečnosť, spoľahlivosť, pochopiteľnosť, hands-on učenie
- **Dokumentácia:** Všetka písaná tak, aby ju pochopil 16-ročný študent

### 1.3 Existujúca motokára
- Elektrická motokára s existujúcim dataloggerom
- Datalogger potenciálne poskytuje 5V napájanie pre náš systém
- Motor riadený cez externý motor driver Cytron MD30C
- Batériové napájanie motora: 2× 12V autobatérie v sérii = 24V nominálne (~25–28V plne nabité)

---

## 2. Architektúra systému

### 2.1 Modulárny 3-PCB dizajn

Systém je rozdelený na 3 samostatné PCB prepojené kabelážou:

```
═══════════════════ VOLANT ═══════════════════     ════════ PRI MOTORE ════════

┌─────────────┐  krátky   ┌──────────────────┐          ┌──────────────────┐
│   PCB1      │  kábel    │      PCB2        │  dlhý    │  Cytron MD30C    │
│ Sensor Board│ ────────► │  Main Control    │  kábel   │  Motor Driver    │
│ (v tlačidle)│ Hall sig  │  Board (Arduino) │ ────────►│                  │
└─────────────┘ +napájanie└──────────────────┘ PWM+DIR  └──────────────────┘
                                │       ▲        +GND            │
                           LED dáta     │ 5V                     │
                          +napájanie    │ napájanie               ▼
                                ▼       │                     DC Motor
                        ┌─────────────┐ │
                        │    PCB3     │ │
                        │  LED Strip  │ [ext. 5V zdroj]
                        │   Board     │ (zdroj zatiaľ neurčený)
                        └─────────────┘
```

### 2.2 Prečo modulárny dizajn
- Jednoduchšie troubleshooting – každý modul sa dá testovať samostatne
- Jednoduchšia údržba a výmena komponentov
- Senzor musí byť fyzicky v tlačidle (oddelený od riadiacej dosky)
- LED pásik je na inom mieste ako riadiaca doska

---

## 3. PCB1 – Sensor Board (Senzorová doska)

### 3.1 Účel
Nesie Hall efektový senzor SS49E. Montuje sa priamo do mechanizmu throttle tlačidla na motokáre.

### 3.2 Kľúčový komponent
- **SS49E** – Lineárny Hall efektový senzor
  - Napájanie: 2.7V – 6.5V (typicky 5V)
  - Výstup: Analógový napäťový signál (ratiometrický)
  - Pokojový stav (bez magnetického poľa): ~2.5V (stred napájacieho napätia)
  - Citlivosť: ~1.4 mV/Gauss (typická)
  - Rozsah: ±700 Gauss
  - 3 piny: VCC, GND, OUT (analógový výstup)

### 3.3 Magnetický systém
- **Spodný magnet (statický):** Neodýmový magnet pevne fixovaný pod senzorom
- **Horný magnet (pohyblivý):** Neodýmový magnet na tlačidle/páke, pohybuje sa stlačením
- **Polarita:** Opačná – magnety sú orientované tak, že vytvárajú diferenciálne pole
  - Spodný: napr. severný pól smerom k senzoru
  - Horný: južný pól smerom k senzoru
- **Princíp:** V pokoji (nestlačené) sú magnety ďaleko od seba → slabšie pole. Pri stlačení sa horný magnet blíži k senzoru → zosilnenie diferenciálneho poľa → zmena výstupného napätia senzora
- Tento setup vytvára predvídateľné a monotónne premenné pole vhodné na presné riadenie plynu

### 3.4 Prepojenie s PCB2
- Cez kábel s JST-XH konektorom
- 3 vodiče: VCC (5V), GND, SIGNAL (analógový výstup senzora)
- Kábel vedie z tlačidla k hlavnej doske

---

## 4. PCB2 – Main Control Board (Hlavná riadiaca doska)

### 4.1 Prehľad
Centrálny mozog systému. Obsahuje Arduino Nano, spracováva signál zo senzora, generuje PWM pre motor driver, riadi LED pásik a spracováva prepínač smeru.

### 4.2 DÔLEŽITÉ: Dve verzie dizajnu

#### Verzia 1 – Pôvodná (s MP1584EN step-down konvertorom)
- 31 komponentov + Arduino Nano
- Obsahovala MP1584EN step-down konvertor pre napájanie z batériového napätia (24-36V) na 5V
- MP1584EN + 15 podporných komponentov (induktor, diódy, kondenzátory, rezistory pre nastavenie napätia)
- Väčšia doska
- **STATUS: NAHRADENÁ zjednodušenou verziou**

#### Verzia 2 – Zjednodušená (aktuálna, len 5V vstup) ✅
- **16 komponentov + Arduino Nano**
- Odstránený celý step-down konvertor a jeho 15 podporných komponentov
- 5V napájanie z externého zdroja (datalogger motokáry)
- Stabilizácia pre dlhý kábel (~1 meter) pomocou bulk kondenzátora
- **Rozmer dosky: 40 × 55 mm**
- **TOTO JE AKTUÁLNY DIZAJN NA KTOROM SA PRACUJE**

### 4.3 Zjednodušená PCB2 – Kompletný zoznam komponentov (16 ks + Arduino Nano)

| # | Komponent | Hodnota/Typ | Účel |
|---|-----------|-------------|------|
| 1 | Arduino Nano | ATmega328P | Hlavný mikrokontrolér |
| 2 | C_bulk | 470µF/16V elektrolytický | Stabilizácia 5V pre dlhý kábel (~1m) |
| 3 | C1 | 100nF keramický | Filtrácia napájania Arduino |
| 4 | C2 | 100nF keramický | Filtrácia analógového vstupu Hall senzora |
| 5 | C3 | 100nF keramický | Filtrácia napájania LED výstupu |
| 6 | R_pulldown1 | 10kΩ | Bezpečnostný pull-down na PWM výstupe pre motor |
| 7 | R_pulldown2 | 10kΩ | Bezpečnostný pull-down (redundantný) |
| 8 | R_hall | 10kΩ | Pull-down na analógovom vstupe Hall senzora |
| 9 | R_led | 470Ω | Sériový ochranný rezistor pre dátový signál LED pásika |
| 10 | R_sw1 | 10kΩ | Pull-down pre pin prepínača smeru (pozícia 1) |
| 11 | R_sw2 | 10kΩ | Pull-down pre pin prepínača smeru (pozícia 2) |
| 12 | J_power | JST-XH 2-pin | Konektor 5V napájania (z datalogger/ext. zdroj) |
| 13 | J_hall | JST-XH 3-pin | Konektor pre Hall senzor (VCC, GND, SIG) |
| 14 | J_motor | JST-XH 3-pin | Konektor pre motor driver (PWM + DIR + GND) |
| 15 | J_led | JST-XH 3-pin | Konektor pre LED pásik (5V, GND, DATA) |
| 16 | J_switch | JST-XH 3-pin | Konektor pre 3-pozičný prepínač smeru |

### 4.4 Arduino Nano – Priradenie pinov

| Arduino Pin | Funkcia | Pripojený na | Poznámka |
|-------------|---------|-------------|----------|
| A0 | Analógový vstup | J_hall SIG (SS49E) | Čítanie Hall senzora. PCB2: R6 10kΩ pull-down + C5 100nF RC filter (f_c ≈ 159 Hz) |
| A1 | Analógový vstup | J_pot1 (10kΩ pot) | PWM limiter (30–100%), aktívne keď `USE_PWM_LIMIT_POT` |
| A2 | Analógový vstup | J_pot2 (10kΩ pot) | Ramp-up čas (2–4 s), aktívne keď `USE_RAMPUP_POT` |
| D5 | PWM výstup | J_motor → Cytron MD30C PWM | 976 Hz PWM. PCB2: R4‖R5 = 2× 10kΩ pull-down (redundant) |
| D4 | DIR výstup | J_motor → Cytron MD30C DIR | HIGH = MOTOR_FORWARD, LOW = MOTOR_REVERSE |
| D9 | Digitálny výstup | J_led → NeoPixel DATA | Cez R7 470Ω sériový rezistor |
| D2 | Digitálny vstup | J_switch pin 2 (FWD) | INPUT_PULLUP, active LOW (prepínač COM → GND) |
| D3 | Digitálny vstup | J_switch pin 3 (REV) | INPUT_PULLUP, active LOW (prepínač COM → GND) |
| D13 | Digitálny výstup | vstavaná LED | Stavová indikácia |
| 5V | Napájanie | Napájacie rozvádzanie na PCB | Z J_power cez C4 (470µF bulk) + C3 (100nF bypass) |
| GND | Zem | Spoločná zem celého systému | |
| VIN | **NEPOUŽITÝ** | NEPRIPOJENÝ | MUSÍ ostať floating (kolízia s 5V napájaním cez regulátor) |

> **Poznámka:** Hodnoty zodpovedajú aktuálnemu firmware (`hall_throttle_tzw.ino`) a PCB špecifikácii (`PCB2_ZAPOJENIE_SPECIFIKACIA.md`).

### 4.5 Napájací obvod (zjednodušená verzia)

```
Ext. 5V zdroj ──►[J_power]──►──┬──[C4 470µF]──┬──[C3 100nF]──► 5V rail (+ Arduino 5V pin)
(datalogger)                    │              │
                               GND            GND

5V rail ──[A0]──┬── C5 100nF ── GND    (RC filter Hall vstupu, paralelne s R6)
                R6 (10kΩ)
                │
               GND
```

- **C4 470µF bulk** kompenzuje pokles napätia na dlhom kábli (~1 m) a poskytuje zásobu energie pri prúdových špičkách NeoPixel pásika.
- **C3 100nF bypass** pri Arduino 5V pine — štandardný HF bypass pre MCU.
- **C5 100nF** spolu s R6 tvorí RC dolnopriepustný filter pre A0 (f_c ≈ 159 Hz, tlmí PWM šum motora 976 Hz).

> Plný BOM s konektormi, polaritami a LCSC číslami: `PCB2_ZAPOJENIE_SPECIFIKACIA.md`.

---

## 5. PCB3 – LED Strip Board

### 5.1 Účel
Vizuálna spätná väzba pre jazdca – zobrazuje úroveň plynu, smer, kalibračný režim atď.

### 5.2 Komponent
- **NeoPixel (WS2812B) LED pásik** – adresovateľné RGB LED
- Napájanie: 5V
- Dátový signál: jednovodičový protokol (z Arduino D6 cez 470Ω rezistor)

### 5.3 Prepojenie s PCB2
- Cez kábel s JST-XH 3-pin konektorom
- 3 vodiče: 5V, GND, DATA

### 5.4 LED indikácie (z firmware)
- **Úroveň plynu:** Postupné rozsvecovanie LED podľa pozície throttle
- **Smer:** Rôzne farby pre Forward / Reverse / Neutral
- **Kalibrácia:** Špeciálny blikací pattern počas kalibračného režimu
- **Chyba:** Indikácia chyby kalibrácie alebo senzora

---

## 6. Motor Driver – Cytron MD30C

### 6.1 Špecifikácie
- **Model:** Cytron MD30C (30Amp 5V-30V DC Motor Driver)
- **Napätie:** 5V až 30V DC
- **Prúd:** 30A kontinuálne, **80A špičkovo (1 sekunda)**
- **Riadenie:** PWM + DIR (sign-magnitude mode)
- **Logický vstup:** 3.3V aj 5V kompatibilný (Arduino Nano = 5V → OK)
- **Ochranné funkcie:** Ochrana proti prepólovaniu napájania, nadprúdová ochrana
- **Brzdenie:** Regeneratívne brzdenie (energia sa vracia do batérie)
- **H-Bridge:** Full NMOS dizajn (efektívnejší, nepotrebuje externý chladič)
- **PWM frekvencia:** Do 20kHz (Arduino D5 generuje 976Hz → OK)
- **Použitie:** Riadenie DC motora motokáry (500W, 24V)

### 6.2 Prečo Cytron MD30C

Pôvodná doska (TZW-36V-30A-HS) vyhorela pri prepnutí smeru za chodu motora (plugging efekt, prúdový náraz ~150A). Cytron MD30C bol zvolený ako náhrada pre overené parametre (80A peak), vstavanú ochranu (nadprúd, prepólovanie), regeneratívne brzdenie a interné napájanie logiky.

### 6.3 Napájanie motor drivera

- Cytron MD30C sa napája **priamo z batérie motora** (24V)
- Logika si generuje napájanie **interne** z napätia motora
- **NIE JE POTREBNÉ** externé 5V napájanie z PCB2
- Jediné prepojenie s PCB2: **PWM signál + DIR signál + GND**

```
Batéria 24V ──────────────► Cytron MD30C (Vmotor vstup)
                                │
                                ├── Interný regulátor → logika
                                ├── H-bridge → motor
                                │
PCB2 (Arduino) ──PWM (D5)────► PWM vstup
               ──DIR (D4)────► DIR vstup
               ──GND─────────► GND (spoločná zem - POVINNÉ)
```

### 6.4 Prepojenie s PCB2
- **PWM signál:** Z Arduino D5 cez J_motor konektor
- **DIR signál:** Z Arduino D4 cez J_motor konektor
- **GND:** Spoločná zem medzi PCB2 a motor driverom (POVINNÉ pre správnu funkciu PWM/DIR signálov)
- **J_motor:** JST-XH 3-pin (PWM, DIR, GND)

### 6.5 Režim ovládania
- **Sign-magnitude mode:** PWM pin = rýchlosť (duty cycle 0–100%), DIR pin = smer (HIGH/LOW)
- Firmware používa `analogWrite()` pre PWM a `digitalWrite()` pre DIR
- Cytron podporuje aj locked-antiphase mode, ale **nepoužívame ho**
- **PWM=0 = aktívne brzdenie** (low-side MOSFETy skratujú vinutie motora → regeneratívne brzdenie)

### 6.6 Bezpečnosť PWM výstupu
- **2× 10kΩ pull-down rezistory** na PWM line na PCB2
- Zabezpečujú, že pri reštarte/resete Arduina alebo strate signálu je PWM line stiahnutá na GND
- Cytron MD30C pri PWM=LOW **aktívne brzdí** motor (skratovanie vinutia cez MOSFET-y → regeneratívne brzdenie)
- Kombinácia HW pull-down + aktívne brzdenie = motor sa zastaví aj pri výpadku Arduina
- Toto je **hardvérová bezpečnostná vrstva** nezávislá od firmware

---

## 7. Prepínač smeru jazdy

### 7.1 Typ
- **3-pozičný koliskový prepínač** (rocker switch)
- Pozície: **1=Forward** / **0=Neutral** / **2=Reverse**
- Fyzicky: 1 ↔ 0 ↔ 2 (vždy prechádza cez Neutral, nie je možné ísť priamo z 1 do 2)

### 7.2 Zapojenie
- Stredný (COM) pin prepínača: GND
- Krajný pin: Arduino D2 (Forward) – **bez externého rezistora**, FW použije INPUT_PULLUP
- Krajný pin: Arduino D3 (Reverse) – **bez externého rezistora**, FW použije INPUT_PULLUP

### 7.3 Logika (active LOW, interný pull-up ~20–50 kΩ)
| Pozícia | D2 | D3 | Stav |
|---------|----|----|------|
| Forward | LOW  | HIGH | Motor vpred (prepínač spojí D2 na COM = GND) |
| Neutral | HIGH | HIGH | Motor vypnutý (oba piny ťahané pull-upom) |
| Reverse | HIGH | LOW  | Motor vzad (prepínač spojí D3 na COM = GND) |

### 7.4 Špeciálna funkcia – Vynútená rekalibrácia
- Ak je prepínač v určitej pozícii **počas zapnutia** (startup), firmware spustí vynútenú rekalibráciu
- Toto umožňuje resetovať kalibráciu bez nutnosti meniť kód

---

## 8. Firmware

### 8.1 Platforma
- **Arduino IDE**
- **Jazyk:** C/C++ (Arduino framework)
- **MCU:** ATmega328P (Arduino Nano)

### 8.2 Knižnice
- **EEPROM.h** – Ukladanie kalibračných dát do EEPROM
- **Adafruit_NeoPixel.h** (alebo ekvivalent) – Riadenie WS2812B LED pásika

### 8.3 Hlavné funkcionality firmware

#### 8.3.1 Čítanie Hall senzora
- Analógový vstup A0
- `analogRead(A0)` → hodnota 0–1023, priemer z 10 vzoriek
- Mapovanie surových hodnôt na rozsah throttle (0–100% alebo 0–255 PWM)
- **Inverzia (`INVERT_THROTTLE`, default zapnuté):** pri aktuálnej orientácii magnetu dáva tlačidlo v pokoji VYSOKÉ ADC a stlačené NÍZKE. Mapovanie je preto `map(raw, cal_max, cal_min, 0, 100)`. Kalibrácia ukladá surové min/max, takže je na orientácii nezávislá. Pre pôvodnú orientáciu stačí `#define` zakomentovať.
- **Stabilné čítanie potov (`analogReadStable`):** po prepnutí ADC kanála z A0 na pot sa spraví jedno zahodené čítanie, 50 µs pauza a priemer zo 4 vzoriek. Bez toho sa náboj zo S/H kondenzátora (Hall ≈ 800) prelieval do pot kanála a spôsoboval špičky na 1023.

#### 8.3.2 EEPROM kalibračný systém
Toto je kľúčová funkcia firmware:

**Kalibračné dáta uložené v EEPROM:**
- `hall_min` – Minimálna hodnota senzora (throttle úplne pustený)
- `hall_max` – Maximálna hodnota senzora (throttle úplne stlačený)
- `calibration_valid` – Validačný flag/marker (na detekciu poškodených dát)

**Kalibračný proces:**
1. Pri štarte firmware skontroluje EEPROM na prítomnosť platných kalibračných dát
2. Validácia: Kontrola validačného markeru + overenie že min < max + overenie rozumného rozsahu hodnôt
3. Ak sú dáta platné → použijú sa uložené hodnoty
4. Ak sú dáta neplatné/poškodené/chýbajú → spustí sa kalibračný režim

**Kalibračný režim:**
- LED pásik indikuje kalibračný stav (blikanie)
- Užívateľ musí stlačiť throttle na minimum a maximum
- Po úspešnej kalibrácii sa dáta uložia do EEPROM
- Kalibrácia prežije vypnutie/zapnutie (perzistentné uloženie)

**Vynútená rekalibrácia:**
- Spustí sa ak je prepínač smeru v určitej pozícii pri zapnutí
- Spustí nový kalibračný proces; stará kalibrácia v EEPROM sa NEMAŽE, prepíše sa až po úspešnej novej kalibrácii (atomicky cez `eepromSaveCalibration`)
- Slúži ako "factory reset" pre prípad zlej kalibrácie

#### 8.3.3 PWM generovanie pre motor
- Výstup na D5 (PWM capable pin)
- `analogWrite(D5, pwm_value)` → 0–255 (0–100% pre Cytron MD30C)
- Hodnota vypočítaná z kalibrovaného rozsahu Hall senzora
- **Dead zone** na spodku rozsahu – malé hodnoty sa zaokrúhlia na 0 (bezpečnosť)

#### 8.3.3a Slew rate limiter (postupný rozbeh a exponenciálny dobeh)
- Výstup sa nemení okamžite, ale je obmedzený maximálnou rýchlosťou zmeny
- **Rozbeh (lineárny, `RAMP_UP_TIME`):** default 2 s z 0 % na 100 %. Cez voliteľný A2 pot (`USE_RAMPUP_POT`) nastaviteľný v rozsahu 2–4 s.
- **Dobeh (spojitá exponenciála):** `currentOutput *= RAMP_DOWN_DECAY^(dt/RAMP_DOWN_INTERVAL_MS)` s `DECAY=0.72` za `INTERVAL=300 ms`. Z 80 % na 10 % cca **1.9 s**, zo 100 % na 5 % cca **2.7 s**; pod 2 % výstup skočí na presnú 0 (cca **3.6 s** zo 100 %), aby anti-plugging kontrola videla skutočnú nulu. Žiadne diskrétne kroky — vyhladzuje sa cez existujúce `dt` v každom loop ticku. (Pôvodne `DECAY=0.85`, ~5.4 s; skrátené na polovicu.)
- Funguje pre všetky prechody: stlačenie/pustenie tlačidla, prepnutie do STOP, anti-plugging brzdenie
- Jedna premenná `currentOutput` sa v každom cykle posúva smerom k cieľu (`target`) — lineárne nahor alebo exponenciálne nadol
- Ak vodič znovu stlačí tlačidlo počas dobehu, výstup začne lineárne stúpať k novej hodnote

#### 8.3.3b PWM limiter (strop výstupu)
- Tvrdý strop na `target`, aplikovaný PRED slew rate limiterom — rampa funguje normálne, len nikdy neprekročí limit
- Default `DEFAULT_MAX_PWM_PERCENT = 70 %`; cez voliteľný A1 pot (`USE_PWM_LIMIT_POT`) nastaviteľný 30–100 %
- Dôvod: brushed DC motor má najvyššiu účinnosť pri ~60–70 % PWM, I²R straty rastú s kvadrátom prúdu
- LED pásik škáluje `currentOutput` na `maxAllowedThrottle`, nie na absolútnych 100 % — na strope je pásik plný červený, vodič vidí, že je na limite
- Sériový výpis v loope ukazuje `LIM:xx%` (a surovú hodnotu potu, ak je pot zapnutý)
- Compile-time prepínače `USE_RAMPUP_POT` / `USE_PWM_LIMIT_POT` sú default zakomentované — firmware ide nahrať bez fyzicky pripojených potov

#### 8.3.4 Bezpečnostné funkcie vo firmware
1. **Motor stop pri neutrale:** Ak je prepínač v Neutral → target = 0 (motor plynulo dobieha)
2. **Motor stop pri chybe senzora:** Ak je hodnota senzora mimo kalibrovaného rozsahu → PWM = 0
3. **Motor stop pri neplatnej kalibrácii:** Ak nie sú platné kalibračné dáta → motor sa nespustí
4. **Dead zone:** Eliminuje neúmyselné pohyby motora pri minimálnom stlačení
5. **Kombinácia s hardware pull-down:** Firmware safety + hardware safety = redundantná ochrana
6. **Anti-plugging ochrana:** Zmena smeru (FWD↔REV) je povolená len keď `currentOutput == 0`. Slew rate limiter zabezpečuje plynulé zastavenie pred zmenou smeru
7. **Slew rate limiter:** Zabraňuje náhlym zmenám PWM výstupu — motor sa rozbieha lineárne (default 2 s) a zastavuje exponenciálne (~3.6 s do nuly).

#### 8.3.5 Anti-plugging ochrana – detail

**Problém:** Pri prepnutí smeru za chodu sa napätie batérie a back-EMF motora sčítajú → prúdový náraz môže dosiahnuť 150A+ a zničiť motor driver.

**Riešenie vo firmware (slew rate limiter + anti-plugging):**
- Firmware sleduje `activeDirection` (posledný aktívny smer motora) a `currentOutput` (aktuálny PWM výstup po slew rate limiteri)
- Koliskový prepínač vždy prechádza cez STOP (1↔0↔2), takže priame FWD↔REV nie je fyzicky možné
- Ak sa požaduje opačný smer:
  1. Ak `currentOutput > 0` → zmena smeru **BLOKOVANÁ**, target = 0, slew rate limiter plynulo znižuje výstup
  2. Ak `currentOutput == 0` → zmena smeru **POVOLENÁ**, DIR pin sa zmení, throttle sa povolí
- Slew rate limiter zabezpečuje plynulé zastavenie cez exponenciálny dobeh (~3.6 s zo 100 % na 0 %)
- Počas dobehu Cytron MD30C regeneruje energiu späť do batérie; po dosiahnutí PWM=0 aktívne brzdí
- Počas blokovania LED pásik **bliká oranžovo** a Serial ukazuje `[BRK!]`

**Typický scenár FWD→REV:** FWD pri 80 % → switch na STOP → exponenciálny dobeh 80→0 (~3.4 s) → switch na REV → `currentOutput==0` → okamžitá zmena smeru → lineárny rozbeh 0→throttle

#### 8.3.5 LED vizuálna spätná väzba
- Postupné rozsvecovanie podľa úrovne throttle
- Farebné kódovanie smeru (napr. zelená=vpred, červená=vzad, modrá=neutál)
- Kalibračný mód – špeciálny blikací pattern
- Chybové stavy – výstražné blikanie

### 8.4 Pseudokód hlavnej slučky

```
setup():
    PWM = 0  // KRITICKÉ: motor stop hneď pri štarte
    init_pins()
    init_neopixel()

    if not forced_recalibration_requested() and eeprom_calibration_valid():
        load_calibration_from_eeprom()   // magic byte + XOR checksum + min<max + rozsah
    else:
        run_calibration_mode()  // blocking, s LED indikáciou; stará EEPROM cal zostáva
        save_calibration_to_eeprom()  // atomický prepis až po úspechu

loop():
    hall_raw = average(analogRead(HALL_PIN), 10 vzoriek)

    // Mapovanie na throttle rozsah s dead zone
    // INVERT_THROTTLE (default): pokoj = cal_max → 0 %, stlačené = cal_min → 100 %
    throttle = map(hall_raw, cal_max, cal_min, 0, 100)
    throttle = apply_deadzone(throttle, 5%, 95%)

    // Čítanie smeru (koliskový prepínač: 1↔0↔2)
    switchState = read_direction_switch()

    // Určenie cieľovej hodnoty (target)
    if switchState is opposite of activeDirection:
        if currentOutput == 0:
            // Motor stojí → bezpečná zmena smeru
            activeDirection = switchState
            target = throttle
        else:
            // Motor ešte dobieha → BLOKOVANÉ
            target = 0
            show_braking_warning()  // oranžové blikanie LED
    else if switchState == STOP:
        target = 0  // Plynulý dobeh (nie okamžitý stop)
    else:
        activeDirection = switchState
        target = throttle

    // PWM limiter — strop PRED slew rate limiterom
    maxAllowedThrottle = DEFAULT_MAX_PWM_PERCENT      // alebo pot A1 (30–100 %)
    target = min(target, maxAllowedThrottle)

    // Slew rate limiter — lineárny rozbeh, exponenciálny dobeh
    if currentOutput < target:
        currentOutput += (100 * dt / RAMP_UP_TIME)  // lineárne, default 50%/s
    else if currentOutput > target:
        factor = pow(RAMP_DOWN_DECAY, dt / RAMP_DOWN_INTERVAL_MS)
        currentOutput = currentOutput * factor      // exponenciálne, ~3.6s do nuly
        if currentOutput < 2: currentOutput = 0     // snap pod 2%
    currentOutput = clamp(currentOutput, target)

    // Výstup
    set_pwm(currentOutput)
    set_dir(activeDirection)
    update_leds(currentOutput, switchState)

    delay(50)
```

---

## 9. Bezpečnostný systém – Súhrn vrstiev

| Vrstva | Typ | Čo chráni | Ako funguje |
|--------|-----|-----------|-------------|
| 1 | HW | Motor pri výpadku MCU | 2× 10kΩ pull-down na PWM line |
| 2 | HW | Analógový vstup | Pull-down na Hall signáli |
| 3 | HW | Motor driver pri prepólovaní | Cytron MD30C vstavaná ochrana proti reverznému napätiu |
| 4 | HW | Motor driver pri nadprúde | Cytron MD30C vstavaná nadprúdová ochrana |
| 5 | HW | Motor pri PWM=0 | Cytron MD30C regeneratívne brzdenie (aktívne zastaví motor) |
| 6 | FW | Motor pri zlej kalibrácii | Validácia EEPROM dát pri štarte |
| 7 | FW | Motor pri chybe senzora | Kontrola rozsahu hodnôt senzora |
| 8 | FW | Neúmyselný pohyb | Dead zone na spodku rozsahu |
| 9 | FW | Motor pri neutrale | Kontrola pozície prepínača |
| 10 | FW | Zlá kalibrácia | Forced recalibration mechanizmus |
| 11 | FW | **Reverz za chodu (plugging)** | **Anti-plugging: zmena smeru povolená len pri currentOutput==0** |
| 12 | FW | **Náhle zmeny výstupu** | **Slew rate limiter: 2 s lineárny rozbeh, exponenciálny dobeh (~3.6 s do nuly)** |
| 13 | FW | **Neefektívny plný plyn** | **PWM limiter: strop 70 % (alebo pot A1 30–100 %)** |

---

## 10. Konektory a kabeláž

### 10.1 Typ konektorov
- **JST-XH** – Zvolené pre robustnosť a spoľahlivosť v prostredí motokáry (vibrácie)
- Zaisťovací mechanizmus zabraňuje náhodnému odpojeniu
- Štandardné 2.5mm rozteče

### 10.2 Prehľad kabeláže

| Prepojenie | Konektor | Počet vodičov | Signály | Odhadovaná dĺžka | Poznámka |
|------------|----------|--------------|---------|-------------------|----------|
| Ext. 5V zdroj → PCB2 | JST-XH 2-pin | 2 | 5V, GND | neurčená | Zdroj 5V zatiaľ neurčený |
| PCB2 → PCB1 (senzor) | JST-XH 3-pin | 3 | 5V, GND, SIGNAL | krátky (~20cm) | Oba na volante |
| **PCB2 → Motor driver** | **JST-XH 3-pin** | **3** | **PWM, DIR, GND** | **dlhý (~1–2m)** | **Volant → motor (len signály!)** |
| PCB2 → PCB3 (LED) | JST-XH 3-pin | 3 | 5V, GND, DATA | krátky (~20cm) | Oba na volante |
| PCB2 → Prepínač | JST-XH 3-pin | 3 | GND, SW1, SW2 | krátky (~20cm) | Oba na volante |

---

## 11. KiCad 9 – Stav PCB dizajnu

### 11.1 Nástroj
- **KiCad 9** – Open-source EDA nástroj pre PCB dizajn
- Projekt bol generovaný a validovaný

### 11.2 Stav jednotlivých PCB

| PCB | Schéma | Layout | Status |
|-----|--------|--------|--------|
| PCB1 (senzor) | ✅ Vytvorená | Potrebuje finalizáciu | KiCad súbory existujú |
| PCB2 (hlavná) | ✅ Vytvorená (zjednodušená v2) | Potrebuje aktualizáciu pre v2 | Interaktívna schéma vytvorená |
| PCB3 (LED) | ✅ Vytvorená | Potrebuje finalizáciu | KiCad súbory existujú |

### 11.3 PCB2 layout parametre (zjednodušená verzia)
- **Rozmer dosky:** 40 × 55 mm
- **Arduino Nano:** Montáž cez pin headers
- **Kondenzátory:** Umiestnené čo najbližšie k príslušným funkčným blokom
- **Konektory JST-XH:** Na okrajoch dosky pre jednoduchý prístup

### 11.4 Preferencia pri práci s KiCad
- Užívateľ preferuje **generované súbory AJ step-by-step vysvetlenia** pre učenie
- Cieľ je aby študent rozumel čo a prečo sa v KiCad robí

---

## 12. Napájanie – Prehľad

### 12.1 Aktuálny dizajn

Systém má **dva nezávislé napájacie okruhy:**

1. **Výkonový okruh (24V):** Batéria → Cytron MD30C → Motor (pri motore)
2. **Logický okruh (5V):** Externý 5V zdroj → PCB2 + senzor + LED (na volante)

Prepojenie medzi okruhmi: **len signály PWM + DIR + GND** (dlhý kábel volant ↔ motor)

```
═══════════ VOLANT ═══════════          ══════════ PRI MOTORE ══════════

Ext. 5V zdroj ──► PCB2 (J_power)       Batéria 24V ──► Cytron MD30C
(zatiaľ neurčený)    │                  (2× 12V)            │
                [C_bulk 470µF]                          [interný reg.]
                     │                                  [H-bridge]
                5V rail na PCB2                              │
                     │                                       ▼
                ├──► Arduino Nano                        DC Motor
                ├──► Hall senzor
                └──► LED pásik
                     │
                Arduino výstupy ──── dlhý kábel ────► Cytron MD30C
                     │                                       │
                     ├── PWM (D5) ─────────────────────► PWM vstup
                     ├── DIR (D4) ─────────────────────► DIR vstup
                     └── GND ──────────────────────────► GND
```

> **Dôležité:**
> - Cytron MD30C sa napája **priamo z batérie** (24V), nie z 5V railu PCB2
> - Z PCB2 ide k motor driveru **len PWM + DIR + GND** (signálový kábel)
> - **GND musí byť prepojený** medzi PCB2 a Cytronom – bez spoločnej zeme nebudú PWM/DIR signály fungovať
> - Zdroj 5V pre PCB2 je zatiaľ otvorená otázka (datalogger, vlastný step-down, alebo iné riešenie)

### 12.2 Prúdová spotreba (odhad)
| Komponent | Spotreba |
|-----------|----------|
| Arduino Nano | ~20-50 mA |
| SS49E Hall senzor | ~5-10 mA |
| NeoPixel LED (záleží na počte a jase) | 20-60 mA/LED pri plnom jase |
| Pull-down rezistory | zanedbateľné |
| **Celkom (odhad)** | **~100-300 mA** (záleží na LED konfigurácii) |

### 12.3 Pôvodný dizajn (s MP1584EN) – archív
- MP1584EN step-down konvertor: 24-36V vstup → 5V výstup
- Výstupný kondenzátor, vstupný kondenzátor, bootstrap kondenzátor
- Induktor pre step-down topológiu
- Spätnoväzbové rezistory pre nastavenie výstupného napätia
- Schottky dióda
- **Tento dizajn bol nahradený zjednodušenou verziou**

---

## 13. Dokumentácia pre študentov

### 13.1 Vytvorené dokumenty (3 ks)

1. **Prehľad projektu a vysvetlenie komponentov**
   - Čo je MagLev Throttle a ako funguje
   - Vysvetlenie každého komponentu (čo robí, prečo je tam)
   - Princíp Hall efektu a magnetického systému

2. **Podrobný návod na zapojenie a inštaláciu**
   - Wiring diagramy s farebnými označeniami
   - Krok-za-krokom montážny postup
   - Umiestnenie komponentov na motokáre
   - Riešenie bežných problémov

3. **Kompletná dokumentácia firmware s vysvetlením kódu**
   - Riadok-po-riadku vysvetlenie kódu
   - Vysvetlenie kalibračného systému
   - Bezpečnostné funkcie a prečo sú dôležité
   - Ako upraviť parametre (dead zone, LED farby, atď.)

### 13.2 Štýl dokumentácie
- Písané jednoducho, bez zbytočného žargónu
- Kde je technický termín, je aj vysvetlenie
- Príklady z reálneho sveta
- Cieľ: 16-ročný študent dokáže pochopiť a zopakovať

---

## 14. Otvorené otázky a TODO

### 14.1 Kritické (musia sa vyriešiť pred ďalším pokračovaním)

- [x] ~~**Motor driver:**~~ → **VYRIEŠENÉ:** Cytron MD30C, sign-magnitude mode, PWM 0–100%, interné napájanie logiky.
- [x] ~~**Anti-plugging ochrana:**~~ → **VYRIEŠENÉ:** Zmena smeru blokovaná kým currentOutput > 0. Slew rate limiter zabezpečuje plynulé zastavenie.
- [x] ~~**Slew rate limiter:**~~ → **VYRIEŠENÉ:** Lineárny rozbeh (2 s) a exponenciálny dobeh (~3.6 s) pre všetky prechody vrátane STOP.
- [ ] **Zdroj 5V pre PCB2:** Vyriešiť ako napájať Arduino a senzory na volante. Možnosti: datalogger, vlastný step-down z batérie, USB powerbank, alebo iné. Musí stačiť na ~100–300mA.
- [ ] **30A poistka:** Pridať automobilovú poistku medzi batériu a motor driver ako HW ochranu.

### 14.2 Dôležité (pred finálnou výrobou)

- [ ] **KiCad PCB layout finalizácia** pre zjednodušenú PCB2
- [ ] **Aktualizácia KiCad projektu** – všetky 3 PCB v aktuálnej verzii
- [ ] **Výber a nákup konkrétnych neodýmových magnetov** – rozmery, sila
- [ ] **Mechanický dizajn tlačidla** – ako sa horný magnet pohybuje, pružina/návrat
- [ ] **Počet LED na NeoPixel pásku** – ovplyvňuje spotrebu a firmware

### 14.3 Nice-to-have (vylepšenia)

- [ ] **Teplotná kompenzácia** – Hall senzor má teplotnú závislosť
- [ ] **Exponenciálna krivka throttle** – namiesto lineárnej pre lepší pocit z jazdy
- [ ] **Diagnostický režim** – cez sériový port pre debugging
- [ ] **Napäťový monitoring** – kontrola 5V rail napätia
- [x] ~~**Soft-start**~~ → **VYRIEŠENÉ:** Slew rate limiter (2 s ramp-up, voliteľne pot A2 2–4 s)

---

## 15. Technické rozhodnutia a ich zdôvodnenie

| Rozhodnutie | Dôvod |
|-------------|-------|
| Hall senzor namiesto potenciometra | Bezkontaktný = bez opotrebenia, spoľahlivejší, dlhšia životnosť |
| SS49E konkrétne | Lineárny výstup, dobrá citlivosť, 5V kompatibilný, dostupný |
| Arduino Nano | Dostatočný výkon, PWM výstupy, analógové vstupy, jednoduchý na programovanie, veľká komunita |
| JST-XH konektory | Robustné, zaisťovacie, vhodné pre vibrácie motokáry |
| Modulárny 3-PCB dizajn | Jednoduchšie troubleshooting, flexibilnejšie umiestnenie |
| EEPROM kalibrácia | Perzistentná (prežije vypnutie), individuálna pre každé tlačidlo |
| Dual pull-down rezistory | Redundantná bezpečnosť – aj pri zlyhaní jedného rezistora je motor chránený |
| Externé 5V napájanie pre PCB2 | Jednoduchšie, menej komponentov na PCB2 (žiadny step-down). Konkrétny zdroj 5V zatiaľ neurčený |
| 470µF bulk kondenzátor | Kompenzácia úbytku na dlhom kábli, zásobník pre prúdové špičky LED |
| 100nF keramické kondenzátory | Štandardná prax – filtrácia VF šumu pri každom funkčnom bloku |
| Cytron MD30C | Overené parametre (80A peak), vstavaná ochrana (nadprúd, prepólovanie), regeneratívne brzdenie, PWM+DIR kompatibilné, interné napájanie logiky |
| Anti-plugging + slew rate limiter | Plugging efekt (reverz za chodu) spôsobil prúdový náraz ~150A. Slew rate limiter zabezpečuje plynulé zastavenie, anti-plugging blokuje zmenu smeru kým currentOutput > 0 |
| 30A poistka medzi batériou a driverom | Posledná záchrana ak všetko ostatné zlyhá – lacné a jednoduché HW riešenie |

---

## 16. Fyzické umiestnenie na motokáre

### 16.1 Prehľad umiestnenia

```
┌──────────────────────────────────────────────────────────┐
│                        MOTOKÁRA                           │
│                                                           │
│  ┌─── VOLANT ──────────────────────────┐                 │
│  │                                      │                 │
│  │  [PCB1 + Magnety] ← Tlačidlo plynu │                 │
│  │  [PCB2 (Arduino)] ← Hlavná doska   │                 │
│  │  [PCB3 + LED pásik] ← Vizuálna     │                 │
│  │  [Prepínač smeru] ← Na dosah       │                 │
│  │                                      │                 │
│  └──────────────┬───────────────────────┘                 │
│                 │                                         │
│            dlhý kábel (PWM + DIR + GND)                  │
│                 │                                         │
│  ┌──── PRI MOTORE ──────────────────────┐                │
│  │              │                        │                │
│  │  [Cytron MD30C] ← Motor driver       │                │
│  │  [Batéria 2×12V] ← Napájanie motora │                │
│  │  [30A poistka] ← Medzi bat. a MD30C │                │
│  │  [DC Motor] ← Zadná náprava         │                │
│  │                                       │                │
│  └───────────────────────────────────────┘                │
│                                                           │
│  [Ext. 5V zdroj] ← Napájanie PCB2 (zatiaľ neurčené)    │
│                                                           │
└──────────────────────────────────────────────────────────┘
```

> **Poznámka:** Arduino (PCB2), tlačidlo (PCB1) a LED pásik (PCB3) sú všetky na volante — krátke kabelové prepojenia. Cytron MD30C je pri motore — dlhý kábel vedie len signály (PWM + DIR + GND), nie výkonový prúd.

---

## 17. Slovník pojmov

| Pojem | Vysvetlenie |
|-------|-------------|
| Hall efekt | Fyzikálny jav – vodič v magnetickom poli generuje napätie kolmo na prúd a pole |
| PWM | Pulse Width Modulation – riadenie výkonu zmenou šírky impulzov |
| EEPROM | Electrically Erasable Programmable Read-Only Memory – pamäť čo prežije vypnutie |
| Pull-down rezistor | Rezistor k zemi – definuje logickú 0 keď nič iné nebudí signál |
| Dead zone | Oblasť na začiatku rozsahu kde sa malé hodnoty ignorujú (= 0) |
| Ratiometrický | Výstup senzora je pomer voči napájaciemu napätiu (nie absolútna hodnota) |
| JST-XH | Typ priemyselného konektora s 2.5mm rozteč a zaisťovacím mechanizmom |
| NeoPixel / WS2812B | Individuálne adresovateľné RGB LED diódy – riadené jedným dátovým vodičom |
| Step-down konvertor | Menič napätia z vyššieho na nižšie (napr. 36V → 5V) |
| Bulk kondenzátor | Veľký kondenzátor pre energetickú zásobu a stabilizáciu napätia |
| Plugging (reverz za chodu) | Prepnutie H-bridge do opačného smeru zatiaľ čo motor ešte rotuje. Back-EMF sa sčíta s napätím batérie → extrémny prúdový náraz |
| Regeneratívne brzdenie | Brzdenie motora vrátením kinetickej energie späť do batérie (namiesto premeny na teplo) |
| Back-EMF | Spätné elektromotorické napätie – napätie ktoré generuje rotujúci motor (pôsobí proti smeru prúdu) |
| H-bridge | Obvod zo 4 tranzistorov (MOSFET-ov) umožňujúci riadenie DC motora oboma smermi |

---

## 18. Poznámky pre pokračovanie v Claude Code

### 18.1 Prioritné ďalšie kroky
1. 30A automobilová poistka medzi batériu a Cytron MD30C
2. Aktualizovať J_motor na PCB2 z 2-pin na 3-pin (PWM + DIR + GND) v KiCad
3. Finalizovať KiCad PCB layout pre zjednodušenú PCB2
4. Aktualizovať študentskú dokumentáciu pre slew rate limiter a anti-plugging

### 18.2 Konvencie a preferencie
- **Jazyk kódu:** Arduino C/C++
- **PCB nástroj:** KiCad 9
- **Dokumentácia:** Markdown, zrozumiteľná pre 16-ročných
- **Konektory:** JST-XH všade
- **Bezpečnosť:** Vždy redundantná (HW + FW)
- **Vysvetlenia:** Generovať súbory AJ vysvetliť krok za krokom

### 18.3 Čo sa riešilo naposledy

**2026-09-04: Vetva `feature/firmware-efficiency` — efektivita + bezpečnostné fixy**
- Zadanie v `FIRMWARE_ZMENY_INSTRUKCIE.md` je IMPLEMENTOVANÉ (detaily v ňom sú už zastarané, autoritatívny je tento dokument, `CLAUDE.md` a firmware)
- PWM limiter (default 70 %, pot A1), compile-time prepínače `USE_RAMPUP_POT` / `USE_PWM_LIMIT_POT`
- Spojitý exponenciálny dobeh, `DECAY=0.72` za 300 ms (~3.6 s do nuly), snap na 0 pod 2 %
- `INVERT_THROTTLE` — nová orientácia magnetu, pokoj = vysoké ADC
- Prehodené poty: A1 = PWM limiter, A2 = ramp-up
- `analogReadStable()` proti prelievaniu náboja S/H kondenzátora medzi ADC kanálmi
- Bezpečnosť: Hall fault detection (rail + margin), watchdog `WDTO_2S`, EEPROM XOR checksum, stará kalibrácia sa pri rekalibrácii nemaže
- RAM: `F()` makro pre Serial literály (~850 B), odstránený `delayMicroseconds(100)` v ADC averagingu

**2026-03-15: Firmware pre Cytron MD30C + slew rate limiter**
- Firmware prepísaný z TZW-36V-30A-HS na Cytron MD30C (PWM 0–100%, sign-magnitude mode)
- **Implementovaný slew rate limiter:** postupný rozbeh (3s) a dobeh (3s) pre všetky prechody
- **Zjednodušený anti-plugging:** zmena smeru blokovaná kým `currentOutput > 0` (namiesto fixného časového oneskorenia)
- LED pásik ukazuje aktuálny výstup aj počas dobehu v STOP (farba posledného smeru)
- Testovanie na reálnom HW — overené scenáre: rozbeh, dobeh, FWD→STOP→REV, pustenie tlačidla

**2026-03-02: Vyhorenie pôvodného motor drivera**
- Plugging efekt zničil pôvodný motor driver → rozhodnutie prejsť na Cytron MD30C
- Implementovaná anti-plugging ochrana vo firmware

**Predchádzajúce:**
- Zjednodušenie PCB2 – odstránenie step-down konvertora, náhrada 5V vstupom z dataloggera
- Vytvorenie interaktívnej schémy nového obvodu

---

*Dokument vytvorený: 2026-02-28*
*Zdroj: Kompletná história chatov projektu MagLev Throttle*
*Posledná aktualizácia: 2026-09-04 – feature/firmware-efficiency: PWM limiter, exp. dobeh 0.72, INVERT_THROTTLE, poty A1/A2, bezpečnostné fixy*
