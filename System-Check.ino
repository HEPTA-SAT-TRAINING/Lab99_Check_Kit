#include "src/HeptaSat.h"

HeptaCdh cdh;
HeptaCom com;
HeptaEps eps;
HeptaSensor sensor;

static const uint32_t CURRENT_SAMPLE_COUNT = 5;
static const uint32_t CURRENT_SAMPLE_INTERVAL_MS = 1000;
static const float TEMP_MIN_C = 10.0f;
static const float TEMP_MAX_C = 35.0f;
static const uint32_t GPS_SENTENCE_TIMEOUT_MS = 3000;
static const uint32_t XBEE_REPLY_TIMEOUT_MS = 30000;
static const uint32_t XBEE_AT_TIMEOUT_MS = 1000;
static const uint32_t XBEE_LINE_IDLE_MS = 300;
static const size_t SESSION_FIELD_MAX = 64;
static const size_t NAME_WIDTH = 10;
static const int FW_VER = 6;

static uint32_t image_index = 1;

static char session_date[SESSION_FIELD_MAX] = "-";
static char session_kit[SESSION_FIELD_MAX] = "-";
static char session_operator[SESSION_FIELD_MAX] = "-";

void log_com(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  com.vprintf(fmt, args);
  va_end(args);
  com.println("");
}

void log_cdh(const char *msg) {
  cdh.print("[CDH] ");
  cdh.println(msg);
}

void print_cdh_banner(void) {
  log_cdh("System-Check firmware is running (VER=6).");
  log_cdh(
      "Do not send commands here. Operate from the peer XBee "
      "(HEPTA-SAT-Serial_Monitor, 38400).");
}

void drain_usb_input(void) {
  if (Serial.available() <= 0) {
    return;
  }
  while (Serial.available() > 0) {
    (void)Serial.read();
  }
  log_cdh(
      "Do not send commands here. Operate from the peer XBee "
      "(HEPTA-SAT-Serial_Monitor, 38400).");
}

void print_padded_name(const char *name) {
  size_t len = strlen(name);
  com.print(name);
  while (len < NAME_WIDTH) {
    com.print(" ");
    len++;
  }
}

void log_status(bool ok, const char *name, const char *detail) {
  com.print(ok ? " OK  " : " NG  ");
  print_padded_name(name);
  if (detail != NULL && detail[0] != '\0') {
    com.print("  ");
    com.print(detail);
  }
  com.println("");
}

void log_action(const char *name, const char *msg) {
  com.print(" >>  ");
  print_padded_name(name);
  if (msg != NULL && msg[0] != '\0') {
    com.print("  ");
    com.print(msg);
  }
  com.println("");
}

