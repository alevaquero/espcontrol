#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace artwork_image {

struct HardwareJpegFrame {
  const uint8_t *data{nullptr};
  int width{0};
  int height{0};
  size_t stride_bytes{0};
  bool ppa_scaled{false};
};

// Returns true only when the ESP32-P4 hardware path produced a complete frame.
// Unsupported/progressive inputs and every hardware error deliberately return
// false so the portable libjpeg decoder remains the compatibility fallback.
bool try_decode_p4_jpeg(const uint8_t *data, size_t size, int target_width, int target_height,
                       bool cover_mode, bool big_endian, HardwareJpegFrame &frame);
void release_p4_jpeg_workspace();

// Live MJPEG frames own their memory, so they never share the artwork
// pipeline's workspace; hardware use is serialized with it.
struct P4StreamDecoder {
  uint8_t *input{nullptr};
  size_t input_capacity{0};
  uint8_t *output{nullptr};
  size_t output_capacity{0};
};

// Allocates the decoder input buffer that the stream fills with JPEG bytes.
bool p4_stream_decoder_init(P4StreamDecoder &decoder, size_t input_capacity);
// Decodes decoder.input[0, size) and scales it with the PPA into `out`
// (RGB565, 64-byte aligned, DMA capable) to fit inside the area.
bool p4_stream_decode_frame(P4StreamDecoder &decoder, size_t size, int area_width, int area_height,
                            bool cover, bool big_endian, uint8_t *out, size_t out_capacity,
                            int &width, int &height);
void p4_stream_decoder_release(P4StreamDecoder &decoder);

}  // namespace artwork_image
}  // namespace esphome
