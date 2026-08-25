# System-Check

Manual verification Arduino sketches for HEPTA-SAT Full.

The staff full kit (station automation and Python tools) lives in a separate repository:  
https://github.com/HEPTA-SAT-TRAINING/Lab99_Check_Kit_Staff

## Contents

| Path | Description |
| --- | --- |
| `System-Check.ino` | Manual verification sketch |
| `empty_sketch/empty_sketch.ino` | Idle sketch left after inspection (keeps USB CDC) |
| `src/` | [HEPTA-SAT-Library](https://github.com/HEPTA-SAT-TRAINING/HEPTA-SAT-Library) submodule |

## Setup

```bash
git clone --recurse-submodules https://github.com/HEPTA-SAT-TRAINING/System-Check.git
cd System-Check
```

If the repo is already cloned:

```bash
git submodule update --init --recursive
```

Open `System-Check.ino` in the Arduino IDE and upload it for HEPTA-SAT Full (RP2040).

## Usage

Use the peer XBee on the PC with [HEPTA-SAT-Serial_Monitor](https://hepta-sat-training.github.io/HEPTA-SAT-Serial_Monitor/) (baud **38400**). Progress also appears on USB (`[CDH]`, 9600 baud).

1. Connect the peer XBee, open HEPTA-SAT-Serial_Monitor, and wait for `BOOT READY`
2. The kit asks for **DATE**, **KIT** name, and **OPERATOR** (any text, one field at a time). Type each value and press **Send**
   - EOL may be **None** (the whole field is sent at once) or **LF** / **CRLF**
3. Those fields are printed in the log so **Save Log** keeps them with the test output
4. All tests start automatically (same as command `a`). When finished, send `a` or individual commands to re-run

### Commands

| Command | Action |
| --- | --- |
| `a` | Run all tests (also runs automatically after session metadata; XBee RX test last) |
| `l` | Blink OBC board LEDs (`HEPTA_OBC_LED1`–`3`) |
| `e` | EPS voltages |
| `i` | Current sense / ammeter (ISOL / IBUS / ICHG). Shine light on the solar panel and watch ISOL change |
| `t` | Temperature |
| `m` | IMU |
| `s` | SD read/write |
| `c` | Camera capture (JPEG to SD) |
| `g` | GPS NMEA sentence present (FIX not required) |
| `n` | XBee AT identity |
| `p` | Interactive XBee RX test |

### Session header (DATE / KIT / OPERATOR)

On boot the kit waits for three strings from the XBee uplink (HEPTA-SAT-Serial_Monitor). Empty input is stored as `-`. After **OPERATOR** is entered, all tests start automatically. The session header is logged again at the start of the run:

```text
From Sat: SESSION DATE=2026-08-25 KIT=KIT-04 OPERATOR=n_mas
```

Use **Save Log** in HEPTA-SAT-Serial_Monitor to keep this header with the later test lines.

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
