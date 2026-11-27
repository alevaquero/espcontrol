#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace esphome {
namespace artwork_image {

// Pulls complete JPEG images out of a multipart MJPEG body. It follows the
// JPEG marker structure rather than the part headers, so boundaries, missing
// Content-Length headers and embedded EXIF thumbnails never split a frame.
class MjpegParser {
 public:
  void reset(uint8_t *buffer, size_t capacity) {
    this->buffer_ = buffer;
    this->capacity_ = capacity;
    this->size_ = 0;
    this->state_ = State::SEEK;
  }
  size_t dropped() const { return this->dropped_; }

  // Calls on_frame(data, size) for each complete image. The data stays valid
  // only during the call.
  template<typename F> void feed(const uint8_t *data, size_t length, F &&on_frame) {
    size_t i = 0;
    while (i < length) {
      switch (this->state_) {
        case State::SEEK: {
          const void *found = memchr(data + i, 0xFF, length - i);
          if (!found) return;
          i = static_cast<const uint8_t *>(found) - data + 1;
          this->state_ = State::SEEK_SOI;
          break;
        }
        case State::SEEK_SOI: {
          const uint8_t byte = data[i++];
          if (byte == 0xD8) {
            this->size_ = 0;
            this->append_(0xFF);
            this->append_(0xD8);
            this->state_ = State::MARKER_PREFIX;
          } else if (byte != 0xFF) {
            this->state_ = State::SEEK;
          }
          break;
        }
        case State::MARKER_PREFIX: {
          const uint8_t byte = data[i++];
          this->state_ = byte == 0xFF ? State::MARKER : State::SEEK;
          if (byte != 0xFF) ++this->dropped_;
          else this->append_(byte);
          break;
        }
        case State::MARKER:
        case State::ENTROPY_MARKER: {
          const uint8_t byte = data[i++];
          const bool entropy = this->state_ == State::ENTROPY_MARKER;
          this->append_(byte);
          if (this->state_ == State::SEEK || byte == 0xFF) break;  // Dropped, or a fill byte.
          if (byte == 0xD9) {
            on_frame(static_cast<const uint8_t *>(this->buffer_), this->size_);
            this->state_ = State::SEEK;
          } else if (entropy && (byte == 0x00 || (byte >= 0xD0 && byte <= 0xD7))) {
            this->state_ = State::ENTROPY;  // Stuffed byte or restart marker.
          } else if (byte == 0x01 || (byte >= 0xD0 && byte <= 0xD7)) {
            this->state_ = State::MARKER_PREFIX;  // Markers without a length.
          } else if (byte == 0x00 || byte == 0xD8) {
            this->drop_();
          } else {
            this->marker_ = byte;
            this->state_ = State::LENGTH_HIGH;
          }
          break;
        }
        case State::LENGTH_HIGH:
          this->remaining_ = static_cast<size_t>(data[i]) << 8;
          this->state_ = State::LENGTH_LOW;
          this->append_(data[i++]);
          break;
        case State::LENGTH_LOW:
          this->remaining_ |= data[i];
          this->append_(data[i++]);
          if (this->state_ == State::SEEK) break;
          if (this->remaining_ < 2) {
            this->drop_();
            break;
          }
          this->remaining_ -= 2;
          this->state_ = State::SEGMENT;
          if (this->remaining_ == 0) this->end_segment_();
          break;
        case State::SEGMENT: {
          const size_t count = this->remaining_ < length - i ? this->remaining_ : length - i;
          this->append_(data + i, count);
          i += count;
          this->remaining_ -= count;
          if (this->state_ == State::SEGMENT && this->remaining_ == 0) this->end_segment_();
          break;
        }
        case State::ENTROPY: {
          const void *found = memchr(data + i, 0xFF, length - i);
          const size_t end = found ? static_cast<const uint8_t *>(found) - data + 1 : length;
          this->append_(data + i, end - i);
          i = end;
          if (found && this->state_ == State::ENTROPY) this->state_ = State::ENTROPY_MARKER;
          break;
        }
      }
    }
  }

 private:
  enum class State : uint8_t {
    SEEK,
    SEEK_SOI,
    MARKER_PREFIX,
    MARKER,
    LENGTH_HIGH,
    LENGTH_LOW,
    SEGMENT,
    ENTROPY,
    ENTROPY_MARKER,
  };

  void append_(uint8_t byte) { this->append_(&byte, 1); }
  void append_(const uint8_t *data, size_t count) {
    if (this->state_ == State::SEEK) return;
    if (count > this->capacity_ - this->size_) {
      this->drop_();
      return;
    }
    memcpy(this->buffer_ + this->size_, data, count);
    this->size_ += count;
  }
  void drop_() {
    ++this->dropped_;
    this->size_ = 0;
    this->state_ = State::SEEK;
  }
  // After the start-of-scan header comes entropy-coded data; other segments
  // are followed by the next marker.
  void end_segment_() { this->state_ = this->marker_ == 0xDA ? State::ENTROPY : State::MARKER_PREFIX; }

  uint8_t *buffer_{nullptr};
  size_t capacity_{0};
  size_t size_{0};
  size_t remaining_{0};
  size_t dropped_{0};
  uint8_t marker_{0};
  State state_{State::SEEK};
};

}  // namespace artwork_image
}  // namespace esphome
