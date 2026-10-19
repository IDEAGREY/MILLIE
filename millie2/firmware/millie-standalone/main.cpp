#include "core.h"
#include "touch_math.h"
#include "ui.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
using namespace millie;
static const char *FW = "civops-2.0.4";
TFT_eSPI tft;
TFT_eSprite frame(&tft);
bool buffered = false;
uint8_t ink(uint16_t col) {
  return col == GREEN   ? 1
         : col == DIM   ? 2
         : col == AMBER ? 3
         : col == RED   ? 4
         : col == GREY  ? 5
         : col == CYAN  ? 6
         : col == PINK  ? 7
                        : 0;
}
SPIClass touchBus(VSPI);       // Dedicated CYD touch bus; TFT uses HSPI.
XPT2046_Touchscreen touch(33, 36); // CYD XPT2046: CS33, IRQ36, SCK25, MISO39, MOSI32.
Preferences prefs;
Model model;
View view;
Calibration calibration;
static const Point targets[] = {{20, 112}, {220, 112}, {20, 260}, {120, 188}};
Point rawTargets[3];
bool prefsReady = false, uiDirty = true, touchReady = false;
uint32_t wifiHits = 0, bleHits = 0, txLines = 0, lastAppRx = 0, sliceStart = 0, lastHop = 0,
         lastBeat = 0, lastDraw = 0, lastSave = 0, lastTouch = 0, touchReleases = 0;
uint32_t wifiWindowMs = 6000, bleWindowMs = 3000;
volatile uint32_t queueDrops = 0;
volatile uint32_t controlCounts[16]{};
volatile int8_t controlRssi[16]{};
volatile uint8_t controlChannels[16]{};
uint32_t lastControlReport = 0;
uint8_t channelLock = 0;
bool hopEnabled = true, radioWifi = true;
uint8_t forceMode = 0;
NimBLEScan *bleScan = nullptr;
QueueHandle_t observations = nullptr;
struct Dedup {
  uint8_t mac[6];
  uint32_t last, window;
  bool used;
};
Dedup wifiDedup[64]{};
char input[192];
size_t inputPos = 0;
bool inputOverflow = false;
uint32_t rawSumX = 0, rawSumY = 0, pressStart = 0;
uint16_t rawSamples = 0;
bool wakeOnly = false;
volatile bool touchDown = false;
volatile uint32_t touchQueueDrops = 0;
struct RawPress {
  uint32_t sumX, sumY, duration;
  uint16_t samples;
  Point first, last;
};
QueueHandle_t touchPresses = nullptr;
Point latestRaw{}, firstRaw{}, lastRaw{};
uint32_t touchFailures = 0;
bool touchIrqEvent = false, touchIrqPinLow = false, touchPressed = false;
int touchPressure = 0;

