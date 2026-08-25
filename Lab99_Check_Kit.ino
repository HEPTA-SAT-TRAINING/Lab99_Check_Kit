#include "src/HeptaSat.h"

#include <Arduino.h>
#include <SD.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

HeptaCdh cdh;
HeptaCom com;
HeptaEps eps;
HeptaSensor sensor;

static const uint32_t CURRENT_SAMPLE_COUNT = 5;
static const uint32_t CURRENT_SAMPLE_INTERVAL_MS = 1000;
static const uint32_t GPS_SENTENCE_TIMEOUT_MS = 3000;
static const uint32_t XBEE_RX_TIMEOUT_MS = 30000;
static const uint32_t XBEE_AT_TIMEOUT_MS = 1000;
static const uint32_t XBEE_LINE_IDLE_MS = 300;
static const size_t SERIAL_LINE_MAX = 80;
static const size_t SESSION_FIELD_MAX = 64;
static const size_t LOG_MSG_MAX = 160;

static uint32_t image_index = 1;
static char serial_line[SERIAL_LINE_MAX];
static size_t serial_line_len = 0;
static bool gps_uart_live = false;

static char session_date[SESSION_FIELD_MAX] = "-";
static char session_kit[SESSION_FIELD_MAX] = "-";
static char session_operator[SESSION_FIELD_MAX] = "-";

void log_progress(const char *fmt, ...) {
  char body[LOG_MSG_MAX];
  va_list args;
  va_start(args, fmt);
  vsnprintf(body, sizeof(body), fmt, args);
  va_end(args);

  cdh.printf("[CDH] %s\n", body);
  com.printf("From Sat: %s\n", body);
}

void sanitize_session_field(char *s, size_t size) {
  if (s == NULL || size == 0) {
    return;
  }

  size_t start = 0;
  while (s[start] != '\0' && isspace((unsigned char)s[start])) {
    start++;
  }
  if (start > 0) {
    memmove(s, s + start, strlen(s + start) + 1);
  }

  size_t n = strlen(s);
  while (n > 0 && isspace((unsigned char)s[n - 1])) {
    s[--n] = '\0';
  }

  for (size_t i = 0; s[i] != '\0'; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x20 || c == '=' || c == ',') {
      s[i] = '_';
    }
  }

  if (s[0] == '\0') {
    strncpy(s, "-", size);
    s[size - 1] = '\0';
  }
}

bool read_session_line(char *out, size_t out_size) {
  if (out == NULL || out_size == 0) {
    return false;
  }

  size_t n = 0;
  bool xbee_started = false;
  uint32_t last_xbee_ms = 0;
  out[0] = '\0';

  uint32_t drain_ms = millis();
  while ((uint32_t)(millis() - drain_ms) < 50) {
    if (com.available()) {
      char c = com.get_char();
      if (c == '\r' || c == '\n') {
        continue;
      }
      if (n + 1 < out_size) {
        out[n++] = c;
        out[n] = '\0';
      }
      xbee_started = true;
      last_xbee_ms = millis();
      break;
    }
    if (Serial.available() > 0) {
      break;
    }
    delay(1);
  }

  while (true) {
    while (Serial.available() > 0) {
      char c = (char)Serial.read();
      if (c == '\r') {
        continue;
      }
      if (c == '\n') {
        sanitize_session_field(out, out_size);
        return true;
      }
      if (n + 1 < out_size) {
        out[n++] = c;
        out[n] = '\0';
      }
    }

    if (com.available()) {
      char c = com.get_char();
      if (c == '\r' || c == '\n') {
        sanitize_session_field(out, out_size);
        return true;
      }
      if (n + 1 < out_size) {
        out[n++] = c;
        out[n] = '\0';
      }
      xbee_started = true;
      last_xbee_ms = millis();
      continue;
    }

    if (xbee_started && (uint32_t)(millis() - last_xbee_ms) >= XBEE_LINE_IDLE_MS) {
      sanitize_session_field(out, out_size);
      return true;
    }

    delay(1);
  }
}

void prompt_session_info(void) {
  log_progress("SESSION: enter DATE (any text), then Send in HEPTA-SAT-Serial_Monitor");
  read_session_line(session_date, sizeof(session_date));
  log_progress("SESSION: DATE=%s", session_date);

  log_progress("SESSION: enter KIT name (any text), then Send");
  read_session_line(session_kit, sizeof(session_kit));
  log_progress("SESSION: KIT=%s", session_kit);

  log_progress("SESSION: enter OPERATOR (any text), then Send");
  read_session_line(session_operator, sizeof(session_operator));
  log_progress("SESSION: OPERATOR=%s", session_operator);

  log_progress(
      "SESSION DATE=%s KIT=%s OPERATOR=%s",
      session_date,
      session_kit,
      session_operator);
}

