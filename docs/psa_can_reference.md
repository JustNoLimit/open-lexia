# PSA CAN-Bus Reference — Citroen C5 Mk1 Facelift (Phase 2, Full CAN)

Open-source Lexia-3 / Diagbox replacement reference for **Raspberry Pi Pico 2 W (RP2350) + 2x MCP2515 (8 MHz)**.

This document is the compiled knowledge base for the project, gathered from the
following open-source PSA/Stellantis projects and forums:

| Source | Why it matters |
|---|---|
| `ludwig-v/arduino-psa-diag` (218*) | UDS + KWP2000-over-CAN command sets, ISO-15765-2 transport, ECU ID list, PSA seed/key algorithm, OBD2 pinout (AEE2004/AEE2010/NEA2020) |
| `ludwig-v/arduino-psa-comfort-can-adapter` (71*) | CAN2004 <-> CAN2010 bridge; the source of most CAN-LS/Comfort IDs and signal decoding formulas used here |
| `ludwig-v/psa-seedkey-algorithm` | Reverse-engineered PSA seed/key algorithm (constants `0xB2,0x3F,0xAA` / `0xB1,0x02,0xAB`) |
| `Melnik-Alex/PSA_CAN` (2*) | CAN2010 signal addresses (0x0B6 speed/RPM, 0x0F6 temp/ign, 0x036 brightness...) |
| `autowp/arduino-mcp2515` | Reference MCP2515 driver; canonical CNF1/CNF2/CNF3 register tables for 8/16/20 MHz |
| `0xCAFEDECAF/VanBus`, `morcibacsi/esp32_rmt_van_rx` | Older PSA VAN-bus (pre-2004) — NOT used on C5 Mk1 FL, listed for context |

> **Vehicle scope:** Citroen C5 Mk1 Facelift / Phase 2 (approx. 2004-2008),
> architecture **Full CAN-BUS / AEE2004 / "CAN2004"** (BSI-controlled, *not* the
> older VAN-bus, *not* the newer CAN2010). The C5 Mk1 FL is the boundary case: it
> speaks **KWP2000 over CAN (ISO 14230-3 on CAN, "IS 500k" variant)** as its primary
> diagnostic protocol, with **UDS (ISO 14229)** support on newer ECUs for forward
> compatibility. An open Lexia-3 replacement must speak **both**.
>
> **Naming disambiguation (this is genuinely confusing in the PSA community):**
>
> | What it is | Years | PSA internal code | CAN arch | Common names |
> |---|---|---|---|---|
> | C5 1st gen, pre-facelift | 2000-2004 | (PF3 platform) | CAN2004 / AEE2004 | "C5 Mk1", "C5 phase 1" |
> | **C5 1st gen, facelift — TARGET VEHICLE** | **2004-2008** | **(PF3 platform, Phase 2)** | **CAN2004 / AEE2004** | **"C5 Mk1 Facelift", "C5 Phase 2", colloquially "C5 Mk2"** |
> | C5 2nd gen (all-new) | 2008-2017 | **X7** | CAN2010 / AEE2010 | "C5 Mk2", "C5 Mk3", "C5 X7", "C5 phase 2" (yes, really) |
>
> The user's car is the **middle row**: 1st-generation body, facelifted, PF3 platform,
> **CAN2004**. It is **NOT the X7** — X7 is the all-new 2008+ second generation
> (CAN2010). The confusion arises because both the Mk1 Facelift and the X7 are
> variously called "Mk2" / "Phase 2" depending on the market and forum. When a source
> says "C5 X7" without qualification, assume it means the 2008+ CAN2010 car, **not**
> this project's target. (The `arduino-psa-comfort-can-adapter` uses "C5 X7" loosely
> in its steering-wheel mapping labels for the button layout shared across the
> PF3-facelift and X7 — see section 2.2 note.)

---

## 1. Vehicle CAN architecture

The C5 Mk1 FL has **two CAN buses** plus a diagnostic access point:

| Bus | PSA name | Speed | MPC2515 slot | Content |
|---|---|---|---|---|
| **CAN-HS** | IS / "High Speed" / "Car" | **500 kbps** | MCP2515 #1 (SPI0) | Engine, ABS, gearbox, steering, suspension, instrument panel, alerts |
| **CAN-LS** | HAB / "Comfort" / "Low Speed" / "Entertainment" | **125 kbps** | MCP2515 #2 (SPI1) | BSI comfort, climate, radio/nav, steering-wheel buttons, VIN, clock, dash dimming |
| **Diagnostic** | OBD2 (AEE2004) | 500 kbps (shares IS) | via CAN-HS node | KWP2000/UDS diagnostic frames, OBD2 pins **3 (CAN-H)** and **8 (CAN-L)** |

