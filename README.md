# System-Check

Manual verification Arduino sketches for HEPTA-SAT Full.

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

Operate **only** from the peer XBee with [HEPTA-SAT-Serial_Monitor](https://hepta-sat-training.github.io/HEPTA-SAT-Serial_Monitor/) (baud **38400**). USB Serial (9600) is monitor-only: it shows that System-Check is running and that commands must come from XBee. Do not send commands over USB.

1. Connect the peer XBee, open HEPTA-SAT-Serial_Monitor, and wait for `System-Check 6`
2. Answer **DATE?**, **KIT?**, and **OPERATOR?** (any text, one field at a time). Press **Send** after each
   - EOL may be **None** (the whole field is sent at once) or **LF** / **CRLF**
3. The three values are echoed so **Save Log** keeps them with the test lines
4. All tests start automatically (same as command `a`). When finished, send `a` or individual commands to re-run

### Commands

| Command | Action |
| --- | --- |
| `a` | Run all tests (also runs automatically after session metadata; XBee link test last). Prints one status line per check, then `N/9 passed` and `failed:` if needed |
| `l` | Blink OBC board LEDs (`HEPTA_OBC_LED1`–`3`) |
| `e` | EPS voltages |
| `i` | Current sense / ammeter (ISOL / IBUS / ICHG). Shine light on the solar panel when prompted |
| `t` | Temperature (pass if 10–35 °C) |
| `m` | IMU |
| `s` | SD read/write |
| `c` | Camera capture (JPEG to SD) |
| `g` | GPS NMEA sentence present (FIX not required) |
| `n` | Optional XBee AT identity diagnostic (not part of `a`) |
| `p` | Interactive XBee link test |

### Session and runner output

On boot the kit prompts for three strings over XBee. Empty input is stored as `-`. After **OPERATOR**, all tests start. Example transcript:

```text
System-Check 6
Operate from this XBee window (USB is monitor-only).

DATE?
KIT?
OPERATOR?

DATE        2026-08-26
KIT         KIT-04
OPERATOR    n_mas

running 9 checks

 OK  LED
 OK  EPS        BUS=4.12  V5=5.01  V3V3=3.30  SAP=0.10
 >>  CURRENT    shine light on solar panel
 OK  CURRENT    ISOL=0.12  IBUS=0.05  ICHG=0.01
 OK  TEMP       24.3 C
 OK  IMU        |a|=9.81  AX=0.12  AY=0.05  AZ=9.80
 OK  SD
 OK  CAM        IMG0001.JPG  12345 B
 NG  GPS        no NMEA in 3s
 >>  XBEE_LINK  reply with any text (30s)
 OK  XBEE_LINK  got 'x'

8/9 passed
failed: GPS

cmd>  a l e i t m s c g n p
```

- ` OK` / ` NG` = result; ` >>` = wait for operator action
- Measured values and failure reasons sit on the right of each status line (use **Save Log** to keep them)
- There are **9** scored items (`LED` … `XBEE_LINK`; `n` / XBee identity is not included)

### Channels

| Path | Role |
| --- | --- |
| XBee (COM) | Commands, session prompts, and the runner transcript |
| USB (CDH) | Banner only: firmware is running; operate from XBee |

### XBee link test (`p`)

1. Send `p`
2. Wait for ` >>  XBEE_LINK  reply with any text (30s)`
3. Send one or more characters from the peer XBee
4. When the kit receives the first non-newline character, you get ` OK  XBEE_LINK  got '…'`

If no reply arrives within 30 seconds, the result is NG. CR and LF characters
alone do not count as a reply. The XBee pair
must already be configured for `AP=0` / Transparent mode at 38400 baud. The
optional `n` diagnostic can inspect its AT identity, but its result does not
affect the normal all-tests result.

### Current sense / ammeter (`i`)

Samples currents for about five seconds (no per-sample lines). Indoors, `IBUS` is usually greater than zero. Confirm that `ISOL` rises when you shine light on the solar panel. If all currents stay stuck at zero, check wiring, the MCP3208, and the shunt path.

### Temperature (`t`)

Pass if the Pt100 reading is between **10 °C and 35 °C** (typical indoor room). Outside that range is NG (open/short, conversion error, or not ambient).

## Empty sketch

To leave an idle firmware after inspection, upload `empty_sketch/empty_sketch.ino`. USB remains available as a COM port.