struct Canvas {
  void fill(int x, int y, int w, int h, uint16_t col) {
    if (buffered)
      frame.fillRect(x, y, w, h, ink(col));
    else
      tft.fillRect(x, y, w, h, col);
  }
  void rect(int x, int y, int w, int h, uint16_t col) {
    if (buffered)
      frame.drawRect(x, y, w, h, ink(col));
    else
      tft.drawRect(x, y, w, h, col);
  }
  void line(int x, int y, int x2, int y2, uint16_t col) {
    if (buffered)
      frame.drawLine(x, y, x2, y2, ink(col));
    else
      tft.drawLine(x, y, x2, y2, col);
  }
  void circle(int x, int y, int r, uint16_t col) {
    if (buffered)
      frame.drawCircle(x, y, r, ink(col));
    else
      tft.drawCircle(x, y, r, col);
  }
  void text(int x, int y, const char *s, uint16_t col) {
    if (buffered) {
      frame.setTextColor(ink(col), 0);
      frame.setTextSize(1);
      frame.setCursor(x, y);
      frame.print(s);
    } else {
      tft.setTextColor(col, BG);
      tft.setTextSize(1);
      tft.setCursor(x, y);
      tft.print(s);
    }
  }
} canvas;
void repaint() {
  drawUI(canvas, model, view);
  if (buffered)
    frame.pushSprite(0, 0);
}
void notice(const char *s) {
  view.notice = s;
  view.noticeUntil = millis() + 2500;
  uiDirty = true;
}
void brightness() { ledcWrite(0, model.saved.brightness * 255 / 100); }
void saveState() {
  if (!prefsReady) {
    view.storeOk = false;
    return;
  }
  model.saved.bootCounter = model.boot;
  view.storeOk = prefs.putBytes("state", &model.saved, sizeof(model.saved)) == sizeof(model.saved);
  if (view.storeOk)
    model.dirty = false;
  lastSave = millis();
}
void beginCalibration() {
  view.pocket = false;
  view.calStep = 0;
  rawSamples = 0;
  uiDirty = true;
  lastTouch = millis();
  brightness();
}
void setMode(uint8_t mode);
void startWifi() {
  radioWifi = true;
  esp_wifi_set_promiscuous_rx_cb([](void *buf, wifi_promiscuous_pkt_type_t typ) {
    if (typ == WIFI_PKT_MISC || !observations)
      return;
    const wifi_promiscuous_pkt_t *p = (const wifi_promiscuous_pkt_t *)buf;
    size_t len = p->rx_ctrl.sig_len;
    const uint8_t *f = p->payload;
    if (typ == WIFI_PKT_CTRL) {
      if (len < 10) return;
      uint8_t sub = (f[0] >> 4) & 15;
      controlRssi[sub] = p->rx_ctrl.rssi;
      controlChannels[sub] = p->rx_ctrl.channel;
      __atomic_add_fetch(&controlCounts[sub], 1, __ATOMIC_RELAXED);
      return;
    }
    if (len < 24)
      return;
    uint8_t ft = (f[0] >> 2) & 3, sub = (f[0] >> 4) & 15;
    if (ft != 0 && ft != 2)
      return;
    const uint8_t *mac = f + 10;
    if (mac[0] & 1)
      return;
    uint32_t now = millis();
    int slot = -1, oldest = 0;
    uint32_t age = 0;
    for (int i = 0; i < 64; i++) {
      if (wifiDedup[i].used && !memcmp(wifiDedup[i].mac, mac, 6)) {
        slot = i;
        break;
      }
      if (!wifiDedup[i].used) {
        oldest = i;
        age = 0xffffffff;
      } else if (age != 0xffffffff && now - wifiDedup[i].last > age) {
        oldest = i;
        age = now - wifiDedup[i].last;
      }
    }
    if (slot < 0) {
      slot = oldest;
      wifiDedup[slot] = Dedup{};
      memcpy(wifiDedup[slot].mac, mac, 6);
      wifiDedup[slot].used = true;
      wifiDedup[slot].last = now - 4000;
      wifiDedup[slot].window = now;
    }
    Dedup &d = wifiDedup[slot];
    if (now - d.last < 4000)
      return;
    d.last = now;
    Observation o{};
    macText(mac, o.mac);
    o.kind = 'W';
    o.rssi = p->rx_ctrl.rssi;
    o.channel = p->rx_ctrl.channel;
    const char *frame = ft == 2    ? "data"
                        : sub == 4 ? "probe_req"
                        : sub == 5 ? "probe_resp"
                        : sub == 8 ? "beacon"
                                   : "mgmt";
    copyText(o.frame, frame, sizeof(o.frame));
    if (ft == 0)
      ssidFromFrame(f, len, sub, o.name, sizeof(o.name));
    o.sig = GENERIC;
    if (xQueueSend(observations, &o, 0) != pdTRUE)
      __atomic_add_fetch(&queueDrops, 1, __ATOMIC_RELAXED);
  });
  wifi_promiscuous_filter_t filter{};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT | WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_CTRL;
  esp_wifi_set_promiscuous_filter(&filter);
  wifi_promiscuous_filter_t controlFilter{};
  controlFilter.filter_mask = WIFI_PROMIS_CTRL_FILTER_MASK_ALL;
  esp_wifi_set_promiscuous_ctrl_filter(&controlFilter);
  esp_wifi_set_channel(channelLock ? channelLock : view.channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(true);
}
void stopWifi() { esp_wifi_set_promiscuous(false); }
class BleCallbacks : public NimBLEScanCallbacks {
  void onResult(const NimBLEAdvertisedDevice *d) override {
    Observation o{};
    std::string mac = d->getAddress().toString(), name = d->getName();
    copyText(o.mac, mac.c_str(), 18);
    for (char &c : o.mac)
      c = toupper((unsigned char)c);
    copyText(o.name, name.c_str(), 33);
    o.kind = 'B';
    o.rssi = d->getRSSI();
    std::string md = d->getManufacturerData();
    size_t mlen = md.size() < sizeof(o.manufacturer) / 2 - 1 ? md.size() : sizeof(o.manufacturer) / 2 - 1;
    for (size_t i = 0; i < mlen; ++i)
      snprintf(o.manufacturer + i * 2, sizeof(o.manufacturer) - i * 2, "%02X", (unsigned char)md[i]);
    size_t used = 0;
    for (uint8_t i = 0; i < d->getServiceUUIDCount() && used + 2 < sizeof(o.services); ++i) {
      std::string uuid = d->getServiceUUID(i).toString();
      if (i) o.services[used++] = ',';
      size_t n = uuid.size() < sizeof(o.services) - used - 1 ? uuid.size() : sizeof(o.services) - used - 1;
      memcpy(o.services + used, uuid.data(), n);
      used += n;
      o.services[used] = 0;
    }
    o.sig = GENERIC;
    if (xQueueSend(observations, &o, 0) != pdTRUE)
      __atomic_add_fetch(&queueDrops, 1, __ATOMIC_RELAXED);
  }
} bleCallbacks;
void startBle() {
  radioWifi = false;
  if (!bleScan) {
    NimBLEDevice::init("");
    bleScan = NimBLEDevice::getScan();
    bleScan->setScanCallbacks(&bleCallbacks, false);
    bleScan->setMaxResults(0);
    bleScan->setActiveScan(false);
    bleScan->setInterval(80);
    bleScan->setWindow(45);
  }
  bleScan->start(0, false, true);
}
void stopBle() {
  if (bleScan)
    bleScan->stop();
}
void setMode(uint8_t mode) {
  forceMode = mode;
  view.mode = mode;
  sliceStart = millis();
  if (mode == 1 && !radioWifi) {
    stopBle();
    startWifi();
  }
  if (mode == 2 && radioWifi) {
    stopWifi();
    startBle();
  }
  uiDirty = true;
}
void exportLog() {
  for (int i = model.saved.count - 1; i >= 0; i--) {
    const LogEntry *e = model.logAt(i);
    char name[60];
    jsonEscape(e->name, name, sizeof(name));
    Serial.printf("{\"_millie\":\"log\",\"record\":{\"boot\":%lu,\"uptime\":%lu,\"mac\":\"%s\","
                  "\"name\":\"%s\",\"kind\":\"%c\",\"match\":\"%s\",\"rssi\":%d}}\n",
                  (unsigned long)e->boot, (unsigned long)e->seconds, e->mac, name, e->kind,
                  tag(e->sig), e->rssi);
  }
}
void openDevice(const Device &d, Page back) {
  copyText(view.selected, d.mac, 18);
  view.selectedKind = d.kind;
  view.historical = d;
  view.back = back;
  view.page = DETAIL;
  uiDirty = true;
}
void handleTap(int x, int y) {
  lastTouch = millis();
  if (view.pocket) {
    view.pocket = false;
    brightness();
    return;
  }
  int nav = navAt(x, y);
  if (nav >= 0) {
    view.page = (Page)nav;
    view.offset = 0;
    uiDirty = true;
    return;
  }
  if (x < 0 || x >= 240 || y < 33 || y >= 288)
    return;
  if (view.page == DEVICES || view.page == WATCH) {
    int order[DEVICE_CAP], n = model.list(order, view.page == WATCH);
    if (y >= 254) {
      if (x < 65)
        view.offset = view.offset >= LIST_ROWS ? view.offset - LIST_ROWS : 0;
      else if (x >= 177 && view.offset + LIST_ROWS < n)
        view.offset += LIST_ROWS;
    } else {
      int row = (y - LIST_TOP) / LIST_ROW;
      if (y >= LIST_TOP && row >= 0 && row < LIST_ROWS && view.offset + row < n)
        openDevice(model.devices[order[view.offset + row]], view.page);
    }
  } else if (view.page == DETAIL) {
    if (y < 66 && x < 75) {
      view.page = view.back;
    } else if (y >= 254) {
      int idx = model.find(view.selected, view.selectedKind);
      Device &d = idx >= 0 ? model.devices[idx] : view.historical;
      if (!model.toggleRule(d, x < 120))
        notice("RULE LIST FULL (16)");
      else {
        saveState();
        notice(x < 120 ? "WATCH RULE SAVED" : "ALERT RULE SAVED");
      }
    }
  } else if (view.page == LOG) {
    if (y >= 254) {
      if (x < 65)
        view.offset = view.offset >= LIST_ROWS ? view.offset - LIST_ROWS : 0;
      else if (x >= 177 && view.offset + LIST_ROWS < model.saved.count)
        view.offset += LIST_ROWS;
    } else if (y >= LIST_TOP) {
      int row = (y - LIST_TOP) / LIST_ROW;
      const LogEntry *e = row < LIST_ROWS ? model.logAt(view.offset + row) : nullptr;
      if (e) {
        int idx = model.find(e->mac, e->kind);
        if (idx >= 0)
          openDevice(model.devices[idx], LOG);
        else {
          Device d{};
          copyText(d.mac, e->mac, 18);
          copyText(d.name, e->name, 33);
          d.kind = e->kind;
          d.rssi = e->rssi;
          d.sig = e->sig;
          d.unobserved = true;
          d.last = e->boot == model.boot ? e->seconds * 1000 : 0;
          openDevice(d, LOG);
        }
      }
    }
  } else if (view.page == SETUP && y >= SET_TOP && y < SET_TOP + 8 * SET_ROW) {
    int row = (y - SET_TOP) / SET_ROW;
    switch (row) {
    case 0:
      setMode((forceMode + 1) % 3);
      break;
    case 1:
      channelLock = (channelLock + 1) % 14;
      if (channelLock) {
        view.channel = channelLock;
        if (radioWifi)
          esp_wifi_set_channel(channelLock, WIFI_SECOND_CHAN_NONE);
      }
      break;
    case 2:
      model.saved.filter = !model.saved.filter;
      model.dirty = true;
      saveState();
      break;
    case 3:
      model.saved.alerts = !model.saved.alerts;
      model.dirty = true;
      saveState();
      break;
    case 4:
      model.saved.brightness = model.saved.brightness >= 100 ? 20 : model.saved.brightness + 20;
      model.dirty = true;
      brightness();
      saveState();
      break;
    case 5:
      beginCalibration();
      break;
    case 6:
      exportLog();
      notice("LOG SENT OVER USB");
      break;
    case 7:
      saveState();
      notice(view.storeOk ? "SAVED" : "STORAGE FULL / NOT SAVED");
      break;
    }
  } else if (view.page == SCAN && y >= 254 && model.alertIndex >= 0 &&
             model.now - model.alertAt < 8000)
    openDevice(model.devices[model.alertIndex], SCAN);
  uiDirty = true;
}
void finishPress() {
  if (rawSamples < (view.calStep >= 0 ? 3 : 2) ||
      millis() - pressStart < (view.calStep >= 0 ? 50 : 20))
    return;
  Point raw = {(float)rawSumX / rawSamples, (float)rawSumY / rawSamples};
  latestRaw = raw;
  if (wakeOnly)
    return;
  if (view.calStep >= 0) {
    int step = view.calStep;
    if (step < 3) {
      rawTargets[step] = raw;
      view.calStep++;
      if (view.calStep == 3 && !solveCalibration(rawTargets, targets, calibration)) {
        view.calStep = 0;
        touchFailures++;
        notice("RETRY CALIBRATION");
      }
    } else {
      Point mapped = project(calibration, raw);
      float error = hypotf(mapped.x - targets[3].x, mapped.y - targets[3].y);
      if (error > 10 || !calibrationValid(calibration)) {
        view.calStep = 0;
        touchFailures++;
        notice("CALIBRATION FAILED");
      } else {
        view.calStep = -1;
        bool stored = prefsReady && prefs.putBytes("touch", &calibration, sizeof(calibration)) ==
                                        sizeof(calibration);
        view.storeOk = stored;
        Serial.printf("{\"_millie\":\"touch_calibrated\",\"error_px\":%.1f,\"saved\":%s}\n", error,
                      stored ? "true" : "false");
        notice(stored ? "CALIBRATED" : "CALIBRATED / NOT SAVED");
      }
    }
    uiDirty = true;
    return;
  }
  if (!calibrationValid(calibration)) {
    beginCalibration();
    return;
  }
  Point p = project(calibration, raw);
  Point p0 = project(calibration, firstRaw), p1 = project(calibration, lastRaw);
  if (hypotf(p0.x - p1.x, p0.y - p1.y) > 24)
    return;
  if (p.x < -8 || p.y < -8 || p.x > 248 || p.y > 328) {
    touchFailures++;
    return;
  }
  touchReleases++;
  handleTap((int)fmaxf(0, fminf(239, p.x)), (int)fmaxf(0, fminf(319, p.y)));
}
bool sampleTouch(TS_Point &p) {
  touchIrqEvent = touch.tirqTouched();
  touchIrqPinLow = digitalRead(36) == LOW;
  p = touch.getPoint();
  touchPressure = p.z;
  touchPressed = p.z >= 400;
  if (touchPressed)
    latestRaw = {(float)p.x, (float)p.y};
  return p.z >= 400;
}
void pollTouch() {
  static RawPress capture{};
  static uint32_t started = 0, lastSample = 0;
  static uint8_t released = 0;
  static bool active = false;
  uint32_t now = millis();
  if (now - lastSample < 8)
    return;
  lastSample = now;
  TS_Point p;
  bool down = sampleTouch(p);
  if (down) {
    released = 0;
    if (!active) {
      capture = RawPress{};
      started = now;
      active = true;
      capture.first = {(float)p.x, (float)p.y};
    }
    capture.last = {(float)p.x, (float)p.y};
    if (capture.samples < 500) {
      capture.sumX += p.x;
      capture.sumY += p.y;
      capture.samples++;
    }
  } else if (active && ++released >= 2) {
    active = false;
    released = 0;
    if (capture.samples >= 2) {
      rawSumX = capture.sumX;
      rawSumY = capture.sumY;
      rawSamples = capture.samples;
      firstRaw = capture.first;
      lastRaw = capture.last;
      pressStart = now - (now - started);
      lastTouch = now;
      wakeOnly = view.pocket;
      if (wakeOnly) {
        view.pocket = false;
        brightness();
        uiDirty = true;
      }
      finishPress();
    }
  }
  touchDown = active;
}
int split(char *s, char *out[], int cap) {
  int n = 1;
  out[0] = s;
  for (char *p = s; *p && n < cap; p++)
    if (*p == '|') {
      *p = 0;
      out[n++] = p + 1;
    }
  return n;
}
// Compatibility signal-variation telemetry. Experimental, never a person detector.
bool senseEnabled = false, senseCal = false;
int senseThreshold = 35;
int16_t senseHist[32]{};
uint8_t senseHead = 0, senseCount = 0;
float senseBaseline = 0, senseVar = 0;
char senseMac[18]{};
int senseRssi = -100;
uint32_t senseAt = 0, senseSeen = 0;
float variance() {
  if (senseCount < 4)
    return 0;
  float mean = 0, v = 0;
  for (int i = 0; i < senseCount; i++)
    mean += senseHist[i];
  mean /= senseCount;
  for (int i = 0; i < senseCount; i++) {
    float x = senseHist[i] - mean;
    v += x * x;
  }
  return v / senseCount;
}
void senseFeed(const Observation &o) {
  if (!senseEnabled || o.kind != 'W' || strcmp(o.frame, "beacon"))
    return;
  if (!senseMac[0] || millis() - senseSeen > 20000) {
    copyText(senseMac, o.mac, 18);
    senseCount = senseHead = 0;
    senseCal = false;
  }
  if (strcmp(o.mac, senseMac))
    return;
  senseSeen = millis();
  senseRssi = o.rssi;
  senseHist[senseHead] = o.rssi;
  senseHead = (senseHead + 1) % 32;
  if (senseCount < 32)
    senseCount++;
  senseVar = variance();
}
void processLine(char *s) {
  char *f[10];
  int n = split(s, f, 10);
  if (!strcmp(f[0], "DET") && n >= 2 && macValid(f[1])) {
    lastAppRx = millis();
    char k = n > 7 && f[7][0] == 'B' ? 'B' : 'W';
    int idx = model.find(f[1], k);
    if (idx < 0) {
      for (int i = 0; i < DEVICE_CAP; i++)
        if (!model.devices[i].used) {
          idx = i;
          Device &d = model.devices[i];
          d = Device{};
          d.used = true;
          d.unobserved = true;
          copyText(d.mac, f[1], 18);
          d.kind = k;
          if (n > 5)
            d.rssi = constrain(atoi(f[5]), -127, 0);
          break;
        }
    }
    if (idx >= 0) {
      Device &d = model.devices[idx];
      if (n > 6)
        copyText(d.phoneLabel, f[6], sizeof(d.phoneLabel));
      if (n > 2)
        d.phoneConf = f[2][0];
      uiDirty = true;
    }
    return;
  }
  if (strcmp(f[0], "CMD") || n < 3)
    return;
  bool ok = true;
  lastAppRx = millis();
  if (!strcmp(f[1], "mode")) {
    if (!strcmp(f[2], "auto"))
      setMode(0);
    else if (!strcmp(f[2], "wifi"))
      setMode(1);
    else if (!strcmp(f[2], "ble"))
      setMode(2);
    else
      ok = false;
  } else if (!strcmp(f[1], "ch")) {
    int c = atoi(f[2]);
    channelLock = c >= 1 && c <= 13 ? c : 0;
    if (channelLock) {
      view.channel = channelLock;
      if (radioWifi)
        esp_wifi_set_channel(channelLock, WIFI_SECOND_CHAN_NONE);
    }
  } else if (!strcmp(f[1], "win") && n >= 4) {
    wifiWindowMs = constrain(atoi(f[2]), 1000, 60000);
    bleWindowMs = constrain(atoi(f[3]), 500, 60000);
  } else if (!strcmp(f[1], "hop"))
    hopEnabled = !strcmp(f[2], "on") || !strcmp(f[2], "1") || !strcmp(f[2], "true");
  else if (!strcmp(f[1], "sense")) {
    if (!strcmp(f[2], "on"))
      senseEnabled = true;
    else if (!strcmp(f[2], "off"))
      senseEnabled = false;
    else if (!strcmp(f[2], "threshold") && n >= 4)
      senseThreshold = constrain(atoi(f[3]), 5, 95);
    else if (!strcmp(f[2], "calibrate")) {
      senseBaseline = fmaxf(.3f, variance());
      senseCal = senseCount >= 12;
      Serial.printf("{\"_status\":\"wall_cal\",\"baseline\":%.2f}\n", senseBaseline);
    } else
      ok = false;
  } else if (!strcmp(f[1], "touch")) {
    if (!strcmp(f[2], "calibrate"))
      beginCalibration();
    else if (!strcmp(f[2], "status"))
      Serial.printf("{\"_millie\":\"touch\",\"controller\":\"XPT2046\",\"ready\":%s,\"calibrated\":%s,\"step\":%d,\"taps\":%"
                    "lu,\"errors\":%lu,\"irq_event\":%s,\"irq_pin_low\":%s,\"pressed\":%s,\"pressure\":%d,\"raw_x\":%.1f,\"raw_y\":%.1f}\n",
                    touchReady ? "true" : "false",
                    (view.calStep < 0 && calibrationValid(calibration)) ? "true" : "false",
                    view.calStep, (unsigned long)touchReleases,
                    (unsigned long)(touchFailures + touchQueueDrops), touchIrqEvent ? "true" : "false",
                    touchIrqPinLow ? "true" : "false", touchPressed ? "true" : "false",
                    touchPressure, latestRaw.x, latestRaw.y);
    else
      ok = false;
  } else if (!strcmp(f[1], "ui") && !strcmp(f[2], "page") && n >= 4) {
    int p = atoi(f[3]);
    if (p >= 0 && p <= 4) {
      view.page = (Page)p;
      view.offset = 0;
      uiDirty = true;
    } else
      ok = false;
  } else if (!strcmp(f[1], "log") && !strcmp(f[2], "export"))
    exportLog();
  else if (!strcmp(f[1], "save") && !strcmp(f[2], "now"))
    saveState();
  else if (!strcmp(f[1], "link") && !strcmp(f[2], "ping")) {
  } else
    ok = false;
  Serial.printf("{\"_status\":\"cmd_ack\",\"cmd\":\"%s\",\"ok\":%s}\n", f[1],
                ok ? "true" : "false");
  uiDirty = true;
}
void serialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputPos && !inputOverflow) {
        input[inputPos] = 0;
        processLine(input);
      }
      inputPos = 0;
      inputOverflow = false;
    } else if (inputPos < sizeof(input) - 1)
      input[inputPos++] = c;
    else
      inputOverflow = true;
  }
}
void sendObservation(const Observation &o) {
  char name[205];
  jsonEscape(o.name, name, sizeof(name));
  if (o.kind == 'W') {
    Serial.printf("{\"t\":\"wifi\",\"mac\":\"%s\",\"rssi\":%d,\"ch\":%u,\"ftype\":\"%s\",\"ssid\":\"%s\"}\n",
                  o.mac, o.rssi, o.channel, o.frame, name);
    wifiHits++;
  } else {
    Serial.printf("{\"t\":\"ble\",\"mac\":\"%s\",\"rssi\":%d,\"name\":\"%s\",\"manufacturer\":\"%s\",\"services\":\"%s\"}\n",
                  o.mac, o.rssi, name, o.manufacturer, o.services);
    bleHits++;
  }
  txLines++;
}
void setup() {
  Serial.setTxBufferSize(4096);
  Serial.begin(115200);
  delay(200);
  esp_log_level_set("*", ESP_LOG_NONE);
  esp_err_t nv = nvs_flash_init();
  if (nv != ESP_OK) {
    Serial.printf("{\"_status\":\"nvs_init_failed\",\"error\":%d}\n", (int)nv);
  }
  tft.init();
  tft.setRotation(0);
  tft.setTextWrap(false);
  frame.setColorDepth(4);
  buffered = frame.createSprite(240, 320) != nullptr;
  if (buffered) {
    uint16_t colors[16] = {BG, GREEN, DIM, AMBER, RED, GREY, CYAN, PINK};
    frame.createPalette(colors, 16);
    frame.setTextWrap(false);
  }
  pinMode(27, OUTPUT);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(27, 0);
  pinMode(0, INPUT_PULLUP);
  prefsReady = nv == ESP_OK && prefs.begin("millie2", false);
  view.storeOk = prefsReady;
  if (prefsReady) {
    Stored st{};
    if (prefs.getBytesLength("state") == sizeof(st) &&
        prefs.getBytes("state", &st, sizeof(st)) == sizeof(st) && st.magic == 0x4d494c32 &&
        st.version == 1 && st.head < LOG_CAP && st.count <= LOG_CAP && st.brightness >= 20 &&
        st.brightness <= 100 && st.filter <= 1) {
      model.saved = st;
      model.boot = st.bootCounter + 1;
    }
    if (prefs.getBytesLength("touch") == sizeof(calibration))
      prefs.getBytes("touch", &calibration, sizeof(calibration));
  }
  for (const auto &r : model.saved.rules)
    if (r.kind && r.watch) {
      for (int i = 0; i < DEVICE_CAP; i++)
        if (!model.devices[i].used) {
          Device &d = model.devices[i];
          d.used = true;
          d.unobserved = true;
          d.kind = r.kind;
          d.rssi = -100;
          copyText(d.mac, r.mac, 18);
          break;
        }
    }
  model.saved.bootCounter = model.boot;
  model.dirty = true;
  saveState();
  touchReady = false;
  view.calStep = -1;
  lastTouch = millis();
  brightness();
  observations = xQueueCreate(48, sizeof(Observation));
  if (!observations) {
    Serial.println("{\"_status\":\"queue_failed\"}");
    while (true)
      delay(1000);
  }
  esp_event_loop_create_default();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  cfg.nvs_enable = 0;
  esp_err_t wr = esp_wifi_init(&cfg);
  if (wr == ESP_OK)
    wr = esp_wifi_set_storage(WIFI_STORAGE_RAM);
  if (wr == ESP_OK)
    wr = esp_wifi_set_mode(WIFI_MODE_NULL);
  if (wr == ESP_OK)
    wr = esp_wifi_start();
  if (wr != ESP_OK) {
    Serial.printf("{\"_status\":\"wifi_init_failed\",\"error\":%d}\n", (int)wr);
    notice("RADIO INIT FAILED");
  } else
    startWifi();
  sliceStart = millis();
  Serial.printf("{\"_status\":\"MILLIE CIVOPS "
                "boot\",\"fw\":\"%s\",\"standalone\":true,\"protocol\":1,\"touch_ready\":%s,"
                "\"touch_cal\":%s}\n",
                FW, touchReady ? "true" : "false",
                (view.calStep < 0 && calibrationValid(calibration)) ? "true" : "false");
  repaint();
}
void loop() {
  uint32_t now = millis();
  model.now = now;
  serialInput();
  if (forceMode == 0 && now - sliceStart >= (radioWifi ? wifiWindowMs : bleWindowMs)) {
    if (radioWifi) {
      stopWifi();
      startBle();
    } else {
      stopBle();
      startWifi();
    }
    sliceStart = now;
    uiDirty = true;
  }
  if (radioWifi && hopEnabled && !channelLock && now - lastHop >= 220) {
    lastHop = now;
    view.channel = view.channel % 13 + 1;
    esp_wifi_set_channel(view.channel, WIFI_SECOND_CHAN_NONE);
  }
  Observation o{};
  int drained = 0;
  while (drained++ < 6 && Serial.availableForWrite() > 600 &&
         xQueueReceive(observations, &o, 0) == pdTRUE) {
    model.ingest(o);
    senseFeed(o);
    sendObservation(o);
    uiDirty = true;
  }
  if (now - lastControlReport >= 1000 && Serial.availableForWrite() > 300) {
    lastControlReport = now;
    static const char *controlNames[16] = {"ctrl0", "ctrl1", "trigger", "tack", "beamforming", "pspoll", "rts", "cts", "ack", "cfend", "cfend_ack", "control11", "control12", "control13", "control14", "control15"};
    for (uint8_t sub = 0; sub < 16; ++sub) {
      uint32_t count = __atomic_exchange_n(&controlCounts[sub], 0, __ATOMIC_RELAXED);
      if (count && Serial.availableForWrite() > 220) {
        Serial.printf("{\"t\":\"wifi_control\",\"ftype\":\"%s\",\"count\":%lu,\"rssi\":%d,\"ch\":%u}\n",
                      controlNames[sub], (unsigned long)count, controlRssi[sub], controlChannels[sub]);
        txLines++;
      } else if (count) {
        __atomic_add_fetch(&controlCounts[sub], count, __ATOMIC_RELAXED);
      }
    }
  }
  if (senseEnabled && now - senseAt >= 1000) {
    senseAt = now;
    float motion =
        senseCal ? fminf(1.5f, senseVar / fmaxf(.5f, senseBaseline)) : fminf(1.0f, senseVar / 25);
    bool present = senseCount >= 12 && now - senseSeen < 20000 && motion > senseThreshold / 100.0f;
    Serial.printf("{\"t\":\"wallsense\",\"motion\":%.2f,\"presence\":%s,\"var\":%.2f,\"anchor\":\"%"
                  "s\",\"rssi\":%d,\"cal\":%s,\"experimental\":true}\n",
                  motion, present ? "true" : "false", senseVar, senseMac, senseRssi,
                  senseCal ? "true" : "false");
    txLines++;
  }
  if (now - lastBeat >= 3000) {
    lastBeat = now;
    Serial.printf(
        "{\"_status\":\"alive\",\"fw\":\"%s\",\"radio\":\"%s\",\"ch\":%u,\"wifi\":%lu,\"ble\":%lu,"
        "\"tx\":%lu,\"mode\":\"%s\",\"lock\":%u,\"sense\":%d,\"presence\":false,\"drops\":%lu,"
        "\"heap\":%lu,\"ui\":%d,\"touch_cal\":%s,\"touch_taps\":%lu,\"store\":%s}\n",
        FW, radioWifi ? "wifi" : "ble", view.channel, (unsigned long)wifiHits,
        (unsigned long)bleHits, (unsigned long)txLines,
        forceMode == 1   ? "wifi"
        : forceMode == 2 ? "ble"
                         : "auto",
        channelLock, senseEnabled, (unsigned long)queueDrops, (unsigned long)ESP.getFreeHeap(),
        view.page, (view.calStep < 0 && calibrationValid(calibration)) ? "true" : "false",
        (unsigned long)touchReleases, view.storeOk ? "true" : "false");
  }
  if (model.dirty && now - lastSave >= 60000)
    saveState();
  view.wifi = radioWifi;
  view.mode = forceMode;
  view.tx = txLines;
  view.drops = queueDrops;
  view.heap = ESP.getFreeHeap();
  view.linked = lastAppRx && now - lastAppRx < 15000;
  if (uiDirty || now - lastDraw >= 200) {
    repaint();
    uiDirty = false;
    lastDraw = now;
  }
  brightness();
  delay(2);
}
