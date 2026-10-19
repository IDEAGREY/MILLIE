#pragma once
#include "core.h"
namespace millie {
static const int LIST_TOP = 38, LIST_ROW = 30, LIST_ROWS = 7, SET_TOP = 36, SET_ROW = 29;
enum Page : uint8_t { SCAN, DEVICES, WATCH, LOG, SETUP, DETAIL };
static const uint16_t BG = 0x0020, GREEN = 0x3fe2, DIM = 0x1b42, AMBER = 0xfd80, RED = 0xf986,
                      GREY = 0x8410, CYAN = 0x249f, PINK = 0xf81f;
struct View {
  Page page = SCAN, back = DEVICES;
  char selected[18]{};
  char selectedKind = 'W';
  int offset = 0;
  uint8_t mode = 0, channel = 1;
  bool wifi = true, linked = false, storeOk = true, pocket = false;
  uint32_t tx = 0, drops = 0, heap = 0;
  int calStep = -1;
  const char *notice = "";
  uint32_t noticeUntil = 0;
  Device historical{};
};
inline uint16_t sigColor(Signature s) {
  return grade(s) == 'M' ? AMBER : s == GENERIC ? GREEN : GREY;
}
inline const char *modeName(uint8_t m) { return m == 1 ? "WIFI" : m == 2 ? "BLE" : "AUTO"; }
inline void elapsedText(uint32_t seconds, char *out, size_t cap) {
  snprintf(out, cap, "%02lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
}
template <class C>
void button(C &c, int x, int y, int w, int h, const char *text, bool active = false) {
  c.rect(x, y, w, h, active ? PINK : DIM);
  c.text(x + 5, y + (h - 8) / 2, text, active ? PINK : GREY);
  if (active)
    c.fill(x + 2, y + h - 3, w - 4, 1, PINK);
}
template <class C> void drawUI(C &c, Model &m, View &v) {
  c.fill(0, 0, 240, 320, BG);
  char b[96];
  c.text(5, 5, "MILLIE CIVOPS", PINK);
  c.text(170, 5, v.linked ? "APP:RX" : "APP:--", v.linked ? CYAN : GREY);
  if (!v.storeOk)
    c.text(190, 20, "ST!", AMBER);
  snprintf(b, sizeof(b), "%s CH%02d", v.wifi ? "WIFI" : "BLE ", v.channel);
  c.text(5, 20, b, GREEN);
  snprintf(b, sizeof(b), "W:%lu", (unsigned long)m.live('W'));
  c.text(92, 20, b, AMBER);
  snprintf(b, sizeof(b), "B:%lu", (unsigned long)m.live('B'));
  c.text(153, 20, b, CYAN);
  c.line(0, 32, 239, 32, DIM);

  const int cx = 120, cy = 91, radius = 52;
  for (int ring = 1; ring <= 3; ++ring)
    c.circle(cx, cy, radius * ring / 3, DIM);
  c.line(cx - radius, cy, cx + radius, cy, DIM);
  c.line(cx, cy - radius, cx, cy + radius, DIM);
  float sweep = (m.now % 6000) * 6.2831853f / 6000.0f;
  c.line(cx, cy, cx + (int)(cosf(sweep) * radius), cy + (int)(sinf(sweep) * radius), GREEN);
  c.fill(cx - 2, cy - 2, 5, 5, PINK);
  snprintf(b, sizeof(b), "W %02lu", (unsigned long)m.live('W'));
  c.text(47, 149, b, AMBER);
  snprintf(b, sizeof(b), "B %02lu", (unsigned long)m.live('B'));
  c.text(105, 149, b, CYAN);
  snprintf(b, sizeof(b), "CH%02u", v.channel);
  c.text(161, 149, b, GREEN);
  c.line(0, 161, 239, 161, DIM);
  c.text(5, 166, "LIVE LOG", PINK);

  int recent[DEVICE_CAP], count = 0;
  for (int i = 0; i < DEVICE_CAP; ++i) {
    const Device &d = m.devices[i];
    if (d.used && !d.unobserved && m.now - d.last < 30000)
      recent[count++] = i;
  }
  for (int a = 0; a < count; ++a)
    for (int j = a + 1; j < count; ++j)
      if (m.devices[recent[j]].last > m.devices[recent[a]].last) {
        int t = recent[a];
        recent[a] = recent[j];
        recent[j] = t;
      }
  int rows = count < 6 ? count : 6;
  for (int row = 0; row < rows; ++row) {
    const Device &d = m.devices[recent[row]];
    const char *transport = d.kind == 'B' ? "BLE" : "WIFI";
    if (d.name[0])
      snprintf(b, sizeof(b), "%-3s %-8.8s %-17.17s %4d", transport, d.name, d.mac, d.rssi);
    else
      snprintf(b, sizeof(b), "%-3s %-17.17s %4d", transport, d.mac, d.rssi);
    int y = 181 + row * 22;
    uint16_t color = d.kind == 'B' ? CYAN : AMBER;
    c.text(3, y, b, color);
    c.line(3, y + 15, 237, y + 15, DIM);
  }
  return;

  if (v.calStep >= 0) {
    c.text(10, 64, "TOUCH CALIBRATION", GREEN);
    static const int tx[] = {20, 220, 20, 120}, ty[] = {112, 112, 260, 188};
    int step = v.calStep % 4;
    c.line(tx[step] - 10, ty[step], tx[step] + 10, ty[step], PINK);
    c.line(tx[step], ty[step] - 10, tx[step], ty[step] + 10, PINK);
    c.rect(tx[step] - 13, ty[step] - 13, 27, 27, GREEN);
    snprintf(b, sizeof(b), "TARGET %d/4 %s", step + 1, "");
    c.text(10, 292, b, GREEN);
    if (v.notice[0] && m.now < v.noticeUntil)
      c.text(10, 276, v.notice, AMBER);
    return;
  }
  if (v.page == SCAN) {
    const int cx = 120, cy = 121, r = 82;
    for (int k = 1; k <= 3; k++)
      c.circle(cx, cy, r * k / 3, DIM);
    c.line(cx - r, cy, cx + r, cy, DIM);
    c.line(cx, cy - r, cx, cy + r, DIM);
    int order[DEVICE_CAP], n = m.list(order);
    int shown = 0;
    for (int k = 0; k < n && shown < 8; k++) {
      const Device &d = m.devices[order[k]];
      if (m.now - d.last >= 30000 || d.unobserved)
        continue;
      int rr = 8 + (int)(fmaxf(0, fminf(1, (-d.rssi - 30) / 65.0f)) * 72);
      c.circle(cx, cy, rr, d.kind == 'B' ? CYAN : AMBER);
      shown++;
    }
    c.fill(cx - 2, cy - 2, 5, 5, GREEN);
    snprintf(b, sizeof(b), "LIVE %02d  /  MODE %s", m.live(), modeName(v.mode));
    c.text(5, 210, b, GREEN);
    snprintf(b, sizeof(b), "TX %lu  DROP %lu", (unsigned long)v.tx, (unsigned long)v.drops);
    c.text(5, 225, b, GREY);
    char tm[16];
    elapsedText(m.now / 1000, tm, sizeof(tm));
    snprintf(b, sizeof(b), "SESSION B%lu +%s", (unsigned long)m.boot, tm);
    c.text(5, 240, b, DIM);
    c.line(0, 254, 239, 254, DIM);
    if (m.alertIndex >= 0 && m.now - m.alertAt < 8000) {
      const Device &d = m.devices[m.alertIndex];
      snprintf(b, sizeof(b), "> %s %s", m.watched(d) ? "WATCH" : tag(d.sig), d.mac + 9);
      c.text(5, 265, b, AMBER);
    } else
      c.text(5, 265, "SCANNING", DIM);
  } else if (v.page == DEVICES || v.page == WATCH) {
    int order[DEVICE_CAP], n = m.list(order, v.page == WATCH);
    if (v.offset >= n)
      v.offset = n ? ((n - 1) / LIST_ROWS) * LIST_ROWS : 0;
    if (!n) {
      c.text(8, 75, v.page == WATCH ? "NO WATCHES" : "NO DEVICES", GREEN);
    }
    for (int row = 0; row < LIST_ROWS && v.offset + row < n; row++) {
      const Device &d = m.devices[order[v.offset + row]];
      int y = LIST_TOP + row * LIST_ROW;
      bool live = m.now - d.last < 30000 && !d.unobserved;
      snprintf(b, sizeof(b), "%c %c %-16.16s %4d",
               m.watched(d)   ? '*'
               : m.ignored(d) ? '-'
                              : '>',
               d.kind, d.name[0] ? d.name : d.mac, d.rssi);
      c.text(5, y, b, live ? GREEN : GREY);
      snprintf(b, sizeof(b), "  %-9s [%c] %s", tag(d.sig), grade(d.sig), live ? "LIVE" : "STALE");
      c.text(5, y + 12, b, sigColor(d.sig));
      c.line(4, y + 26, 235, y + 26, DIM);
    }
    snprintf(b, sizeof(b), "%d-%d / %d", n ? v.offset + 1 : 0,
             (v.offset + LIST_ROWS < n ? v.offset + LIST_ROWS : n), n);
    c.text(75, 265, b, GREY);
    button(c, 4, 254, 59, 29, "< PREV");
    button(c, 177, 254, 59, 29, "NEXT >");
  } else if (v.page == DETAIL) {
    int idx = m.find(v.selected, v.selectedKind);
    Device *d = idx >= 0 ? &m.devices[idx] : &v.historical;
    button(c, 5, 37, 65, 27, "< BACK");
    c.text(80, 47, d->kind == 'B' ? "BLE RECORD" : "WIFI RECORD", DIM);
    c.text(5, 73, d->name[0] ? d->name : "UNNAMED DEVICE", GREEN);
    c.text(5, 88, d->mac, GREY);
    snprintf(b, sizeof(b), "MATCH %s / %c", tag(d->sig), grade(d->sig));
    c.text(5, 107, b, sigColor(d->sig));
    snprintf(b, sizeof(b), "RSSI %d dBm  CH %02d  HITS %lu", d->rssi, d->channel,
             (unsigned long)d->hits);
    c.text(5, 141, b, GREEN);
    c.rect(5, 156, 230, 57, DIM);
    for (int i = 1; i < d->traceCount; i++) {
      int ia = (d->traceHead + TRACE_CAP - d->traceCount + i - 1) % TRACE_CAP,
          ib = (ia + 1) % TRACE_CAP;
      int y1 = 209 - (int)(fmaxf(0, fminf(1, (d->trace[ia] + 100) / 75.0f)) * 48),
          y2 = 209 - (int)(fmaxf(0, fminf(1, (d->trace[ib] + 100) / 75.0f)) * 48);
      c.line(9 + (i - 1) * 11, y1, 9 + i * 11, y2, GREEN);
    }
    if (d->phoneLabel[0]) {
      snprintf(b, sizeof(b), "APP> %.29s", d->phoneLabel);
      c.text(5, 221, b, CYAN);
    } else
      c.text(5, 221, "RSSI TRACE", DIM);
    if (d->unobserved)
      snprintf(b, sizeof(b), "SAVED / NOT SEEN THIS BOOT");
    else
      snprintf(b, sizeof(b), "LAST %lus AGO", (unsigned long)((m.now - d->last) / 1000));
    c.text(5, 238, b, GREY);
    button(c, 5, 254, 110, 29, m.watched(*d) ? "UNWATCH" : "WATCH", m.watched(*d));
    button(c, 125, 254, 110, 29, m.ignored(*d) ? "UNMUTE" : "MUTE", m.ignored(*d));
  } else if (v.page == LOG) {
    if (v.offset >= m.saved.count)
      v.offset = 0;
    for (int row = 0; row < LIST_ROWS; row++) {
      const LogEntry *e = m.logAt(v.offset + row);
      if (!e)
        break;
      int y = LIST_TOP + row * LIST_ROW;
      char tm[16];
      elapsedText(e->seconds, tm, sizeof(tm));
      snprintf(b, sizeof(b), "B%lu +%s  %s", (unsigned long)e->boot, tm,
               e->event == 2   ? "WATCH"
               : e->event == 1 ? "MATCH"
                               : "NEW");
      c.text(5, y, b, e->event ? AMBER : GREEN);
      snprintf(b, sizeof(b), "%c %-16.16s %4d", e->kind, e->name[0] ? e->name : e->mac, e->rssi);
      c.text(5, y + 12, b, GREY);
      c.line(4, y + 26, 235, y + 26, DIM);
    }
    if (!m.saved.count) {
      c.text(8, 80, "NO EVENTS", GREEN);
    }
    button(c, 4, 254, 59, 29, "< PREV");
    snprintf(b, sizeof(b), "%d EVENTS", m.saved.count);
    c.text(75, 265, b, GREY);
    button(c, 177, 254, 59, 29, "NEXT >");
  } else if (v.page == SETUP) {
    const char *labels[] = {"RADIO MODE", "CHANNEL LOCK",    "ALERT FILTER",   "ALERTS",
                            "BRIGHTNESS", "CALIBRATE TOUCH", "EXPORT LOG USB", "SAVE SETTINGS"};
    for (int i = 0; i < 8; i++) {
      int y = SET_TOP + i * SET_ROW;
      c.text(7, y + 9, labels[i], i == 5 ? AMBER : GREEN);
      const char *val = "";
      char value[24];
      if (i == 0)
        val = modeName(v.mode);
      if (i == 1) {
        if (v.channel)
          snprintf(value, sizeof(value), "CH%02u", v.channel);
        else
          snprintf(value, sizeof(value), "HOP");
        val = value;
      }
      if (i == 2)
        val = m.saved.filter ? "MED+" : "ALL";
      if (i == 3)
        val = m.saved.alerts ? "ON" : "OFF";
      if (i == 4) {
        snprintf(value, sizeof(value), "%u%%", m.saved.brightness);
        val = value;
      }
      if (i >= 5)
        val = ">";
      c.text(i == 1 ? 156 : 180, y + 9, val, GREY);
      c.line(5, y + SET_ROW - 1, 235, y + SET_ROW - 1, DIM);
    }
  }
  if (v.notice[0] && m.now < v.noticeUntil) {
    c.fill(0, 273, 240, 14, BG);
    c.text(5, 276, v.notice, AMBER);
  }
  static const char *nav[] = {"SCAN", "DEV", "WATCH", "LOG", "SET"};
  for (int i = 0; i < 5; i++)
    button(c, i * 48, 288, 48, 32, nav[i], v.page == i || (v.page == DETAIL && v.back == i));
}
} // namespace millie
