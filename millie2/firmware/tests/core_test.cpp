#include "../millie-standalone/core.h"
#include "../millie-standalone/touch_math.h"
#include "../millie-standalone/ui.h"
#include <assert.h>
#include <iostream>
#include <stdlib.h>
using namespace millie;
int main() {
  char out[40];
  uint8_t beacon[50]{};
  beacon[36] = 0;
  beacon[37] = 7;
  memcpy(beacon + 38, "flock-X", 7);
  ssidFromFrame(beacon, 45, 8, out, sizeof(out));
  assert(!strcmp(out, "flock-X"));
  ssidFromFrame(beacon, 41, 8, out, sizeof(out));
  assert(!out[0]);
  uint8_t probe[32]{};
  probe[24] = 0;
  probe[25] = 5;
  memcpy(probe + 26, "hello", 5);
  ssidFromFrame(probe, 31, 4, out, sizeof(out));
  assert(!strcmp(out, "hello"));
  for (int k = 0; k < 10000; k++) {
    uint8_t bytes[128];
    for (auto &b : bytes)
      b = rand() % 256;
    size_t len = rand() % 129;
    char small[5];
    ssidFromFrame(bytes, len, (k % 3 == 0) ? 4 : 8, small, 5);
    assert(strlen(small) < 5);
  }
  char esc[80];
  jsonEscape("x\"\\\n\r\t", esc, sizeof(esc));
  assert(!strcmp(esc, "x\\\"\\\\\\u000a\\u000d\\u0009"));
  char tiny[3];
  jsonEscape("\"\"", tiny, 3);
  assert(strlen(tiny) <= 2);
  Model m;
  Observation o{};
  copyText(o.mac, "AA:BB:CC:01:02:03", 18);
  o.kind = 'W';
  o.rssi = -60;
  o.sig = GENERIC;
  m.now = 1000;
  int idx = m.ingest(o);
  assert(idx >= 0 && m.live() == 1);
  assert(m.alertIndex < 0);
  assert(m.saved.count == 1);
  m.ingest(o);
  assert(m.saved.count == 1 && m.devices[idx].hits == 2);
  assert(m.toggleRule(m.devices[idx], true));
  assert(m.watched(m.devices[idx]));
  m.now = 62000;
  m.ingest(o);
  assert(m.logAt(0)->event == 2 && m.alertIndex == idx);
  assert(m.toggleRule(m.devices[idx], false));
  m.alertIndex = -1;
  m.now = 124000;
  m.ingest(o);
  assert(m.alertIndex < 0);
  for (int i = 0; i < 150; i++) {
    snprintf(o.mac, 18, "00:01:02:03:%02X:%02X", i / 256, i % 256);
    m.now++;
    m.ingest(o);
  }
  assert(m.find("AA:BB:CC:01:02:03", 'W') >= 0);
  assert(m.saved.count == LOG_CAP && m.saved.head < LOG_CAP);
  assert(m.logAt(LOG_CAP) == nullptr);
  int order[DEVICE_CAP];
  assert(m.list(order) == DEVICE_CAP);
  Model restored;
  restored.now = 400;
  Device &savedDevice = restored.devices[0];
  savedDevice.used = true;
  savedDevice.unobserved = true;
  savedDevice.kind = 'W';
  copyText(savedDevice.mac, "AA:BB:CC:01:02:03", 18);
  assert(restored.toggleRule(savedDevice, true));
  copyText(o.mac, savedDevice.mac, 18);
  o.kind = 'W';
  o.sig = GENERIC;
  restored.ingest(o);
  assert(restored.alertIndex == 0 && restored.logAt(0)->event == 2);
  assert(LIST_TOP > 32 && LIST_TOP + LIST_ROWS * LIST_ROW <= 254 && SET_TOP + 8 * SET_ROW <= 273);
  Calibration cal;
  Point screen[] = {{20, 112}, {220, 112}, {20, 260}},
        raw[] = {{3500, 500}, {3500, 3500}, {700, 500}};
  assert(solveCalibration(raw, screen, cal));
  assert(calibrationValid(cal));
  for (int i = 0; i < 3; i++) {
    Point p = project(cal, raw[i]);
    assert(hypotf(p.x - screen[i].x, p.y - screen[i].y) < .01f);
  }
  Point mid = project(cal, {2100, 2000});
  assert(hypotf(mid.x - 120, mid.y - 186) < .1f);
  Point bad[] = {{1000, 1000}, {1000, 1000}, {1000, 1000}};
  assert(!solveCalibration(bad, screen, cal));
  for (int i = 0; i < 5; i++) {
    assert(navAt(i * 48 + 24, 304) == i);
    assert(navAt(i * 48, 288) == i);
  }
  assert(navAt(240, 300) < 0 && navAt(10, 287) < 0 && navAt(-1, 300) < 0);
  std::cout << "PASS: frame bounds, JSON escaping, raw observation model, watch/ignore, eviction, "
               "history, affine touch math, nav targets\n";
}
