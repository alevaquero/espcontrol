#pragma once

#include <cstdint>
#include <string>

namespace esphome {
namespace artwork_image {

struct MjpegStreamFrame {
  const uint8_t *data{nullptr};
  int width{0};
  int height{0};
};

// One live MJPEG stream for the expanded camera view (ESP32-P4 only). A
// background task reads the stream, decodes each frame with the JPEG hardware
// and scales it with the PPA into one of two RGB565 buffers.
class MjpegStream {
 public:
  static MjpegStream &instance();

  // Returns false while a previous stream is still shutting down; try again later.
  bool start(const std::string &url, int area_width, int area_height, bool cover, bool big_endian);
  // The caller must stop displaying the last frame before calling stop().
  void stop();
  bool active() const;
  // Takes the newest decoded frame. It stays valid until the next take_frame() or stop().
  bool take_frame(MjpegStreamFrame &frame);
  uint32_t last_frame_ms() const;

 private:
  MjpegStream() = default;
};

}  // namespace artwork_image
}  // namespace esphome
