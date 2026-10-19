#include "../millie-standalone/ui.h"
#include <iostream>
#include <string>
#include <vector>
using namespace millie;
struct Canvas {
  std::vector<std::string> ops;
  void add(const char *type, int x, int y, int a, int b, uint16_t color, const char *s = "") {
    char t[1200], esc[600];
    jsonEscape(s, esc, sizeof(esc));
    snprintf(t, sizeof(t), "[\"%s\",%d,%d,%d,%d,%u,\"%s\"]", type, x, y, a, b, color, esc);
    ops.push_back(t);
  }
  void fill(int x, int y, int w, int h, uint16_t c) { add("fill", x, y, w, h, c); }
  void rect(int x, int y, int w, int h, uint16_t c) { add("rect", x, y, w, h, c); }
  void line(int x, int y, int a, int b, uint16_t c) { add("line", x, y, a, b, c); }
  void circle(int x, int y, int r, uint16_t c) { add("circle", x, y, r, 0, c); }
  void text(int x, int y, const char *s, uint16_t c) { add("text", x, y, 0, 0, c, s); }
};
int main() {
  Model m;
  View v;
  m.now = 20000;
  for (int i = 0; i < 12; i++) {
    Observation o{};
    snprintf(o.mac, 18, "24:0A:C4:10:20:%02X", i);
    snprintf(o.name, 33, "%s",
             i == 0   ? "Cafe Mesh"
             : i == 1 ? "Office WiFi"
             : i == 2 ? "Beacon"
             : i == 3 ? ""
                      : "Unnamed broadcast");
    o.kind = i % 2 ? 'B' : 'W';
    o.sig = GENERIC;
    o.rssi = -45 - i * 3;
    o.channel = 6;
    m.now += 1000;
    m.ingest(o);
  }
  int idx = m.find("24:0A:C4:10:20:00", 'W');
  m.toggleRule(m.devices[idx], true);
  m.boot = 4;
  v.heap = 120000;
  v.tx = 684;
  v.channel = 6;
  v.linked = true;
  std::cout << "[";
  for (int page = 0; page < 7; page++) {
    Canvas c;
    v.page = (Page)(page < 6 ? page : 0);
    v.calStep = page == 6 ? 0 : -1;
    if (page == 5) {
      copyText(v.selected, m.devices[idx].mac, 18);
      v.selectedKind = 'W';
      v.back = DEVICES;
    }
    drawUI(c, m, v);
    if (page)
      std::cout << ",";
    std::cout << "[";
    for (size_t i = 0; i < c.ops.size(); i++) {
      if (i)
        std::cout << ",";
      std::cout << c.ops[i];
    }
    std::cout << "]";
  }
  std::cout << "]";
}