Notes:
- On **AEE2004 / AEE2010** vehicles (C5 Mk1 FL = AEE2004), the diagnostic CAN is on
  OBD2 **pins 3 and 8** — *not* the standard pins 6/14. The standard 6/14 pinout only
  applies to **NEA2020** (DoIP-era) vehicles. This is the single most common wiring
  mistake when building a PSA diagnostic adapter.
- The diagnostic bus is electrically the IS (500 kbps) bus brought out to the OBD2
  connector. KWP2000-on-IS uses a **short-command** variant (see section 4.3).
- Comfort-bus diagnostics (HAB, 125 kbps) uses the **KWP2000 HAB** command variant.
- A 120-ohm termination resistor is required at each end of each bus. In Lexia-style
  **dump/passive** mode the termination resistor on the adapter must be *disabled*.

---

## 2. Critical CAN IDs (CAN2004 / C5 Mk1 FL)

These IDs come from cross-referencing `ludwig-v/arduino-psa-comfort-can-adapter`,
`prototux/PSA-CAN-RE`, `Melnik-Alex/PSA_CAN`, `autowp`, `JustNoLimit/citroen-can`.
IDs are 11-bit standard frames. "TESTED" = verified on C5 Mk1 FL or Peugeot 207 in
the wild.

### 2.1 CAN-HS (500 kbps) — vehicle dynamics / powertrain / alerts

| ID | Len | Signal | Decode | Verified |
|-----|------|--------|--------|----------|
| `0x0B6` | 8 | **Engine RPM + vehicle speed** | `rpm = ((d[0]<<8)\|d[1]) * 0.125`; `speed_kmh = ((d[2]<<8)\|d[3]) * 0.01` | **TESTED** |
| `0x0E6` | ≤8 | **ABS status** | d0=status(bit6=fault,bit5=active,bit1=fluid); d1-d4=wheel rotations; d5=battery; d6=STT/slope/emergency-braking; d7=checksum | **TESTED** |
| `0x120` | 8 | **Alerts journal** | 3×8-byte blocks: oil, temp, ESP, airbag, ABS, suspension, doors, lights, tyre | **TESTED** |
| `0x168` | 8 | **Warning lights** | d0: bit7=CheckEng, bit6=STOP, bit5=oil, bit4=batt, bit3=handbrake, bit2=ABS; d6 bit7-5=gearbox, bit6=EMF avail | **TESTED** |
| `0x128` | 8 | **Gearbox display** | d6 bits4-7=gear pos (0=P,1=R,2=N,3=D,4-9=6-1); d7 bits4-6=auto mode; d0 bit5=handbrake | **TESTED** |
| `0x217` | 8 | **Cluster status / buttons** | Dimmer, dash button presses (AAS, AC, airbag, blindspot, brightness, auto-check, black-panel) | **TESTED** |
| `0x072` | 8 | **Immobilizer query (ECU→BSI)** | Engine ECU requests immo authentication | COMMUNITY |
| `0x0A8` | 8 | **Immobilizer response (BSI→ECU)** | BSI validates key, enables start | COMMUNITY |
| `0x108` | 8 | **Engine ECU heartbeat** | General status + comm heartbeat | COMMUNITY |
| `0x208` | 4+ | **Engine Dynamic Data 1** | d0=gas pedal % × 0.392; d[1:2]=torque Nm | COMMUNITY |
| `0x305` | 3 | **Steering angle (ESP)** | d[0:1]=steering angle sensor | COMMUNITY |
| `0x348` | 8 | **Engine Dynamic Data 2** | Turbo boost, manifold air temp, injection rate | COMMUNITY |
| `0x34D` | 4+ | **ESP/ASR intervention** | Wheel-slip state, ESP OFF button | COMMUNITY |
| `0x38D` | 4+ | **Yaw rate** | Lateral G-force, yaw rate sensor | COMMUNITY |
| `0x44D` | 8 | **Wheel speeds (ABS)** | d[0:1]=FL; d[2:3]=FR; d[4:5]=RL; d[6:7]=RR × 0.01 km/h | COMMUNITY |
| `0x468` | 8 | **Engine Dynamic Data 3** | Oil temp, EGT, DPF soot % | COMMUNITY |
| `0x488` | 3+ | **Engine General Data** | d0=coolant = d[0]-40; d1=oil temp °C | COMMUNITY |
| `0x0E1` | 7 | **Parking-aid beeps (AAS)** | Beep direction L/R, duration | COMMUNITY |
| `0x15B` | — | (do-not-bridge marker) | Internal cross-network marker | — |

