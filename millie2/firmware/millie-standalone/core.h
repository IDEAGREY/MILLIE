#pragma once
#include <ctype.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace millie {
static const int DEVICE_CAP = 64, RULE_CAP = 16, LOG_CAP = 24, TRACE_CAP = 20;
enum Signature : uint8_t {
  GENERIC,
  CHIPSET,
  FLOCK_NAME,
  FINDMY,
  TILE_SERVICE,
  SMARTTAG_SERVICE,
  NAME_HINT,
  PHONE_LABEL
};
inline const char *tag(Signature s) {
  (void)s;
  return "RAW";
}
inline char grade(Signature s) {
  (void)s;
  return 'L';
}
inline void copyText(char *dst, const char *src, size_t n) {
  if (!n)
    return;
  size_t i = 0;
  for (; src && src[i] && i + 1 < n; i++) {
    unsigned char c = src[i];
    dst[i] = (c >= 32 && c <= 126) ? char(c) : '.';
  }
  dst[i] = 0;
}
inline bool startsCI(const char *s, const char *k) {
  while (*k) {
    if (tolower((unsigned char)*s++) != tolower((unsigned char)*k++))
      return false;
  }
  return true;
}
inline bool macValid(const char *s) {
  if (!s || strlen(s) != 17)
    return false;
  for (int i = 0; i < 17; i++) {
    if (i % 3 == 2) {
      if (s[i] != ':')
        return false;
    } else if (!isxdigit((unsigned char)s[i]))
      return false;
  }
  return true;
}
inline void macText(const uint8_t *m, char *out) {
  snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
}
// Parse the management frame's tagged elements after its actual fixed fields.
inline void ssidFromFrame(const uint8_t *p, size_t n, uint8_t subtype, char *out, size_t cap) {
  if (!cap)
    return;
  out[0] = 0;
  size_t pos = subtype == 4 ? 24 : (subtype == 5 || subtype == 8 ? 36 : n);
  while (pos + 2 <= n) {
    uint8_t id = p[pos], len = p[pos + 1];
    pos += 2;
    if (pos + len > n)
      return;
    if (id == 0) {
      size_t m = len < cap - 1 ? len : cap - 1;
      for (size_t i = 0; i < m; i++)
        out[i] = p[pos + i] >= 32 && p[pos + i] <= 126 ? char(p[pos + i]) : '.';
      out[m] = 0;
      return;
    }
    pos += len;
  }
}
inline size_t jsonEscape(const char *s, char *out, size_t cap) {
  size_t j = 0;
  if (!cap)
    return 0;
  for (size_t i = 0; s && s[i]; i++) {
    unsigned char c = s[i];
    if (c == '"' || c == '\\') {
      if (j + 2 >= cap)
        break;
      out[j++] = '\\';
      out[j++] = c;
    } else if (c < 32) {
      if (j + 6 >= cap)
        break;
      snprintf(out + j, 7, "\\u%04x", c);
      j += 6;
    } else {
      if (j + 1 >= cap)
        break;
      out[j++] = c;
    }
  }
  out[j] = 0;
  return j;
}
struct Observation {
  char mac[18];
  char name[33];
  char frame[12];
  char manufacturer[65];
  char services[81];
  char kind;
  int8_t rssi;
  uint8_t channel;
  Signature sig;
};
struct Rule {
  char mac[18];
  char kind;
  bool watch, ignore;
};
struct Device {
  char mac[18], name[33], phoneLabel[28];
  char kind, phoneConf;
  Signature sig;
  int8_t rssi, trace[TRACE_CAP];
  uint8_t channel, traceCount, traceHead;
  uint32_t first, last, hits;
  bool used, unobserved;
};
struct LogEntry {
  char mac[18], name[24];
  char kind;
  Signature sig;
  int8_t rssi;
  uint8_t event;
  uint32_t seconds, boot;
};
struct Stored {
  uint32_t magic, version;
  Rule rules[RULE_CAP];
  LogEntry log[LOG_CAP];
  uint8_t head, count, brightness, filter;
  bool alerts;
  uint32_t bootCounter;
};
struct Model {
  Device devices[DEVICE_CAP]{};
  Stored saved{};
  uint32_t now = 0, boot = 1;
  int alertIndex = -1;
  uint32_t alertAt = 0;
  bool dirty = false;
  Model() {
    saved.magic = 0x4d494c32;
    saved.version = 1;
    saved.brightness = 80;
    saved.filter = 1;
    saved.alerts = true;
  }
  int find(const char *mac, char k) const {
    for (int i = 0; i < DEVICE_CAP; i++)
      if (devices[i].used && devices[i].kind == k && !strcasecmp(devices[i].mac, mac))
        return i;
    return -1;
  }
  int rule(const char *mac, char k) const {
    for (int i = 0; i < RULE_CAP; i++)
      if (saved.rules[i].kind == k && !strcasecmp(saved.rules[i].mac, mac))
        return i;
    return -1;
  }
  bool watched(const Device &d) const {
    int i = rule(d.mac, d.kind);
    return i >= 0 && saved.rules[i].watch;
  }
  bool ignored(const Device &d) const {
    int i = rule(d.mac, d.kind);
    return i >= 0 && saved.rules[i].ignore;
  }
  int ruleSlot(const Device &d) {
    int i = rule(d.mac, d.kind);
    if (i >= 0)
      return i;
    for (i = 0; i < RULE_CAP; i++)
      if (!saved.rules[i].kind) {
        copyText(saved.rules[i].mac, d.mac, 18);
        saved.rules[i].kind = d.kind;
        return i;
      }
    return -1;
  }
  bool toggleRule(const Device &d, bool watch) {
    int i = ruleSlot(d);
    if (i < 0)
      return false;
    if (watch)
      saved.rules[i].watch = !saved.rules[i].watch;
    else
      saved.rules[i].ignore = !saved.rules[i].ignore;
    if (!saved.rules[i].watch && !saved.rules[i].ignore)
      saved.rules[i] = Rule{};
    dirty = true;
    return true;
  }
  void logEvent(const Device &d, uint8_t event) {
    LogEntry &e = saved.log[saved.head];
    e = LogEntry{};
    copyText(e.mac, d.mac, 18);
    copyText(e.name, d.name, 24);
    e.kind = d.kind;
    e.sig = d.sig;
    e.rssi = d.rssi;
    e.event = event;
    e.seconds = now / 1000;
    e.boot = boot;
    saved.head = (saved.head + 1) % LOG_CAP;
    if (saved.count < LOG_CAP)
      saved.count++;
    dirty = true;
  }
  int ingest(const Observation &o) {
    if (!macValid(o.mac))
      return -1;
    int idx = find(o.mac, o.kind);
    bool fresh = idx < 0;
    if (fresh) {
      uint32_t age = 0;
      for (int i = 0; i < DEVICE_CAP; i++) {
        if (!devices[i].used) {
          idx = i;
          break;
        }
        uint32_t a = now - devices[i].last;
        if (!watched(devices[i]) && (idx < 0 || a > age)) {
          idx = i;
          age = a;
        }
      }
      if (idx < 0)
        return -1;
      if (idx == alertIndex)
        alertIndex = -1;
      devices[idx] = Device{};
      copyText(devices[idx].mac, o.mac, 18);
      devices[idx].kind = o.kind;
      devices[idx].first = now;
      devices[idx].used = true;
    }
    Device &d = devices[idx];
    bool returning = !fresh && (d.unobserved || now - d.last > 60000);
    Signature old = d.sig;
    d.rssi = o.rssi;
    d.channel = o.channel;
    d.last = now;
    d.hits++;
    d.unobserved = false;
    if (o.name[0])
      copyText(d.name, o.name, sizeof(d.name));
    if (o.sig != GENERIC || fresh)
      d.sig = o.sig;
    d.trace[d.traceHead] = o.rssi;
    d.traceHead = (d.traceHead + 1) % TRACE_CAP;
    if (d.traceCount < TRACE_CAP)
      d.traceCount++;
    bool significant = (fresh && d.sig != GENERIC && d.sig != CHIPSET) ||
                       (old != d.sig && d.sig != GENERIC && d.sig != CHIPSET);
    if (fresh || significant || (returning && watched(d)))
      logEvent(d, watched(d) ? 2 : significant ? 1 : 0);
    bool passes = saved.filter == 0 || grade(d.sig) == 'M';
    if (saved.alerts && !ignored(d) && ((significant && passes) || (returning && watched(d)))) {
      alertIndex = idx;
      alertAt = now;
    }
    return idx;
  }
  int list(int *out, bool watchOnly = false) const {
    int n = 0;
    for (int i = 0; i < DEVICE_CAP; i++)
      if (devices[i].used && (!watchOnly || watched(devices[i])))
        out[n++] = i;
    for (int a = 0; a < n; a++)
      for (int b = a + 1; b < n; b++) {
        const Device &x = devices[out[a]], &y = devices[out[b]];
        bool xl = now - x.last < 30000, yl = now - y.last < 30000;
        if ((yl && !xl) || (yl == xl && y.rssi > x.rssi)) {
          int t = out[a];
          out[a] = out[b];
          out[b] = t;
        }
      }
    return n;
  }
  int live(char kind = 0) const {
    int n = 0;
    for (const auto &d : devices)
      if (d.used && !d.unobserved && now - d.last < 30000 && (!kind || d.kind == kind))
        n++;
    return n;
  }
  const LogEntry *logAt(int offset) const {
    if (offset < 0 || offset >= saved.count)
      return nullptr;
    return &saved.log[(saved.head + LOG_CAP - 1 - offset) % LOG_CAP];
  }
};
} // namespace millie
