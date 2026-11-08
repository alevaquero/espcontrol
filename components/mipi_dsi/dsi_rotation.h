#pragma once

// Driver-side rotation for the MIPI DSI panel. LVGL hands over blocks in
// rotated (logical) coordinates; these helpers find where a block lands on the
// physical panel and copy its pixels into that orientation. The mapping
// matches MipiDsi::draw_pixel_at: rotations are clockwise.

#include <cstdint>
#include <cstring>

namespace esphome::mipi_dsi {

struct RotatedBlock {
  int x;
  int y;
  int w;
  int h;
};

// rotation is 0, 90, 180 or 270; panel_w/panel_h are the physical size.
inline RotatedBlock rotated_block(int rotation, int x, int y, int w, int h, int panel_w, int panel_h) {
  switch (rotation) {
    case 90:
      return {panel_w - (y + h), x, h, w};
    case 180:
      return {panel_w - (x + w), panel_h - (y + h), w, h};
    case 270:
      return {y, panel_h - (x + w), h, w};
    default:
      return {x, y, w, h};
  }
}

// Copies a w x h logical block into dst laid out as the physical block
// returned by rotated_block. bpp is bytes per pixel.
inline void rotate_block_cpu(const uint8_t *src, uint8_t *dst, int w, int h, int bpp, int rotation) {
  for (int ly = 0; ly != h; ly++) {
    const uint8_t *row = src + static_cast<size_t>(ly) * w * bpp;
    for (int lx = 0; lx != w; lx++) {
      size_t index;
      switch (rotation) {
        case 90:
          index = static_cast<size_t>(lx) * h + (h - 1 - ly);
          break;
        case 180:
          index = static_cast<size_t>(h - 1 - ly) * w + (w - 1 - lx);
          break;
        case 270:
          index = static_cast<size_t>(w - 1 - lx) * h + ly;
          break;
        default:
          index = static_cast<size_t>(ly) * w + lx;
          break;
      }
      std::memcpy(dst + index * bpp, row + static_cast<size_t>(lx) * bpp, bpp);
    }
  }
}

}  // namespace esphome::mipi_dsi