### 2.2 CAN-LS (125 kbps) — comfort / body / infotainment

| ID | Len | Signal | Decode | Verified |
|-----|------|--------|--------|----------|
| `0x036` | 8 | **Economy mode + brightness** | `econ = bit(d[2],7)`; `brightness = d[3]` (0x20..0x2F=dimmed) | **TESTED** |
| `0x0F6` | 8 | **BSI Data Slow + SWC** | `ign = d[0]>128`; SWC: d0 bits 0/1/2/3=V+/V-/Seek+/Seek-, bit4=Source; `coolant = d[1]-40`; `odo = (d[2]<<16)\|(d[3]<<8)\|d[4]`; fuel=d[5]; `ext_temp = (d[6]/2)-40` | **TESTED** |
| `0x1E1` | 2+ | **Ignition & Power** | d0: 0x00=off, 0x01=ACC, 0x04=on, 0x08=cranking; d1=economy mode | COMMUNITY |
| `0x1D0` | 7 | **Climate control** | d0=mode(0x00=auto+AC,0x02=AC off,0xA2=off); d2=fan(0x0F=off,0-7=spd1-8); d3=distribution(0x10=none,0x20=feet,0x40=face,0x80=all); d4=recycle(0x30)/demist(0x10); d5=left setpoint; d6=right setpoint | **TESTED** |
| `0x21F` | 3 | **Radio stalk buttons** | d0: bit1=Source, bit2=Vol-, bit3=Vol+, bit6=Seek-, bit7=Seek+; d1=scroll | **TESTED** |
| `0x0A2` | 5 | **COM2000 buttons** | d1: bit2=MODE/NAV, bit3=MENU, bit4=ESC/APPS, bit5=OK/PHONE | **TESTED** |
| `0x122` | 8 | **FMUX buttons (emulated)** | Forged menu/volume forwarded to head unit; d5=0x02 source tag | **TESTED** |
| `0x1A1` | 8 | **Door & Light Status** | d0 bits: 0=driver,1=pass,2=RL,3=RR,4=trunk,5=bonnet; d1=lights (park/low/high/turn) | **TESTED** |
| `0x1A8` | 8 | **Cruise setpoint** | d1=setpoint value | **TESTED** |
| `0x221` | 8 | **Trip consumption + range** | d[3:4]=range km; d[1:2]=consumption×0.1 L/100km | **TESTED** |
| `0x228` | 2 | **Clock** | d0=hour(bits0-4), d1=minute(bits0-5) | **TESTED** |
| `0x336` | 3 | **VIN chars 1-3 (ASCII)** | WMI e.g. VF7 | **TESTED** |
| `0x3B6` | 6 | **VIN chars 4-9 (ASCII)** | OEM + serial prefix | **TESTED** |
| `0x2B6` | 8 | **VIN chars 10-17 (ASCII)** | Last 8 of VIN | **TESTED** |
| `0x3A7` | 7 | **Service indicator** | d[3:4]=km remaining ×20; d[5:6]=days remaining; d0 bit7=service due | **TESTED** |
| `0x3E5` | 6 | **EMF menu navigation** | d0: bit6=MENU, bit4=phone, bit0=AC; d1: bit6=TRIP, bit4=mode, bit0=audio; d2: bit6=OK, bit4=ESC; d5: up/down/right/left | **TESTED** |
| `0x2A5` | 8 | **RDS Radio Text** | 8 ASCII bytes, station name / RDS scroll | **TESTED** |
| `0x225` | 4+ | **Radio Tuner** | d2 bits4-7=band (0=FMAST,1=FM1,2=FM2,3=FMAST,4=AM); freq = 50 + ((d[3]\|d[4]<<8)) × 0.05 MHz | **TESTED** |
| `0x125` | 8 | **Radio/CD Text List** | Track + author names (20 ASCII bytes each) | **TESTED** |
| `0x0A4` | 8 | **Radio Status** | Power state, source (AM/FM/CD/AUX) | COMMUNITY |
| `0x165` | 7 | **Radio Sound Mode** | Bass/treble/EQ + routing | **TESTED** |
| `0x1A5` | 2 | **Volume Level** | d0=0-30 | **TESTED** |
| `0x0D6` | 5+ | **Gearbox position** | P,R,N,D,M + Sport/Snow mode | COMMUNITY |
| `0x220` | 8 | **Door / Hood / Flap** | Per-door open bits | **TESTED** |
| `0x227` | 8 | **Dashboard button LEDs** | AC-on, recycle, child-lock, ESP, AAS, overspeed, fuel-info | **TESTED** |
| `0x2E1` | ? | **BSI Menu Config + Suspension** | Auto wipers, DRL, follow-me-home; suspension pos + auto-lock | **TESTED** |
| `0x361` | ? | **Personalisation** | Ambient, auto-light, blindspot, AAS, auto e-brake | **TESTED** |
| `0x5E5` | 8 | **EMF / display version** | HW/SW version of dash display | **TESTED** |
| `0x350` | 8 | **Climate → CAN2010 forge** | Re-encoded climate for CAN2010 head units | REFERENCE |
| `0x1E0` | 2+ | **Cruise stalk buttons** | SET+, SET-, RES, PAUSE (separate from setpoint 0x1A8) | COMMUNITY |
| `0x0E1` | 7 | **Parking-aid beeps (AAS)** | Beep direction L/R, duration (on LS bus) | COMMUNITY |
| `0x3F6` | 4+ | **Clock & Date Config** | System clock/date sync from EMF/BSI | COMMUNITY |
| `0x261` | 7 | **Trip Memory 2** | Distance, avg consumption, avg speed | **TESTED** |
| `0x2A1` | 7 | **Trip Memory 1** | Distance, avg consumption, avg speed | **TESTED** |
| `0x0E8` | ? | **Doors + auto lights/wiper** | Auto-light & wiper alert enable, alarm, doors (unverified layout) | UNVERIFIED |
| `0x161` | 8 | **Oil + Fuel Level** | Oil temp, oil level, fuel level (unverified layout) | UNVERIFIED |

