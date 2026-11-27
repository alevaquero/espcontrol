#include "mjpeg_stream.h"

#if defined(USE_ESP_IDF) && defined(CONFIG_IDF_TARGET_ESP32P4)

#include <algorithm>
#include <atomic>

#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "mjpeg_parser.h"
#include "p4_jpeg_backend.h"

namespace esphome {
namespace artwork_image {

static const char *const TAG = "artwork_image.mjpeg";

static constexpr size_t FRAME_INPUT_CAPACITY = 1024 * 1024;
static constexpr size_t READ_CHUNK_BYTES = 8192;
static constexpr int HTTP_TIMEOUT_MS = 2000;
static constexpr uint32_t STALL_TIMEOUT_MS = 6000;
static constexpr uint32_t RETRY_MAX_MS = 10000;

namespace {

struct StreamState {
  SemaphoreHandle_t mutex{nullptr};
  std::atomic<bool> task_alive{false};
  std::atomic<bool> stop_requested{false};
  std::atomic<uint32_t> last_frame_ms{0};
  // Task parameters, written before the task starts.
  std::string url;
  int area_width{0};
  int area_height{0};
  bool cover{true};
  bool big_endian{false};
  // Double buffer shared with the main loop under the mutex. The task writes
  // only the buffer that is neither on screen (front) nor waiting (ready).
  uint8_t *buffers[2]{nullptr, nullptr};
  size_t buffer_capacity{0};
  int widths[2]{0, 0};
  int heights[2]{0, 0};
  int front{-1};
  int ready{-1};
};

StreamState &state() {
  static StreamState value;
  return value;
}

void lock() { xSemaphoreTake(state().mutex, portMAX_DELAY); }
void unlock() { xSemaphoreGive(state().mutex); }

bool wait_or_stop(uint32_t delay_ms) {
  StreamState &s = state();
  for (uint32_t waited = 0; waited < delay_ms; waited += 100) {
    if (s.stop_requested.load()) return false;
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  return !s.stop_requested.load();
}

struct Session {
  P4StreamDecoder decoder;
  MjpegParser parser;
  uint32_t frames{0};
  uint32_t decode_failures{0};
};

// The parser assembles each frame in the decoder input buffer, so it decodes in place.
void handle_frame(Session &session, size_t size) {
  StreamState &s = state();
  lock();
  const int target = s.ready != -1 ? -1 : (s.front == 0 ? 1 : 0);
  unlock();
  if (target < 0) return;  // The screen has not shown the previous frame yet.
  int width = 0;
  int height = 0;
  if (!p4_stream_decode_frame(session.decoder, size, s.area_width, s.area_height, s.cover, s.big_endian,
                              s.buffers[target], s.buffer_capacity, width, height)) {
    if (session.decode_failures++ % 50 == 0) ESP_LOGW(TAG, "Could not decode a live frame (%zu bytes)", size);
    return;
  }
  lock();
  s.widths[target] = width;
  s.heights[target] = height;
  s.ready = target;
  unlock();
  s.last_frame_ms.store(millis());
  if (session.frames++ == 0) ESP_LOGI(TAG, "Live stream showing %dx%d frames", width, height);
}

// Reads one connection until it fails, stalls or a stop is requested.
// Returns true when at least one frame was decoded.
bool run_connection(Session &session, char *chunk) {
  StreamState &s = state();
  esp_http_client_config_t config{};
  config.url = s.url.c_str();
  config.method = HTTP_METHOD_GET;
  config.timeout_ms = HTTP_TIMEOUT_MS;
  config.auth_type = HTTP_AUTH_TYPE_NONE;
  config.buffer_size = READ_CHUNK_BYTES;
  config.max_redirection_count = 3;
  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (!client) return false;
  const uint32_t frames_before = session.frames;
  esp_err_t error = esp_http_client_open(client, 0);
  if (error == ESP_OK && esp_http_client_fetch_headers(client) < 0) error = ESP_ERR_HTTP_FETCH_HEADER;
  const int status = error == ESP_OK ? esp_http_client_get_status_code(client) : 0;
  if (error != ESP_OK || status != 200) {
    ESP_LOGW(TAG, "Live stream connection failed (error %d, HTTP %d)", error, status);
  } else {
    session.parser.reset(session.decoder.input, session.decoder.input_capacity);
    uint32_t last_data = millis();
    while (!s.stop_requested.load()) {
      const int received = esp_http_client_read(client, chunk, READ_CHUNK_BYTES);
      const uint32_t now = millis();
      if (received > 0) {
        last_data = now;
        session.parser.feed(reinterpret_cast<const uint8_t *>(chunk), received,
                            [&session](const uint8_t *, size_t size) { handle_frame(session, size); });
        continue;
      }
      if ((received < 0 && received != -ESP_ERR_HTTP_EAGAIN) || now - last_data > STALL_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Live stream ended (%d); reconnecting", received);
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return session.frames != frames_before;
}

void free_buffers() {
  StreamState &s = state();
  lock();
  for (auto &buffer : s.buffers) {
    heap_caps_free(buffer);
    buffer = nullptr;
  }
  s.buffer_capacity = 0;
  s.front = s.ready = -1;
  unlock();
}

void stream_task(void *) {
  StreamState &s = state();
  const size_t frame_bytes = static_cast<size_t>(s.area_width) * s.area_height * 2;
  const size_t capacity = (frame_bytes + 63) & ~static_cast<size_t>(63);
  lock();
  s.front = s.ready = -1;
  s.buffer_capacity = capacity;
  for (auto &buffer : s.buffers) {
    buffer = static_cast<uint8_t *>(heap_caps_aligned_alloc(64, capacity, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM));
  }
  const bool buffers_ready = s.buffers[0] && s.buffers[1];
  unlock();
  auto *session = new Session();
  auto *chunk = static_cast<char *>(heap_caps_malloc(READ_CHUNK_BYTES, MALLOC_CAP_8BIT));
  if (!buffers_ready || !chunk || !p4_stream_decoder_init(session->decoder, FRAME_INPUT_CAPACITY)) {
    ESP_LOGW(TAG, "Not enough memory for the live stream");
  } else {
    ESP_LOGI(TAG, "Starting live stream %s", s.url.c_str());
    uint32_t failures = 0;
    while (!s.stop_requested.load()) {
      failures = run_connection(*session, chunk) ? 0 : failures + 1;
      if (s.stop_requested.load()) break;
      const uint32_t delay = failures == 0 ? 500 : std::min<uint32_t>(1000U << std::min<uint32_t>(failures - 1, 4), RETRY_MAX_MS);
      if (!wait_or_stop(delay)) break;
    }
  }
  p4_stream_decoder_release(session->decoder);
  delete session;
  heap_caps_free(chunk);
  free_buffers();
  ESP_LOGI(TAG, "Live stream stopped");
  s.task_alive.store(false);
  vTaskDelete(nullptr);
}

}  // namespace

MjpegStream &MjpegStream::instance() {
  static MjpegStream value;
  return value;
}

bool MjpegStream::start(const std::string &url, int area_width, int area_height, bool cover, bool big_endian) {
  StreamState &s = state();
  if (s.task_alive.load() || url.empty() || area_width < 16 || area_height < 16) return false;
  if (!s.mutex) s.mutex = xSemaphoreCreateMutex();
  if (!s.mutex) return false;
  s.url = url;
  s.area_width = area_width;
  s.area_height = area_height;
  s.cover = cover;
  s.big_endian = big_endian;
  s.stop_requested.store(false);
  s.last_frame_ms.store(0);
  s.task_alive.store(true);
  if (xTaskCreate(stream_task, "mjpeg_stream", 6144, nullptr, 2, nullptr) != pdPASS) {
    s.task_alive.store(false);
    ESP_LOGW(TAG, "Could not start the live stream task");
    return false;
  }
  return true;
}

void MjpegStream::stop() {
  StreamState &s = state();
  if (!s.task_alive.load()) return;
  s.stop_requested.store(true);
  lock();
  s.front = s.ready = -1;
  unlock();
}

bool MjpegStream::active() const { return state().task_alive.load() && !state().stop_requested.load(); }

bool MjpegStream::take_frame(MjpegStreamFrame &frame) {
  StreamState &s = state();
  if (!this->active()) return false;
  lock();
  const int ready = s.ready;
  if (ready >= 0) {
    s.front = ready;
    s.ready = -1;
    frame.data = s.buffers[ready];
    frame.width = s.widths[ready];
    frame.height = s.heights[ready];
  }
  unlock();
  return ready >= 0;
}

uint32_t MjpegStream::last_frame_ms() const { return state().last_frame_ms.load(); }

}  // namespace artwork_image
}  // namespace esphome

#else

namespace esphome {
namespace artwork_image {

MjpegStream &MjpegStream::instance() {
  static MjpegStream value;
  return value;
}
bool MjpegStream::start(const std::string &, int, int, bool, bool) { return false; }
void MjpegStream::stop() {}
bool MjpegStream::active() const { return false; }
bool MjpegStream::take_frame(MjpegStreamFrame &) { return false; }
uint32_t MjpegStream::last_frame_ms() const { return 0; }

}  // namespace artwork_image
}  // namespace esphome

#endif
