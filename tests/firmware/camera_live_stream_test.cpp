// Live camera view: MJPEG frame extraction, PPA scaling plan, URL checks and
// when the expanded view may stream.
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "camera_refresh_policy.h"
#include "../artwork_image/mjpeg_parser.h"
#include "../artwork_image/p4_scaling.h"

using esphome::artwork_image::MjpegParser;
using esphome::artwork_image::p4::StreamScalePlan;
using esphome::artwork_image::p4::stream_scale_plan;

namespace {

using Bytes = std::vector<uint8_t>;

void append(Bytes &out, const Bytes &data) { out.insert(out.end(), data.begin(), data.end()); }
void append(Bytes &out, const std::string &text) { out.insert(out.end(), text.begin(), text.end()); }

Bytes segment(uint8_t marker, const Bytes &payload) {
  const size_t length = payload.size() + 2;
  Bytes out{0xFF, marker, static_cast<uint8_t>(length >> 8), static_cast<uint8_t>(length & 0xFF)};
  append(out, payload);
  return out;
}

// A structurally valid JPEG: an APP1 segment holding an embedded thumbnail
// (with its own end marker), a scan with stuffed bytes and a restart marker.
Bytes jpeg(uint8_t fill) {
  Bytes out{0xFF, 0xD8};
  Bytes thumbnail{'E', 'x', 'i', 'f', 0, 0, 0xFF, 0xD8, 0x11, 0xFF, 0xD9};
  append(out, segment(0xE1, thumbnail));
  append(out, segment(0xDB, Bytes(65, fill)));
  append(out, segment(0xDA, Bytes{1, 1, 0, 0, 63, 0}));
  append(out, Bytes{fill, 0xFF, 0x00, fill, 0xFF, 0xD3, fill, fill, 0xFF, 0xFF, 0xD9});
  return out;
}

struct Collector {
  std::vector<Bytes> frames;
  void operator()(const uint8_t *data, size_t size) { frames.emplace_back(data, data + size); }
};

Bytes multipart(const std::vector<Bytes> &images, bool content_length) {
  Bytes out;
  for (const Bytes &image : images) {
    append(out, std::string("--frame\r\nContent-Type: image/jpeg\r\n"));
    if (content_length) append(out, "Content-Length: " + std::to_string(image.size()) + "\r\n");
    append(out, std::string("\r\n"));
    append(out, image);
    append(out, std::string("\r\n"));
  }
  return out;
}

void test_parser_whole_and_split() {
  const std::vector<Bytes> images{jpeg(0x11), jpeg(0x22), jpeg(0x33)};
  for (bool content_length : {true, false}) {
    const Bytes body = multipart(images, content_length);
    for (size_t chunk : {size_t(1), size_t(3), size_t(7), body.size()}) {
      uint8_t buffer[512];
      MjpegParser parser;
      parser.reset(buffer, sizeof(buffer));
      Collector frames;
      for (size_t i = 0; i < body.size(); i += chunk) {
        parser.feed(body.data() + i, std::min(chunk, body.size() - i), frames);
      }
      assert(frames.frames.size() == images.size());
      for (size_t i = 0; i < images.size(); ++i) assert(frames.frames[i] == images[i]);
      assert(parser.dropped() == 0);
    }
  }
}

void test_parser_drops_oversized_and_recovers() {
  Bytes big{0xFF, 0xD8};
  append(big, segment(0xE0, Bytes(300, 0x44)));
  append(big, Bytes{0xFF, 0xD9});
  const Bytes small = jpeg(0x55);
  Bytes body = multipart({big, small}, true);
  uint8_t buffer[200];
  MjpegParser parser;
  parser.reset(buffer, sizeof(buffer));
  Collector frames;
  parser.feed(body.data(), body.size(), frames);
  assert(frames.frames.size() == 1 && frames.frames[0] == small);
  assert(parser.dropped() == 1);
}

void test_parser_ignores_noise_and_truncation() {
  // A frame cut off by a reconnect is discarded once the parser is reset.
  const Bytes image = jpeg(0x66);
  uint8_t buffer[512];
  MjpegParser parser;
  parser.reset(buffer, sizeof(buffer));
  Collector frames;
  const Bytes noise{0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x12};
  parser.feed(noise.data(), noise.size(), frames);
  parser.feed(image.data(), image.size() / 2, frames);
  parser.reset(buffer, sizeof(buffer));
  parser.feed(image.data(), image.size(), frames);
  assert(frames.frames.size() == 1 && frames.frames[0] == image);
}

void test_scale_plan() {
  StreamScalePlan plan;
  // 1280x720 stream filling a 1280x656 popup: no scaling, top and bottom trimmed.
  assert(stream_scale_plan(1280, 720, 1280, 656, true, plan));
  assert(plan.steps == 16 && plan.out_width == 1280 && plan.out_height == 656);
  assert(plan.crop_x == 0 && plan.crop_y == 32);
  // The same stream shown whole: one PPA step down, centred bars.
  assert(stream_scale_plan(1280, 720, 1280, 656, false, plan));
  assert(plan.steps == 14 && plan.out_width == 1120 && plan.out_height == 630);
  assert(plan.out_width <= 1280 && plan.out_height <= 656);
  // A 1080p stream covering the popup scales down and never exceeds the area.
  assert(stream_scale_plan(1920, 1080, 1280, 656, true, plan));
  assert(plan.steps == 11 && plan.out_width <= 1280 && plan.out_height <= 656);
  assert(plan.crop_width % 16 == 0 && plan.crop_height % 16 == 0);
  assert(plan.crop_width * plan.steps / 16 == plan.out_width);
  // A small stream is enlarged.
  assert(stream_scale_plan(640, 360, 1280, 656, false, plan));
  assert(plan.steps == 29 && plan.out_width == 1160 && plan.out_height == 638);
  // Odd source sizes crop to whole blocks.
  assert(stream_scale_plan(1278, 718, 1280, 720, false, plan));
  assert(plan.crop_width == 1264 && plan.crop_height == 704 && plan.crop_x == 7 && plan.crop_y == 7);
  // Degenerate sizes are rejected.
  assert(!stream_scale_plan(8, 8, 1280, 720, true, plan));
  assert(!stream_scale_plan(1280, 720, 0, 720, true, plan));
}

void test_live_url() {
  using espcontrol::camera::valid_live_url;
  assert(valid_live_url("http://192.168.1.128:1984/api/stream.mjpeg?src=g6_entry_mjpeg"));
  assert(valid_live_url("http://camera.local/video"));
  assert(!valid_live_url(""));
  assert(!valid_live_url("http://"));
  assert(!valid_live_url("http:///path"));
  assert(!valid_live_url("https://192.168.1.128/stream"));
  assert(!valid_live_url("rtsp://192.168.1.128/stream"));
  assert(!valid_live_url("http://host/a b"));
  assert(!valid_live_url(" http://host/stream"));
  assert(!valid_live_url("http://" + std::string(200, 'a')));
}

void test_live_session() {
  using espcontrol::camera::LiveSession;
  using espcontrol::camera::LIVE_IDLE_LIMIT_MS;
  LiveSession session;
  assert(!session.allowed(0, false));
  const uint32_t start = UINT32_MAX - 1000;  // Wraps during the session.
  session.begin(start);
  assert(session.allowed(start + LIVE_IDLE_LIMIT_MS - 1, false));
  assert(!session.allowed(start + LIVE_IDLE_LIMIT_MS, false));
  assert(session.allowed(start + LIVE_IDLE_LIMIT_MS, true));  // A sensor still on keeps it live.
  session.activity(start + LIVE_IDLE_LIMIT_MS + 5000);
  assert(session.allowed(start + 2 * LIVE_IDLE_LIMIT_MS, false));
  session.close();
  assert(!session.allowed(start, true));
  session.activity(start);
  assert(!session.allowed(start, false));  // Activity does not reopen a closed view.

  using espcontrol::camera::live_frame_fresh;
  assert(!live_frame_fresh(1000, 0));
  assert(live_frame_fresh(5000, 3000));
  assert(!live_frame_fresh(6000, 3000));
}

}  // namespace

int main() {
  test_parser_whole_and_split();
  test_parser_drops_oversized_and_recovers();
  test_parser_ignores_noise_and_truncation();
  test_scale_plan();
  test_live_url();
  test_live_session();
  return 0;
}
