# Lab99 Check Kit

Manual verification Arduino sketches for HEPTA-SAT Full.

The staff full kit (station automation and Python tools) lives in a separate repository:  
https://github.com/HEPTA-SAT-TRAINING/Lab99_Check_Kit_Staff

## Contents

| Path | Description |
| --- | --- |
| `Lab99_Check_Kit.ino` | Manual verification sketch |
| `empty_sketch/empty_sketch.ino` | Idle sketch left after inspection (keeps USB CDC) |
| `src/` | [HEPTA-SAT-Library](https://github.com/HEPTA-SAT-TRAINING/HEPTA-SAT-Library) submodule |

## Setup

```bash
git clone --recurse-submodules https://github.com/HEPTA-SAT-TRAINING/Lab99_Check_Kit.git
cd Lab99_Check_Kit
```

If the repo is already cloned:

```bash
git submodule update --init --recursive
```

Open `Lab99_Check_Kit.ino` in the Arduino IDE and upload it for HEPTA-SAT Full (RP2040).

## Usage

1. Connect the kit over USB and open the Serial Monitor at **9600 baud**
2. If a peer XBee is connected to your PC, progress also appears there (`From Sat:`)
3. Send a one-character command, then press Enter

### Commands

| Command | Action |
| --- | --- |
| `a` | Run all tests (XBee RX test last) |
| `l` | Blink board LEDs (pins 25 / 29 / 24) |
| `e` | EPS voltages |
| `i` | Current sense / ammeter (ISOL / IBUS / ICHG). Shine light on the solar panel and watch ISOL change |
| `t` | Temperature |
| `m` | IMU |
| `s` | SD read/write |
| `c` | Camera capture (JPEG to SD) |
| `g` | GPS NMEA sentence present (FIX not required) |
| `n` | XBee AT identity |
| `p` | Interactive XBee RX test |

### Distinguishing progress output

The same progress messages go out on two paths.

| Path | Prefix | Where to read it |
| --- | --- | --- |
| USB (CDH) | `[CDH]` | Arduino Serial Monitor |
| XBee (COM) | `From Sat:` | Peer XBee on the PC |

### XBee RX test (`p`)

1. Send `p`
2. `[CDH]` / `From Sat:` ask you to send a command from the PC via XBee
3. Send any single character (or similar) from the peer XBee
4. When the kit receives it, you get `XBEE_RX: OK`

If nothing arrives within 30 seconds, the result is NG. The XBee pair must already be configured.

### Current sense / ammeter (`i`)

Samples currents for about five seconds. Indoors, `IBUS` is usually greater than zero. Confirm that `ISOL` rises when you shine light on the solar panel. If all currents stay stuck at zero, check wiring, the MCP3208, and the shunt path.

## Empty sketch

To leave an idle firmware after inspection, upload `empty_sketch/empty_sketch.ino`. USB remains available as a COM port.