void print_cmd_prompt(void) {
  log_com("cmd>  a l e i t m s c g n p");
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
      while (Serial.available() > 0) {
        (void)Serial.read();
      }
    }
    delay(1);
  }

  while (true) {
    if (Serial.available() > 0) {
      while (Serial.available() > 0) {
        (void)Serial.read();
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
  log_com("DATE?");
  read_session_line(session_date, sizeof(session_date));

  log_com("KIT?");
  read_session_line(session_kit, sizeof(session_kit));

  log_com("OPERATOR?");
  read_session_line(session_operator, sizeof(session_operator));

  log_com("");
  print_padded_name("DATE");
  com.print("  ");
  com.println(session_date);
  print_padded_name("KIT");
  com.print("  ");
  com.println(session_kit);
  print_padded_name("OPERATOR");
  com.print("  ");
  com.println(session_operator);
  log_com("");
}

void make_next_image_filename(char *out, size_t out_size) {
  snprintf(out, out_size, "IMG%04lu.JPG", (unsigned long)image_index);
  image_index++;
  if (image_index > 9999) {
    image_index = 1;
  }
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
  log_action("LED", "confirm OBC LEDs blink");
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
  log_status(true, "LED", NULL);
  return true;
}

bool run_eps_test(void) {
  float bus = eps.get_bus_voltage();
  float v5 = eps.get_5v_voltage();
  float v3 = eps.get_3v3_voltage();
  float sap = eps.get_sap_voltage();
  bool ok = (v3 >= 3.0f && v3 <= 3.6f) && (v5 >= 4.5f && v5 <= 5.5f);
  char detail[96];
  snprintf(
      detail,
      sizeof(detail),
      "BUS=%.2f  V5=%.2f  V3V3=%.2f  SAP=%.2f",
      bus,
      v5,
      v3,
      sap);
  log_status(ok, "EPS", detail);
  return ok;
}

bool run_current_test(void) {
  log_action("CURRENT", "shine light on solar panel");
  float isol_max = 0.0f;
  float ibus_max = 0.0f;
  float ichg_max = 0.0f;

  for (uint32_t sample = 0; sample < CURRENT_SAMPLE_COUNT; sample++) {
    float isol = eps.get_current_solar();
    float ibus = eps.get_current_bus();
    float ichg = eps.get_current_charge();
    if (isol > isol_max) {
      isol_max = isol;
    }
    if (ibus > ibus_max) {
      ibus_max = ibus;
    }
    if (ichg > ichg_max) {
      ichg_max = ichg;
    }
    if (sample + 1 < CURRENT_SAMPLE_COUNT) {
      delay(CURRENT_SAMPLE_INTERVAL_MS);
    }
  }

  bool ok = ibus_max >= 0.01f;
  char detail[96];
  if (ok) {
    snprintf(
        detail,
        sizeof(detail),
        "ISOL=%.2f  IBUS=%.2f  ICHG=%.2f",
        isol_max,
        ibus_max,
        ichg_max);
  } else {
    snprintf(
        detail,
        sizeof(detail),
        "IBUS=%.2f need>=0.01  ISOL=%.2f  ICHG=%.2f",
        ibus_max,
        isol_max,
        ichg_max);
  }
  log_status(ok, "CURRENT", detail);
  return ok;
}

bool run_temperature_test(void) {
  float temp_c = sensor.get_temperature();
  bool ok = (temp_c >= TEMP_MIN_C && temp_c <= TEMP_MAX_C);
  char detail[64];
  if (ok) {
    snprintf(detail, sizeof(detail), "%.1f C", temp_c);
  } else {
    snprintf(
        detail,
        sizeof(detail),
        "%.1f C need %.0f-%.0f",
        temp_c,
        TEMP_MIN_C,
        TEMP_MAX_C);
  }
  log_status(ok, "TEMP", detail);
  return ok;
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
  char detail[80];
  snprintf(
      detail,
      sizeof(detail),
      "|a|=%.2f  AX=%.2f  AY=%.2f  AZ=%.2f",
      norm,
      ax,
      ay,
      az);
  log_status(ok, "IMU", detail);
  return ok;
}

bool run_sd_test(void) {
  const char *filename = "CHECK.TXT";
  const char *payload = "System-Check";
  File file = cdh.create_file(filename);
  if (!file) {
    log_status(false, "SD", "open write failed");
    return false;
  }
  size_t written = cdh.write_file(file, payload);
  file.close();
  if (written == 0) {
    log_status(false, "SD", "write failed");
    return false;
  }

  file = cdh.open_file(filename, FILE_READ);
  if (!file) {
    log_status(false, "SD", "open read failed");
    return false;
  }
  uint8_t buffer[32];
  int n = cdh.read_file(file, buffer, sizeof(buffer) - 1);
  file.close();
  if (n < 0) {
    n = 0;
  }
  buffer[n] = '\0';
  bool ok = (strcmp(reinterpret_cast<char *>(buffer), payload) == 0);
  log_status(ok, "SD", ok ? NULL : "readback mismatch");
  return ok;
}

bool run_camera_test(void) {
  char filename[13];
  make_next_image_filename(filename, sizeof(filename));
  bool ok = sensor.camera_snapshot(filename);
  if (!ok) {
    log_status(false, "CAM", "capture failed");
    sensor.camera_invalidate();
    return false;
  }
  if (!cdh.file_exists(filename)) {
    log_status(false, "CAM", "file missing");
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
  char detail[48];
  if (size_ok) {
    snprintf(detail, sizeof(detail), "%s  %lu B", filename, (unsigned long)size);
  } else {
    snprintf(
        detail,
        sizeof(detail),
        "%s  %lu B need>=1000",
        filename,
        (unsigned long)size);
  }
  log_status(size_ok, "CAM", detail);
  return size_ok;
}

bool run_gps_test(void) {
  uint32_t start_ms = millis();

  while ((uint32_t)(millis() - start_ms) < GPS_SENTENCE_TIMEOUT_MS) {
    if (sensor.gps_is_data_available()) {
      sensor.gps_read_byte();
      log_status(true, "GPS", NULL);
      return true;
    }
    delay(1);
  }

  log_status(false, "GPS", "no NMEA in 3s");
  return false;
}

bool run_xbee_identity(void) {
  char sh[12] = "";
  char sl[12] = "";
  char my[12] = "";
  char id[20] = "";

  if (!com.enter_command_mode()) {
    log_status(false, "XBEE_ID", "AT mode failed");
    return false;
  }

  bool ok = xbee_query("ATSH\r", sh, sizeof(sh))
            && xbee_query("ATSL\r", sl, sizeof(sl))
            && xbee_query("ATMY\r", my, sizeof(my))
            && xbee_query("ATID\r", id, sizeof(id));
  com.exit_command_mode();

  if (!ok) {
    log_status(false, "XBEE_ID", "AT query failed");
    return false;
  }
  char detail[80];
  snprintf(detail, sizeof(detail), "SH=%s  SL=%s  MY=%s  ID=%s", sh, sl, my, id);
  log_status(true, "XBEE_ID", detail);
  return true;
}

bool run_xbee_link_test(void) {
  log_action("XBEE_LINK", "reply with any text (30s)");
  uint32_t start_ms = millis();
  while ((uint32_t)(millis() - start_ms) < XBEE_REPLY_TIMEOUT_MS) {
    if (com.is_cmd_received()) {
      char c = com.get_command();
      if (c != '\0') {
        char detail[24];
        snprintf(detail, sizeof(detail), "got '%c'", c);
        log_status(true, "XBEE_LINK", detail);
        return true;
      }
    }
    delay(1);
  }
  log_status(false, "XBEE_LINK", "timeout");
  return false;
}

bool run_test_all(void) {
  struct Item {
    const char *name;
    bool (*run)(void);
  };
  const Item items[] = {
      {"LED", run_led_test},
      {"EPS", run_eps_test},
      {"CURRENT", run_current_test},
      {"TEMP", run_temperature_test},
      {"IMU", run_imu_test},
      {"SD", run_sd_test},
      {"CAM", run_camera_test},
      {"GPS", run_gps_test},
      // Bidirectional XBee payload test last so earlier steps are not blocked.
      // AT identity is diagnostic-only and does not affect the normal pass/fail.
      {"XBEE_LINK", run_xbee_link_test},
  };
  const size_t total = sizeof(items) / sizeof(items[0]);
  bool results[sizeof(items) / sizeof(items[0])];
  size_t passed = 0;

  log_com("running %u checks", (unsigned)total);
  log_com("");

  for (size_t i = 0; i < total; i++) {
    results[i] = items[i].run();
    if (results[i]) {
      passed++;
    }
  }

  log_com("");
  log_com("%u/%u passed", (unsigned)passed, (unsigned)total);

  if (passed < total) {
    com.print("failed:");
    for (size_t i = 0; i < total; i++) {
      if (!results[i]) {
        com.print(" ");
        com.print(items[i].name);
      }
    }
    com.println("");
  }

  return passed == total;
}

void handle_command(char cmd) {
  switch (cmd) {
    case 'a':
      run_test_all();
      break;
    case 'l':
      run_led_test();
      break;
    case 'e':
      run_eps_test();
      break;
    case 'i':
      run_current_test();
      break;
    case 't':
      run_temperature_test();
      break;
    case 'm':
      run_imu_test();
      break;
    case 's':
      run_sd_test();
      break;
    case 'c':
      run_camera_test();
      break;
    case 'g':
      run_gps_test();
      break;
    case 'n':
      run_xbee_identity();
      break;
    case 'p':
      run_xbee_link_test();
      break;
    default:
      log_com("unknown: '%c'", cmd);
      break;
  }
  print_cmd_prompt();
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

void setup(void) {
  cdh.begin();

  eps.init();
  eps.switch_3V3_on();
  delay(800);

  com.begin();
  // The inspection procedure preconfigures the XBee for AP=0 / Transparent
  // mode. Avoid entering AT Command mode during the normal system check.

  sensor.begin();

  print_cdh_banner();

  log_com("System-Check %d", FW_VER);
  log_com("Operate from this XBee window (USB is monitor-only).");
  log_com("");
  prompt_session_info();
  run_test_all();
  log_com("");
  print_cmd_prompt();
}

void loop(void) {
  drain_usb_input();
  poll_xbee_commands();
  delay(1);
}
