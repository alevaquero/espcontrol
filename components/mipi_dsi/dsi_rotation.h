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

// Row length in pixels of a block written compactly as the physical block
// returned by rotated_block.
inline int rotated_block_stride(int w, int h, int rotation) { return rotation == 90 || rotation == 270 ? h : w; }

// 16-bit pixels, processed in square tiles so reads and writes both stay
// within a few cache lines; a plain row-by-row transpose misses the cache on
// almost every write. dst_stride is the destination row length in pixels,
// so the block can be written straight into a larger frame buffer.
inline void rotate_block_rgb565(const uint16_t *src, uint16_t *dst, int w, int h, int rotation,
                                size_t dst_stride) {
  constexpr int TILE = 16;
  if (rotation == 180) {
    for (int ly = 0; ly != h; ly++) {
      const uint16_t *row = src + static_cast<size_t>(ly) * w;
      uint16_t *out = dst + static_cast<size_t>(h - 1 - ly) * dst_stride + (w - 1);
      for (int lx = 0; lx != w; lx++)
        out[-lx] = row[lx];
    }
    return;
  }
  for (int ty = 0; ty < h; ty += TILE) {
    const int y_end = ty + TILE < h ? ty + TILE : h;
    for (int tx = 0; tx < w; tx += TILE) {
      const int x_end = tx + TILE < w ? tx + TILE : w;
      for (int ly = ty; ly != y_end; ly++) {
        const uint16_t *row = src + static_cast<size_t>(ly) * w;
        if (rotation == 90) {
          uint16_t *col = dst + (h - 1 - ly);
          for (int lx = tx; lx != x_end; lx++)
            col[static_cast<size_t>(lx) * dst_stride] = row[lx];
        } else {  // 270
          uint16_t *col = dst + ly;
          for (int lx = tx; lx != x_end; lx++)
            col[static_cast<size_t>(w - 1 - lx) * dst_stride] = row[lx];
        }
      }
    }
  }
}

// Copies a w x h logical block into dst laid out as the physical block
// returned by rotated_block. bpp is bytes per pixel; dst_stride is the
// destination row length in pixels (0 for a compact block).
inline void rotate_block_cpu(const uint8_t *src, uint8_t *dst, int w, int h, int bpp, int rotation,
                             size_t dst_stride = 0) {
  if (dst_stride == 0)
    dst_stride = static_cast<size_t>(rotated_block_stride(w, h, rotation));
  if (bpp == 2 && rotation != 0 && (reinterpret_cast<uintptr_t>(src) & 1) == 0 &&
      (reinterpret_cast<uintptr_t>(dst) & 1) == 0) {
    rotate_block_rgb565(reinterpret_cast<const uint16_t *>(src), reinterpret_cast<uint16_t *>(dst), w, h,
                        rotation, dst_stride);
    return;
  }
  for (int ly = 0; ly != h; ly++) {
    const uint8_t *row = src + static_cast<size_t>(ly) * w * bpp;
    for (int lx = 0; lx != w; lx++) {
      size_t index;
      switch (rotation) {
        case 90:
          index = static_cast<size_t>(lx) * dst_stride + (h - 1 - ly);
          break;
        case 180:
          index = static_cast<size_t>(h - 1 - ly) * dst_stride + (w - 1 - lx);
          break;
        case 270:
          index = static_cast<size_t>(w - 1 - lx) * dst_stride + ly;
          break;
        default:
          index = static_cast<size_t>(ly) * dst_stride + lx;
          break;
      }
      std::memcpy(dst + index * bpp, row + static_cast<size_t>(lx) * bpp, bpp);
    }
  }
}

}  // namespace esphome::mipi_dsi
