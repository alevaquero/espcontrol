#include <cassert>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>
#include "dsi_rotation.h"

using esphome::mipi_dsi::rotate_block_cpu;
using esphome::mipi_dsi::rotated_block;

// Reference mapping copied from MipiDsi::draw_pixel_at (clockwise rotations).
static void physical(int rotation, int panel_w, int panel_h, int &x, int &y) {
  switch (rotation) {
    case 90: std::swap(x, y); x = panel_w - x - 1; break;
    case 180: x = panel_w - x - 1; y = panel_h - y - 1; break;
    case 270: std::swap(x, y); y = panel_h - y - 1; break;
    default: break;
  }
}

int main() {
  const int panel_w = 48, panel_h = 64;
  for (int rotation : {0, 90, 180, 270}) {
    const bool swapped = rotation == 90 || rotation == 270;
    const int logical_w = swapped ? panel_h : panel_w;
    const int logical_h = swapped ? panel_w : panel_h;
    // Larger than one 16-pixel tile in both directions, with partial tiles.
    const int x = 3, y = 2, w = 37, h = 21;
    assert(x + w <= logical_w && y + h <= logical_h);
    std::vector<uint16_t> src(w * h), dst(w * h, 0);
    for (int i = 0; i < w * h; i++) src[i] = static_cast<uint16_t>(i + 1);
    rotate_block_cpu(reinterpret_cast<const uint8_t *>(src.data()), reinterpret_cast<uint8_t *>(dst.data()), w, h,
                     2, rotation);
    // The generic byte path (used for 24-bit pixels) must agree.
    std::vector<uint8_t> bytes(w * h * 3), bytes_out(w * h * 3, 0);
    for (int i = 0; i < w * h * 3; i++) bytes[i] = static_cast<uint8_t>(i * 7 + 1);
    rotate_block_cpu(bytes.data(), bytes_out.data(), w, h, 3, rotation);
    const auto block = rotated_block(rotation, x, y, w, h, panel_w, panel_h);
    assert(block.w * block.h == w * h);
    assert(block.x >= 0 && block.y >= 0 && block.x + block.w <= panel_w && block.y + block.h <= panel_h);
    // Every logical pixel must land where draw_pixel_at would put it.
    for (int ly = 0; ly < h; ly++) {
      for (int lx = 0; lx < w; lx++) {
        int px = x + lx, py = y + ly;
        physical(rotation, panel_w, panel_h, px, py);
        const int bx = px - block.x, by = py - block.y;
        assert(bx >= 0 && bx < block.w && by >= 0 && by < block.h);
        assert(dst[by * block.w + bx] == src[ly * w + lx]);
        for (int b = 0; b < 3; b++)
          assert(bytes_out[(by * block.w + bx) * 3 + b] == bytes[(ly * w + lx) * 3 + b]);
      }
    }
    // Written straight into a whole frame buffer: every pixel lands at its
    // panel position and nothing outside the block is touched.
    std::vector<uint16_t> frame(panel_w * panel_h, 0xBEEF);
    rotate_block_cpu(reinterpret_cast<const uint8_t *>(src.data()),
                     reinterpret_cast<uint8_t *>(frame.data() + block.y * panel_w + block.x), w, h, 2, rotation,
                     panel_w);
    std::vector<uint8_t> frame_bytes(panel_w * panel_h * 3, 0x5A);
    rotate_block_cpu(bytes.data(), frame_bytes.data() + (block.y * panel_w + block.x) * 3, w, h, 3, rotation,
                     panel_w);
    for (int py = 0; py < panel_h; py++) {
      for (int px = 0; px < panel_w; px++) {
        const bool inside = px >= block.x && px < block.x + block.w && py >= block.y && py < block.y + block.h;
        const uint16_t expected = inside ? dst[(py - block.y) * block.w + (px - block.x)] : 0xBEEF;
        assert(frame[py * panel_w + px] == expected);
        for (int b = 0; b < 3; b++) {
          const uint8_t expected_byte =
              inside ? bytes_out[((py - block.y) * block.w + (px - block.x)) * 3 + b] : 0x5A;
          assert(frame_bytes[(py * panel_w + px) * 3 + b] == expected_byte);
        }
      }
    }
  }
  std::puts("Driver rotation places every pixel where draw_pixel_at would");
}
