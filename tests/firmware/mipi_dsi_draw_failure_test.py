#!/usr/bin/env python3
"""Run production display writes with failed submissions, lost completions, and real wait ordering."""
from pathlib import Path
import os
import re
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
production = (root / "components/mipi_dsi/mipi_dsi.cpp").read_text()
write = re.search(r"^void MipiDsi::write_to_display_\(.*?^\}", production, re.M | re.S)[0]
timeout = re.search(r"^static constexpr uint32_t TRANSFER_TIMEOUT_MS = .*?;$", production, re.M)[0]
wait = re.search(r"^bool MipiDsi::wait_for_transfer_\(.*?^\}", production, re.M | re.S)[0]
source = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
using esp_err_t = int;
constexpr int ESP_OK = 0, pdTRUE = 1;
#define pdMS_TO_TICKS(ms) (ms)
static int waits = 0, draws = 0, failure_at = 0, lost_at = 0;
static bool pending = false;
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
int esp_lcd_panel_draw_bitmap(int, int, int, int, int, const uint8_t *) {
  assert(!pending);  // Never reuse the source before the previous copy finishes.
  ++draws;
  if (draws == failure_at) return -1;
  pending = true;
  return ESP_OK;
}
int xSemaphoreTake(int, uint32_t timeout) {
  assert(timeout > 0 && timeout < 5000);  // Bounded, well under the task watchdog.
  assert(pending);  // A failed submission will never signal completion.
  pending = false;
  ++waits;
  return waits == lost_at ? 0 : 1;  // A lost completion interrupt times out.
}
struct MipiDsi {
  int handle_ = 0, color_depth_ = 1, io_lock_ = 0;
  uint32_t transfer_timeouts_ = 0;
  bool wait_for_transfer_();
  void write_to_display_(int, int, int, int, const uint8_t *, int, int, int);
};
''' + timeout + "\n" + wait + "\n" + write + r'''
int main() {
  MipiDsi display;
  uint8_t data[64]{};
  failure_at = 1;
  display.write_to_display_(0, 0, 2, 2, data, 0, 0, 0);
  assert(draws == 1 && waits == 0 && !pending);
  draws = waits = 0;
  failure_at = 2;
  display.write_to_display_(0, 0, 2, 3, data, 0, 0, 1);
  assert(draws == 2 && waits == 1 && !pending);
  draws = waits = failure_at = 0;
  display.write_to_display_(0, 0, 2, 2, data, 0, 0, 0);
  assert(draws == 1 && waits == 1 && !pending);
  draws = waits = 0;
  display.write_to_display_(0, 0, 2, 3, data, 0, 0, 1);
  assert(draws == 3 && waits == 3 && !pending);
  // A completion that never arrives returns instead of hanging, and a
  // line-by-line draw stops rather than queueing more copies behind it.
  draws = waits = 0;
  lost_at = 1;
  display.write_to_display_(0, 0, 2, 2, data, 0, 0, 0);
  assert(draws == 1 && waits == 1 && display.transfer_timeouts_ == 1);
  draws = waits = 0;
  display.write_to_display_(0, 0, 2, 3, data, 0, 0, 1);
  assert(draws == 1 && waits == 1 && display.transfer_timeouts_ == 2);
  std::puts("Display submission failures skip waits; lost completions time out; copies finish before buffer reuse");
}
'''
with tempfile.TemporaryDirectory(prefix="mipi-dsi-draw-") as directory:
    temp = Path(directory)
    (temp / "test.cpp").write_text(source)
    executable = temp / "test"
    subprocess.run(shlex.split(os.environ.get("CXX", "c++")) + [
        "-std=c++17", "-Wall", "-Wextra", "-Werror", str(temp / "test.cpp"), "-o", str(executable)
    ], check=True)
    subprocess.run([str(executable)], check=True)