### 2.3 CAN2010 IDs (for cross-reference / newer head units)

From `Melnik-Alex/PSA_CAN` (CAN2010, *not* native to C5 Mk1 FL but useful when
retrofitting CAN2010 head units like NAC/SMEG via the comfort-adapter bridge):

| ID | Len | Signal |
|---|---|---|
| `0x0E6` | 8 | Impulse generator / voltage / timer |
| `0x036` | 8 | Eco / on-off / brightness |
| `0x0B6` | 8 | Speedometer + tachometer (same ID, CAN2010 encoding) |
| `0x0F6` | 8 | Engine temp + odometer |
| `0x169` | 8 | Navigation info (compass etc.) |
| `0x2E9` | 4 | Dashboard theme / personalisation |
| `0x128` | 2 | Radio OFF marker (`01 30`) |
| `0x328` | 6 | Radio ON / RDS state |

---

## 3. Diagnostic ECU address table (KWP2000-over-CAN / UDS)

Diagnostic frames use an **(EMIT_ID : RECV_ID)** pair. The tester sends to EMIT_ID and
listens on RECV_ID. Source: `ludwig-v/arduino-psa-diag/ECU_LIST.md` (full list in that
repo; below is the subset relevant to C5 Mk1 FL).

| ECU family | Example ECUs | EMIT : RECV | Notes |
|---|---|---|---|
| **INJ** (engine) | EDC16C3, HDI_SID803, MM6LP, DCM34, ME745... | `0x6A8 : 0x688` | **Engine ECU** — primary diagnostic target |
| **BMF** (BSI) | BSI, BSI2010, BCM... | `0x752 : 0x652` | **BSI** — central gateway, default ECU in ludwig-v sketch |
| **ABRASR** (braking) | ABS81, ESP81, ESP8_BOSCH, ABSMK100... | `0x6AD : 0x68D` | **ABS / ESP** |
| **AIRBAG** | SAC_AUTOLIV, RBG_UDS, SRS, ORC... | `0x744 : 0x644` | **Airbag / SRS** |
| **CLIM** (climate) | CLIM_REGULEE, BCC, HVAC, ACM... | `0x76D : 0x66D` | **Climate control** |
| **COMBINE** (dash) | COMBINE, CIROCCO, MET, IPC... | `0x75F : 0x65F` | **Instrument cluster** |
| **DIRECTN** (steering) | GEP, DAE, EPS, SSCU... | `0x6B5 : 0x695` | **Electric power steering** |
| **HDC** (wheel controls) | COM2000, COM2008P, COM2016, TCM... | `0x742 : 0x642` | **Steering-wheel COM2000** |
| **BOITEVIT** (gearbox) | BVA_AL4, BVMP, BVA_AM6, BVA_ZF... | `0x6A9 : 0x689` | **Automatic gearbox** |
| **SPNEU** (suspension) | SUSPENSION | `0x6B8 : 0x698` | **Hydractive suspension** (C5-specific) |
| **DSG** | DSG, TPMS | `0x6AF : 0x68F` | Tyre pressure |
| **TELEMAT** (nav/radio) | RT3, RD4, NAC, SMEG, RADIONAV_RNEG... | `0x764 : 0x664` | **Telematic / nav** |
| **AUTORADIO** | RD45, RD43, RD6, RDE... | `0x760 : 0x660` | **Radio** |
| **AMPLHIFI** | AMPLI_AUDIO | `0x77D : 0x67D` | Amplifier |
| **CPL** (rain/light) | PLUIE_LUMINOSITE, CDPL... | `0x74A : 0x64A` | Rain/light sensor |
| **BML** (lighting) | BML, SDCM | `0x741 : 0x641` | Lighting control |
| **ADC** (immobiliser) | IMMO, WCM, KOS... | `0x703 : 0x70B` | Immo / key recognition |