void make_next_image_filename(char *out, size_t out_size) {
  snprintf(out, out_size, "IMG%04lu.JPG", (unsigned long)image_index);
  image_index++;
  if (image_index > 9999) {
    image_index = 1;
  }
}

void xbee_pause_gps_pio(void) {
  if (gps_uart_live) {
    sensor.gps_end();
    gps_uart_live = false;
  }
}

void xbee_resume_gps_pio(void) {
  if (!gps_uart_live) {
    sensor.gps_begin();
    gps_uart_live = true;
  }
}

bool xbee_enter_command_mode(void) {
  xbee_pause_gps_pio();
  if (!com.enter_command_mode()) {
    xbee_resume_gps_pio();
    return false;
  }
  return true;
}

bool xbee_query(const char *command, char *value, size_t value_size) {
  char response[48];
  if (!com.send_at_command(command, response, sizeof(response), XBEE_AT_TIMEOUT_MS)) {
    return false;
  }
  size_t n = 0;
  while (response[n] != '\0' && response[n] != '\r' && response[n] != '\n' && n + 1 < value_size) {
    value[n] = response[n];
    n++;
  }
  value[n] = '\0';
  return value[0] != '\0';
}

bool run_led_test(void) {
  log_progress("LED: blinking OBC LEDs — confirm visually");
  for (size_t i = 0; i < HEPTA_OBC_LED_COUNT; i++) {
    pinMode(HEPTA_OBC_LEDS[i], OUTPUT);
    digitalWrite(HEPTA_OBC_LEDS[i], LOW);
  }
  for (int cycle = 0; cycle < 3; cycle++) {
    for (size_t i = 0; i < HEPTA_OBC_LED_COUNT; i++) {
      digitalWrite(HEPTA_OBC_LEDS[i], HIGH);
    }
    delay(300);
    for (size_t i = 0; i < HEPTA_OBC_LED_COUNT; i++) {
      digitalWrite(HEPTA_OBC_LEDS[i], LOW);
    }
    delay(300);
  }
  log_progress("LED: OK");
  return true;
}

bool run_eps_test(void) {
  float bus = eps.get_bus_voltage();
  float v5 = eps.get_5v_voltage();
  float v3 = eps.get_3v3_voltage();
  float sap = eps.get_sap_voltage();
  log_progress("EPS: BUS=%.3f V5=%.3f V3V3=%.3f SAP=%.3f", bus, v5, v3, sap);
  bool ok = (v3 >= 3.0f && v3 <= 3.6f) && (v5 >= 4.5f && v5 <= 5.5f);
  log_progress("EPS: %s", ok ? "OK" : "NG");
  return ok;
}

bool run_current_test(void) {
  log_progress("CURRENT: shine light on the solar panel and watch ISOL change");
  float isol = 0.0f;
  float ibus = 0.0f;
  float ichg = 0.0f;
  float isol_max = 0.0f;
  float ibus_max = 0.0f;
  float ichg_max = 0.0f;

  for (uint32_t sample = 0; sample < CURRENT_SAMPLE_COUNT; sample++) {
    isol = eps.get_current_solar();
    ibus = eps.get_current_bus();
    ichg = eps.get_current_charge();
    if (isol > isol_max) {
      isol_max = isol;
    }
    if (ibus > ibus_max) {
      ibus_max = ibus;
    }
    if (ichg > ichg_max) {
      ichg_max = ichg;
    }
    log_progress(
        "CURRENT sample %lu: ISOL=%.3f IBUS=%.3f ICHG=%.3f",
        (unsigned long)(sample + 1),
        isol,
        ibus,
        ichg);
    if (sample + 1 < CURRENT_SAMPLE_COUNT) {
      delay(CURRENT_SAMPLE_INTERVAL_MS);
    }
  }

  bool ok = ibus_max >= 0.01f;
  if (!ok) {
    log_progress("CURRENT: NG (IBUS too low — check shunt / MCP3208 / GP28)");
  } else {
    log_progress(
        "CURRENT: OK (ISOL_MAX=%.3f IBUS_MAX=%.3f ICHG_MAX=%.3f)",
        isol_max,
        ibus_max,
        ichg_max);
  }
  return ok;
}

