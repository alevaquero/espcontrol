#pragma once

// Touchpad navigation for the Remote card, like the Apple TV remote: a tap
// selects, and a slide sends one direction step for each step distance the
// finger travels, so a long slide scrolls through several items. A quick
// flick shorter than one step still sends one step.
//
// Touch screens can report a brief lift at the start of a swipe. A tap is
// therefore held for a moment and dropped if the finger then slides.

#include <cstdint>
#include <cstdlib>

namespace espcontrol::remote {

enum class TouchpadAction : uint8_t { NONE, UP, DOWN, LEFT, RIGHT, SELECT };

// Taps longer than this are ignored, so resting a finger does nothing.
constexpr uint32_t TOUCHPAD_TAP_MAX_MS = 600;
// How long a tap waits for a slide that would cancel it.
constexpr uint32_t TOUCHPAD_TAP_CONFIRM_MS = 200;

struct TouchpadGesture {
  int32_t dead_zone = 20;  // Movement that still counts as a tap.
  int32_t step = 80;       // Travel per direction step.
  int32_t anchor_x = 0;
  int32_t anchor_y = 0;
  int32_t start_x = 0;
  int32_t start_y = 0;
  uint32_t pressed_at = 0;
  uint32_t tap_at = 0;
  uint16_t steps = 0;
  bool active = false;
  bool moved = false;
  bool tap_pending = false;

  // Sizes follow the pad so the feel is the same on every screen.
  void configure(int32_t pad_size) {
    dead_zone = pad_size / 20 > 12 ? pad_size / 20 : 12;
    step = pad_size / 5 > 40 ? pad_size / 5 : 40;
  }
  void press(int32_t x, int32_t y, uint32_t now) {
    active = true;
    moved = false;
    steps = 0;
    start_x = anchor_x = x;
    start_y = anchor_y = y;
    pressed_at = now;
  }
  // Returns a direction each time the finger has travelled one more step.
  TouchpadAction move(int32_t x, int32_t y) {
    if (!active) return TouchpadAction::NONE;
    if (!moved && (std::abs(x - start_x) > dead_zone || std::abs(y - start_y) > dead_zone)) {
      moved = true;
      tap_pending = false;  // The earlier "tap" was the start of this swipe.
    }
    const int32_t dx = x - anchor_x;
    const int32_t dy = y - anchor_y;
    if (std::abs(dx) < step && std::abs(dy) < step) return TouchpadAction::NONE;
    // Move the anchor one step along the dominant axis; the other axis
    // restarts so a change of direction is picked up promptly.
    if (std::abs(dx) >= std::abs(dy)) {
      anchor_x += dx > 0 ? step : -step;
      anchor_y = y;
    } else {
      anchor_y += dy > 0 ? step : -step;
      anchor_x = x;
    }
    ++steps;
    return direction(dx, dy);
  }
  TouchpadAction release(int32_t x, int32_t y, uint32_t now) {
    if (!active) return TouchpadAction::NONE;
    active = false;
    if (!moved) {
      // Taps in quick succession merge, so a bounced touch selects once.
      // A long rest is not a tap and also cancels a held one.
      tap_pending = static_cast<uint32_t>(now - pressed_at) <= TOUCHPAD_TAP_MAX_MS;
      tap_at = now;
      return TouchpadAction::NONE;
    }
    if (steps != 0) return TouchpadAction::NONE;
    return direction(x - start_x, y - start_y);  // A flick shorter than one step.
  }
  void cancel() { active = false; }
  // Call regularly: sends a held tap once no slide has cancelled it.
  TouchpadAction poll(uint32_t now) {
    if (!tap_pending || active || static_cast<uint32_t>(now - tap_at) < TOUCHPAD_TAP_CONFIRM_MS)
      return TouchpadAction::NONE;
    tap_pending = false;
    return TouchpadAction::SELECT;
  }

  static TouchpadAction direction(int32_t dx, int32_t dy) {
    if (std::abs(dx) >= std::abs(dy)) return dx > 0 ? TouchpadAction::RIGHT : TouchpadAction::LEFT;
    return dy > 0 ? TouchpadAction::DOWN : TouchpadAction::UP;
  }
};

// The remote command option each action sends.
inline const char *touchpad_action_option(TouchpadAction action) {
  switch (action) {
    case TouchpadAction::UP: return "up";
    case TouchpadAction::DOWN: return "dn";
    case TouchpadAction::LEFT: return "lt";
    case TouchpadAction::RIGHT: return "rt";
    case TouchpadAction::SELECT: return "ok";
    case TouchpadAction::NONE: break;
  }
  return nullptr;
}

}  // namespace espcontrol::remote