LIN sub-node access (LIN-over-UDS): supply the LIN ECU code after the ID pair, e.g.:
```
>736:716
L47        # LVDS screen DGT7CFF
```

---

## 4. Diagnostic protocols

### 4.1 ISO 15765-2 (ISO-TP) transport layer

CAN frames carry max 8 bytes; diagnostic PDUs can be longer. PSA uses standard
ISO-15765-2 segmentation (identical to general automotive):

**Single frame (PDU <= 7 bytes):**
```
Byte0      = PCI = total data length (0x01..0x07)
Byte1..7   = data
```

**First frame (PDU > 7 bytes):**
```
Byte0      = 0x10 | (total_len >> 8)
Byte1      = total_len & 0xFF
Byte2..7   = data[0..5]   (6 bytes)
```
Receiver then sends a **flow-control frame**:
```
Byte0 = 0x30   (Clear To Send)
Byte1 = 0x00   (block size = 0 = all remaining)
Byte2 = delay between consecutive frames (ms)
```
Then consecutive frames:
```
Byte0 = 0x21..0x2F  (sequence, wraps 0x2F -> 0x20)
Byte1..7 = data[6..], data[13..], ...
```
> Received consecutive frames **may arrive out of order**; reassemble by sequence
> byte using the known total length. The ludwig-v sketch flushes in 0x2F batches to
> avoid serial-buffer overrun on small MCUs.

**LIN-over-UDS encapsulation:** when addressing a LIN sub-node, every CAN frame gets
a 1-byte LIN node address prefix (range `0x40..0x70`) in Byte0, shifting the ISO-TP
PCI to Byte1. Strip/insert it at the transport boundary.

### 4.2 UDS (ISO 14229) — primary on CAN2010+, partial on C5 FL newer ECUs

| Command | Service | Description |
|---|---|---|
| `3E00` | TesterPresent | Keep-alive (send every ~2-4s) |
| `1003` | DiagnosticSessionControl | Open diagnostic (coding) session |
| `1002` | DiagnosticSessionControl | Open download session |
| `1001` | DiagnosticSessionControl | End communication |
| `2703` | SecurityAccess | Seed — configuration access |
| `2704 XXXXXXXX` | SecurityAccess | Key — configuration access |
| `2701` / `2702 XXXXXXXX` | SecurityAccess | Download seed / key |
| `22 XXXX` | ReadDataByIdentifier | Read zone (2-byte ID) |
| `2E XXXX YYYY...` | WriteDataByIdentifier | Write zone (must be unlocked) |
| `190209` | ReadDTCInformation | List current faults |
| `14FFFFFF` | ClearDiagnosticInformation | Clear all faults |
| `3101FF0081F05A` | RoutineControl | Erase flash for .cal (F05A = tool signature) |
| `3103FF00` | RoutineControl | Erase flash |
| `3481110000` | RequestDownload | Prepare .cal flash write |
| `348244 XX.. YYYY..` | RequestDownload | Full-UDS .ulp block (addr+size) |
| `37` | RequestTransferExit | Flash autocontrol (must be unlocked) |
| `1103` | ECUReset | Reboot |

UDS positive responses: `50` (session), `62` (read), `6E` (write), `67` (security),
`54` (clear), `77` (flash autocontrol OK), `7103FF00xx` (erase progress), `76XX02`
(frame injected; `XX` = block sequence). Negative response: `7F <service> <NRC>`
(e.g. `7F2233` read-of-protected-zone-while-locked, `7F2736` too many attempts,
`7F2E7E` locked, `7FXX78` in-progress). *(Corrected against ludwig-v/arduino-psa-diag:
the earlier `7600xx02` / `7F2733` here were transcription errors.)*