bool run_temperature_test(void) {
  float temp_c = sensor.get_temperature();
  log_progress("TEMP: %.2f C — OK", temp_c);
  return true;
}

bool run_imu_test(void) {
  float ax = 0.0f, ay = 0.0f, az = 0.0f;
  float gx = 0.0f, gy = 0.0f, gz = 0.0f;
  float mx = 0.0f, my = 0.0f, mz = 0.0f;

  bool acc_ok = sensor.get_acceleration(&ax, &ay, &az);
  float norm = sqrtf(ax * ax + ay * ay + az * az);
  if (!acc_ok || norm < 0.5f) {
    delay(200);
    sensor.begin();
    delay(200);
    acc_ok = sensor.get_acceleration(&ax, &ay, &az);
    sensor.get_gyro(&gx, &gy, &gz);
    sensor.get_magnetometer(&mx, &my, &mz);
    norm = sqrtf(ax * ax + ay * ay + az * az);
  } else {
    sensor.get_gyro(&gx, &gy, &gz);
    sensor.get_magnetometer(&mx, &my, &mz);
  }

  bool ok = acc_ok && norm >= 0.5f;
  log_progress(
      "IMU: |a|=%.2f AX=%.3f AY=%.3f AZ=%.3f — %s",
      norm,
      ax,
      ay,
      az,
      ok ? "OK" : "NG");
  return ok;
}

bool run_sd_test(void) {
  const char *filename = "CHECK.TXT";
  const char *payload = "Lab99_Check_Kit";
  File file = cdh.create_file(filename);
  if (!file) {
    log_progress("SD: NG (open write failed)");
    return false;
  }
  size_t written = file.print(payload);
  file.close();
  if (written == 0) {
    log_progress("SD: NG (write failed)");
    return false;
  }

  file = cdh.open_file(filename, FILE_READ);
  if (!file) {
    log_progress("SD: NG (open read failed)");
    return false;
  }
  char buffer[32];
  int n = file.read(buffer, sizeof(buffer) - 1);
  file.close();
  if (n < 0) {
    n = 0;
  }
  buffer[n] = '\0';
  bool ok = (strcmp(buffer, payload) == 0);
  log_progress("SD: %s (wrote/read CHECK.TXT)", ok ? "OK" : "NG");
  return ok;
}

bool run_camera_test(void) {
  char filename[13];
  make_next_image_filename(filename, sizeof(filename));
  log_progress("CAM: capturing %s", filename);
  bool ok = sensor.camera_snapshot(filename);
  if (!ok) {
    log_progress("CAM: NG (capture failed)");
    sensor.camera_invalidate();
    return false;
  }
  if (!cdh.file_exists(filename)) {
    log_progress("CAM: NG (file missing)");
    sensor.camera_invalidate();
    return false;
  }
  File file = cdh.open_file(filename, FILE_READ);
  uint32_t size = file ? file.size() : 0;
  if (file) {
    file.close();
  }
  sensor.camera_invalidate();
  bool size_ok = size >= 1000;
  log_progress("CAM: %s FILE=%s SIZE=%lu", size_ok ? "OK" : "NG", filename, (unsigned long)size);
  return size_ok;
}

bool is_nmea_talker(const char *sentence) {
  if (sentence == NULL || sentence[0] != '$' || sentence[1] != 'G') {
    return false;
  }
  char third = sentence[2];
  return third == 'P' || third == 'N' || third == 'L';
}

bool run_gps_test(void) {
  char sentence[88];
  size_t slen = 0;
  bool in_sentence = false;
  bool found = false;
  uint32_t start_ms = millis();

  while ((uint32_t)(millis() - start_ms) < GPS_SENTENCE_TIMEOUT_MS) {
    int value = sensor.gps_read_byte();
    if (value < 0) {
      delay(1);
      continue;
    }
    char c = (char)value;
    if (c == '$') {
      in_sentence = true;
      slen = 0;
      sentence[slen++] = c;
      continue;
    }
    if (!in_sentence) {
      continue;
    }
    if (c == '\r') {
      continue;
    }
    if (c == '\n' || slen >= sizeof(sentence) - 1) {
      sentence[slen] = '\0';
      in_sentence = false;
      if (is_nmea_talker(sentence)) {
        found = true;
        break;
      }
      slen = 0;
      continue;
    }
    sentence[slen++] = c;
  }

  if (found) {
    log_progress("GPS: OK (NMEA received)");
  } else {
    log_progress("GPS: NG (no NMEA within timeout)");
  }
  return found;
}

