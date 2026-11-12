# Raksha wiring and power guide

This file is a signal-level reference. The exact A9G carrier-board revision is unknown, so its power pins and UART voltage must be verified against the board schematic before connection.

## Main ESP32 ↔ A9G

Use UART2 on the ESP32 DevKit:

| ESP32 DevKit | A9G board | Purpose |
|---|---|---|
| GPIO17 / TX2 | A9G RXD | ESP32 sends AT commands |
| GPIO16 / RX2 | A9G TXD | ESP32 receives modem responses |
| GND | GND | Common signal reference |
| Dedicated regulated supply | Exact A9G power input | Power — do not use ESP32 3V3 |

If the A9G UART logic is not 3.3 V compatible, add a suitable bidirectional/one-way level interface as required. Do not assume the breakout contains level shifting.

## Inputs and outputs

| ESP32 pin | Device | Interface |
|---|---|---|
| GPIO27 | PIR OUT or door reed contact | 3.3 V-compatible digital input; configure pull-up/down |
| GPIO26 | Siren driver gate/base | MOSFET/transistor driver with suitable gate/base resistor |
| GPIO25 | Tamper contact (optional) | Digital input |
| GPIO2 | Status LED (optional) | LED plus series resistor |

**Never connect a siren, solenoid, motor or electric strike directly to an ESP32 GPIO.** Use a correctly rated driver, flyback protection for inductive loads, and a separately fused power path. Door hardware requires a manual egress method and mechanical key/override.

## ESP32-CAM

| ESP32-CAM | Connection |
|---|---|
| 5V | Dedicated stable 5 V regulator with adequate transient capability |
| GND | Common reference |
| Wi-Fi | Camera endpoint communicates with local recognition computer |

Avoid powering the camera from an overloaded ESP32 board regulator. Camera brownouts often look like software faults.

## Conceptual power tree

```text
Battery (chemistry + cell count selected for application)
    |
    +-- fuse
    +-- reverse-polarity protection
    +-- battery protection / low-voltage cutoff
           |
           +-- regulated rail for ESP32 DevKit
           +-- regulated 5 V rail for ESP32-CAM
           +-- A9G rail exactly as required by carrier-board manual
           +-- separately fused siren/lock rail if used

All logic grounds share an appropriate reference.
Do not tie unrelated rails together except as their design requires.
```

### Power sizing warnings

- GSM transmission creates high-current bursts. A9G supply sag can cause modem resets or corrupt GPS/SMS behavior.
- Use the A9G board vendor's recommended input voltage, peak current, capacitor placement and wiring guidance.
- Select battery, regulator and fuse only after calculating peak and continuous loads.
- Use a battery chemistry-specific charger/BMS. Never charge a lithium pack with a generic buck converter.
- Add a low-battery warning and hardware cutoff; don't rely on firmware alone for cell protection.

## Safety commissioning sequence

1. Leave all loads disconnected. Check regulator outputs with a multimeter.
2. Check polarity and ground topology.
3. Power and test each module independently with a current-limited supply.
4. Confirm modem registration, SMS, voice and GPS.
5. Test intrusion and siren output with the door lock disconnected.
6. Test fail-safe egress, battery loss and network loss before any installation.