### 4.3 KWP2000 over CAN (ISO 14230-3 on CAN) — **primary for C5 Mk1 FL**

PSA runs KWP2000 in **two flavours**; both must be supported:

**(a) KWP on HAB (Comfort / 125 kbps)** — long-form commands:
| Command | Description |
|---|---|
| `3E` | Keep-alive |
| `10C0` | Open diagnostic session |
| `1081` | End communication |
| `2781` / `2782 XXXXXXXX` | Download seed / key |
| `2783` / `2784 XXXXXXXX` | Config seed / key |
| `21 XX` | Read zone (1-byte ID) |
| `3B XX YYYY...` | Write zone |
| `17FF00` | List faults |
| `14FF00` | Clear faults |
| `31A800` / `31A801` | Reboot / reboot 2 |
| `318181F05A` | Erase flash for .cal |

Positive responses: `50C0` (session), `61XX...` (read), `57` (DTC list:
`57 <n> <dtc_hi> <dtc_lo> <status>`), `6781/6783 XXXXXXXX` (seed), `6782/6784` (unlocked).

**(b) KWP on IS (High-Speed / 500 kbps)** — **short-form** commands (C5 FL engine/ABS/etc.):
| Command | Description |
|---|---|
| `3E` | Keep-alive |
| `81` | Open diagnostic session |
| `82` | End communication |
| `2781` / `2782 XXXXXXXX` | Download seed / key |
| `2783` / `2784 XXXXXXXX` | Config seed / key |
| `21 XX` | Read zone (1-byte ID) |
| `34 XX YYYY...` | Write zone |
| `17FF00` | List faults |
| `14FF00` | Clear faults |

Positive responses: `C1XXXX` (session opened), `C2` (closed), `61XX...` (read),
`57..` (DTC), `6781/6783 XXXXXXXX` (seed). Same negative-response scheme.

> The short/long KWP split is the key C5-Mk1-FL gotcha: the **same service** has a
> **different byte encoding** depending on which bus (IS vs HAB) the ECU lives on.
> The architecture must keep the protocol variant on a per-ECU basis.

### 4.4 PSA Seed/Key algorithm (SecurityAccess)

Each ECU has a 16-bit **unlock key** (`pin`). The tester requests a 32-bit **seed**
via `2701/2703` (UDS) or `2781/2783` (KWP), then computes a 32-bit **key** and sends
it back via `2702/2704` / `2782/2784` within **5 seconds**. Reverse-engineered
algorithm (from `ludwig-v/psa-seedkey-algorithm`, confirmed against CIROCCO firmware):

```cpp
long transform(byte msb, byte lsb, const byte sec[3]) {
    long data = (msb << 8) | lsb;
    long r = ((data % sec[0]) * sec[2]) - ((data / sec[0]) * sec[1]);
    if (r < 0) r += (sec[0] * sec[2]) + sec[1];
    return r;
}
// Constants are fixed across the PSA family:
static const byte SEC_1[3] = {0xB2, 0x3F, 0xAA};
static const byte SEC_2[3] = {0xB1, 0x02, 0xAB};

uint32_t compute_key(uint16_t pin, uint32_t seed) {
    long r_msb = transform(pin>>8, pin&0xFF, SEC_1)
              | transform(seed>>24, seed&0xFF, SEC_2);
    long r_lsb = transform((seed>>16)&0xFF, (seed>>8)&0xFF, SEC_1)
              | transform(r_msb>>8, r_msb&0xFF, SEC_2);
    return (uint32_t)((r_msb << 16) | (r_lsb & 0xFFFF));
}
```
The per-ECU `pin` (2 bytes) is listed in
`ludwig-v/psa-seedkey-algorithm/ECU_KEYS.md` and is also recoverable by brute force
from a couple of seed/key pairs captured from Diagbox (only 65536 possibilities).

### 4.5 Secured Traceability (mandatory after every write)

After any configuration write, a traceability zone must be written or the ECU logs a
DTC (`B1003` on UDS, `F303` on KWP):

- **UDS:** `2E2901 FD 000000 01 01 01` (zone 2901, site=Aftersales, signature=factory,
  date=01/01/2001). A non-rewritable counter lives in zone `C000` (`22C000` -> `62C000 2A 00`).
- **KWP:** `3BA0 FF FD 000000 01 01 01 00 00` (zone A0). The trailing bytes are `00 00`
  (the ECU auto-increments its own secured-write counter); the earlier `2A 00` here
  conflated the read-back counter example with the write template.