bool run_xbee_identity(void) {
  char sh[12] = "";
  char sl[12] = "";
  char my[12] = "";
  char id[20] = "";

  if (!xbee_enter_command_mode()) {
    log_progress("XBEE_ID: NG (AT mode failed)");
    xbee_resume_gps_pio();
    return false;
  }

  bool ok = xbee_query("ATSH\r", sh, sizeof(sh))
            && xbee_query("ATSL\r", sl, sizeof(sl))
            && xbee_query("ATMY\r", my, sizeof(my))
            && xbee_query("ATID\r", id, sizeof(id));
  com.exit_command_mode();
  xbee_resume_gps_pio();

  if (!ok) {
    log_progress("XBEE_ID: NG (AT query failed)");
    return false;
  }
  log_progress("XBEE_ID: OK SH=%s SL=%s MY=%s ID=%s", sh, sl, my, id);
  return true;
}

bool run_xbee_rx_test(void) {
  // Leave AT mode if a previous identity check left the radio there.
  if (com.enter_command_mode()) {
    com.exit_command_mode();
  }
  delay(1200);

  log_progress("XBEE_RX: send a command from the PC via XBee (30s)");
  uint32_t start_ms = millis();
  while ((uint32_t)(millis() - start_ms) < XBEE_RX_TIMEOUT_MS) {
    if (com.is_cmd_received()) {
      char cmd = com.get_command();
      log_progress("XBEE_RX: OK (received '%c')", cmd);
      return true;
    }
    delay(1);
  }
  log_progress("XBEE_RX: NG (timeout — no command from PC)");
  return false;
}

bool run_test_all(void) {
  log_progress(
      "SESSION DATE=%s KIT=%s OPERATOR=%s",
      session_date,
      session_kit,
      session_operator);
  bool ok = true;
  ok = run_led_test() && ok;
  ok = run_eps_test() && ok;
  ok = run_current_test() && ok;
  ok = run_temperature_test() && ok;
  ok = run_imu_test() && ok;
  ok = run_sd_test() && ok;
  ok = run_camera_test() && ok;
  ok = run_gps_test() && ok;
  ok = run_xbee_identity() && ok;
  // Interactive RX last so earlier steps are not blocked.
  ok = run_xbee_rx_test() && ok;
  log_progress("TEST_ALL: %s", ok ? "OK" : "NG");
  return ok;
}

void handle_command(char cmd) {
  log_progress("COMMAND: '%c' accepted", cmd);
  bool ok = true;

  switch (cmd) {
    case 'a':
      ok = run_test_all();
      break;
    case 'l':
      ok = run_led_test();
      break;
    case 'e':
      ok = run_eps_test();
      break;
    case 'i':
      ok = run_current_test();
      break;
    case 't':
      ok = run_temperature_test();
      break;
    case 'm':
      ok = run_imu_test();
      break;
    case 's':
      ok = run_sd_test();
      break;
    case 'c':
      ok = run_camera_test();
      break;
    case 'g':
      ok = run_gps_test();
      break;
    case 'n':
      ok = run_xbee_identity();
      break;
    case 'p':
      ok = run_xbee_rx_test();
      break;
    default:
      ok = false;
      log_progress("COMMAND: unknown '%c'", cmd);
      break;
  }

  log_progress("COMMAND_DONE: '%c' %s", cmd, ok ? "OK" : "NG");
}

void poll_xbee_commands(void) {
  if (!com.is_cmd_received()) {
    return;
  }
  char cmd = com.get_command();
  if (cmd != '\0') {
    handle_command(cmd);
  }
}

void poll_serial_commands(void) {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      serial_line[serial_line_len] = '\0';
      if (serial_line_len == 1) {
        handle_command(serial_line[0]);
      } else if (serial_line_len > 1) {
        log_progress("COMMAND: unknown line");
      }
      serial_line_len = 0;
      continue;
    }
    if (serial_line_len + 1 < sizeof(serial_line)) {
      serial_line[serial_line_len++] = c;
    }
  }
}

void setup(void) {
  cdh.begin();
  cdh.wait_for_serial();

  eps.init();
  eps.switch_3V3_on();
  delay(800);

  com.begin();
  if (xbee_enter_command_mode()) {
    com.exit_command_mode();
  }

  sensor.begin();
  gps_uart_live = true;

  log_progress("BOOT READY FW=Lab99_Check_Kit VER=2");
  prompt_session_info();
  log_progress("Send command a/l/e/i/t/m/s/c/g/n/p then Enter/Send");
}

void loop(void) {
  poll_serial_commands();
  poll_xbee_commands();
  delay(1);
}