### 4.6 Calibration files (.cal / .ulp)

Motorola S-record format. Frame types: **S0** (hardware info: family mux code, ISO
line, inter-byte/inter-frame timings, cal type 0x81=.cal/0x82=.ulp), **S1** (ZI
identification: flash signature, unlock key, supplier, system/app/sw versions), **S2**
(zone data blocks), **S3** (binary data blocks), **S5** (frame count), **S7/S8/S9**
(end markers). Checksums: `CRC-16/X-25` over data (stored `CRC[1]CRC[0]`), then
`CRC-8/2s-complement` over address+zone+data+crc, minus 1. A flash/upload engine is
out of scope for the sniffer MVP but the file format is documented for later.

---

## 5. MCP2515 (8 MHz) bit-timing configuration

The `autowp/arduino-mcp2515` library is the de-facto reference driver and ships
canonical CNF presets. For an **8 MHz** crystal (the cheap blue MCP2515 modules):

| Bitrate | CNF1 | CNF2 | CNF3 | BRP | TQ/bit | Sync | Prop | Phase1 | Phase2 | Sample point |
|---|---|---|---|---|---|---|---|---|---|---|
| **125 kbps** | `0x01` | `0xB1` | `0x85` | 2 | 16 | 1 | 2 | 7 | 6 | 62.5% |
| **500 kbps** | `0x00` | `0x90` | `0x82` | 1 | 8  | 1 | 1 | 3 | 3 | 62.5% |

Decoding (MCP2515 datasheet sec. 5.x):
- `TQ = 2 * (BRP + 1) / Fosc`  -> 8 MHz, BRP=0: 250 ns ; BRP=1: 500 ns
- `CNF1 = (SJW<<6) | BRP` ; `CNF2 = (BTLMODE<<7) | (SAM<<6) | (PHSEG1<<3) | PRSEG` ;
  `CNF3 = (SOF<<7) | (WAKFIL<<6) | PHSEG2`
- Both presets set **SAM=0** (sample once, best for high speed), **SOF=1**, and
  **BTLMODE=1** (PHSEG2 from CNF3). Sample point 62.5% matches PSA ECUs and is the
  value used by Lexia / Diagbox, so bus-side interop is proven.

Decode of the two presets:
- **125k:** BRP=1 (prescaler 2 -> 500 ns/TQ), PRSEG=1 (2 TQ), PHSEG1=6 (7 TQ),
  PHSEG2=5 (6 TQ) -> 16 TQ -> 8 us/bit -> 125 kbps.
- **500k:** BRP=0 (prescaler 1 -> 250 ns/TQ), PRSEG=0 (1 TQ), PHSEG1=2 (3 TQ),
  PHSEG2=2 (3 TQ) -> 8 TQ -> 2 us/bit -> 500 kbps.

These are the values used in our driver (`src/mcp2515.cpp`). Do **not** roll your own
CNF values — these are interop-tested against real PSA ECUs.

**SPI clock:** up to 10 MHz at the MCP2515 (datasheet max). On the Pico, run SPI at
**~8-10 MHz** for the 500 kbps bus, **~4-8 MHz** is fine for 125 kbps. The autowp
driver default is 10 MHz. Keep SPI clock <= Fosc of the MCP2515 to be safe.

**Wiring per MCP2515 module:**
- `VCC` -> 5 V (modules with 3.3 V reg) *or* 3.3 V (bare board). The Pico GPIO is
  3.3 V — **verify the module's MISO is 3.3 V-safe**; most blue modules have a 3.3 V
  regulator and 5 V-tolerant logic but MISO is pulled to VCC. Power the module from
  5 V and level-shift MISO, *or* use a 3.3 V MCP2515 board. Do not drive 5 V into
  Pico pins.
- `CS`  -> Pico GPIO (CS0 / CS1 below)
- `INT` -> Pico GPIO (edge-triggered; optional, polling also works)
- `SCK/MOSI/MISO` -> the SPI peripheral's pins
- CAN-H / CAN-L -> the bus pair, 120 R termination at this node if end-of-line.

---

## 6. Dual MCP2515 on Raspberry Pi Pico 2 W (RP2350)

The RP2350 (like the RP2040) exposes **two independent SPI peripherals**, `spi0` and
`spi1`, each routable to several GPIO banks through the IO mux. Putting one MCP2515
on each peripheral gives **zero bus contention** (no CS scheduling, no shared-clock
glitches) and lets both be serviced at full SPI speed simultaneously.

### 6.1 Recommended pin assignment (no conflict)

| Signal | MCP2515 #1 (CAN-HS, 500 kbps) | MCP2515 #2 (CAN-LS, 125 kbps) |
|---|---|---|
| SPI peripheral | `spi0` | `spi1` |
| SCK  | GP2  | GP10 |
| MOSI | GP3  | GP11 |
| MISO | GP4  | GP12 |
| CS   | GP5  | GP13 |
| INT  | GP6  | GP14 |
| (optional) RESET | GP7 | GP15 |

Why these pins: GP2-GP5 are the canonical `spi0` Tx/Rx/Sck/Csn on the Pico pinout,
and GP10-GP13 are the canonical `spi1` set. They share no GPIO, no IRQ line, and
leave GP0/GP1 free for UART debug and the USB/LED lines untouched. INT pins use
separate GPIO so each MCP2515's RX interrupt can be handled independently (the
RP2350 supports edge-triggered GPIO IRQs on any pin).

### 6.2 Pico SDK notes

- Initialise each SPI with `spi_init(spi0, 8'000'000)` / `spi_init(spi1, 8'000'000)`,
  `spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST)`.
- `gpio_set_function(pin, GPIO_FUNC_SPI)` for SCK/MOSI/MISO; `gpio_init` + pull-up
  on CS; `gpio_set_dir_in` + `gpio_set_irq_enabled_with_callback` on INT.
- The RP2350 has two cores; a clean split is **core 0 = USB/UART command interface +
  diagnostic state machine**, **core 1 = CAN RX pump** (poll both MCP2515 INT pins,
  push frames into a SPSC ring buffer). This mirrors ludwig-v's `readCAN`/`parseCAN`
  thread split but with hardware concurrency instead of `ThreadController`.
- MCP2515 RESET can be done in software via the `INSTRUCTION_RESET` (0xC0) SPI
  command; a hard-wired RESET GPIO is optional but speeds cold-boot recovery.

### 6.3 Filter strategy for sniffing

For a **sniffer**, set both RX buffers to accept all standard frames:
- `RXBnCTRL_RXM = 00` (STDEXT), masks = 0, filters = 0 -> receive everything.
- For **targeted diagnostics**, program RXF0/RXF1 to the single RECV_ID of the ECU
  you are talking to, mask = 0x7FF, to cut interrupt load.

---

## 7. Architecture mapping (how this reference feeds the code)

| Reference section | Code module |
|---|---|
| ECU address table (sec. 3) | `include/psa/psa_protocol.hpp` — `kEcuTable` |
| KWP/UDS services (sec. 4.2-4.3) | `include/psa/psa_protocol.hpp` — service constants + request builders |
| ISO-TP transport (sec. 4.1) | `include/psa/isotp.hpp` + `src/isotp.cpp` |
| Seed/key (sec. 4.4) | `include/psa/psa_protocol.hpp` — `seed_key::compute` |
| MCP2515 CNF presets (sec. 5) | `include/psa/mcp2515.hpp` + `src/mcp2515.cpp` |
| Dual-SPI wiring (sec. 6) | `include/psa/can_manager.hpp` + `src/can_manager.cpp` |
| Sniffer + diag passthrough | `src/main.cpp` |

Extensibility hooks (explicitly requested): new ECU = add a row to `kEcuTable`;
new diagnostic service = add a constant + builder in `psa_protocol.hpp`; new
transport (e.g. DoIP for NEA2020) = add a class parallel to `IsoTp`. The
KWP-vs-UDS and IS-vs-HAB protocol variants are first-class so a Diagbox/PyPSADiag
passthrough can drive either without touching the transport or driver layers.

---

## 8. Sources

- https://github.com/ludwig-v/arduino-psa-diag  (ECU_LIST.md, README.md, sketch source)
- https://github.com/ludwig-v/arduino-psa-comfort-can-adapter  (CAN2004 IDs, signal decode)
- https://github.com/ludwig-v/psa-seedkey-algorithm  (seed/key constants + assembly trace)
- https://github.com/Melnik-Alex/PSA_CAN  (CAN2010 signal addresses)
- https://github.com/autowp/arduino-mcp2515  (mcp2515.h CNF presets, driver reference)
- https://github.com/0xCAFEDECAF/VanBus , https://github.com/morcibacsi/esp32_rmt_van_rx  (VAN-bus, pre-2004 PSA, context only)
- MCP2515 datasheet (Microchip DS21801) sec. 5 — bit timing
- ISO 15765-2 (ISO-TP), ISO 14229 (UDS), ISO 14230-3 (KWP2000)
