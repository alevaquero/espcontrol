// Remote card touchpad: taps select, slides send one step per step distance,
// short flicks send one step, and resting a finger does nothing.
#include <cassert>
#include <cstring>
#include <vector>

#include "remote_touchpad.h"

using espcontrol::remote::TOUCHPAD_TAP_CONFIRM_MS;
using espcontrol::remote::TouchpadAction;
using espcontrol::remote::TouchpadGesture;

namespace {

TouchpadGesture pad() {
  TouchpadGesture gesture;
  gesture.configure(400);  // Dead zone 20 px, step 80 px.
  assert(gesture.dead_zone == 20 && gesture.step == 80);
  return gesture;
}

// Slides from (x, y) by (dx, dy) in 10 px moves and returns every action.
std::vector<TouchpadAction> slide(TouchpadGesture &gesture, int x, int y, int dx, int dy, uint32_t duration = 300) {
  std::vector<TouchpadAction> actions;
  gesture.press(x, y, 1000);
  const int moves = std::max(std::abs(dx), std::abs(dy)) / 10;
  for (int i = 1; i <= moves; ++i) {
    TouchpadAction action = gesture.move(x + dx * i / moves, y + dy * i / moves);
    if (action != TouchpadAction::NONE) actions.push_back(action);
  }
  TouchpadAction action = gesture.release(x + dx, y + dy, 1000 + duration);
  if (action != TouchpadAction::NONE) actions.push_back(action);
  action = gesture.poll(1000 + duration + TOUCHPAD_TAP_CONFIRM_MS);
  if (action != TouchpadAction::NONE) actions.push_back(action);
  return actions;
}

}  // namespace

int main() {
  TouchpadGesture gesture = pad();

  // A tap, with a little jitter, selects once no slide follows it.
  gesture.press(200, 200, 1000);
  assert(gesture.move(210, 195) == TouchpadAction::NONE);
  assert(gesture.release(212, 196, 1200) == TouchpadAction::NONE);
  assert(gesture.poll(1200 + TOUCHPAD_TAP_CONFIRM_MS - 1) == TouchpadAction::NONE);
  assert(gesture.poll(1200 + TOUCHPAD_TAP_CONFIRM_MS) == TouchpadAction::SELECT);
  assert(gesture.poll(5000) == TouchpadAction::NONE);  // Sent once.
  // Resting a finger longer than a tap does nothing.
  gesture.press(200, 200, 1000);
  assert(gesture.release(200, 200, 1000 + espcontrol::remote::TOUCHPAD_TAP_MAX_MS + 1) == TouchpadAction::NONE);
  assert(gesture.poll(5000) == TouchpadAction::NONE);

  // A touch that bounces at the start of a swipe sends only the swipe.
  gesture.press(200, 200, 1000);
  assert(gesture.release(201, 200, 1015) == TouchpadAction::NONE);
  gesture.press(203, 200, 1030);
  assert(gesture.poll(1300) == TouchpadAction::NONE);  // Held while the finger is down.
  std::vector<TouchpadAction> bounced;
  for (int x = 213; x <= 293; x += 10) {
    TouchpadAction action = gesture.move(x, 200);
    if (action != TouchpadAction::NONE) bounced.push_back(action);
  }
  assert(gesture.release(293, 200, 1400) == TouchpadAction::NONE);
  assert(gesture.poll(2000) == TouchpadAction::NONE);
  assert((bounced == std::vector<TouchpadAction>{TouchpadAction::RIGHT}));
  // A bounced tap selects once.
  gesture.press(200, 200, 1000);
  gesture.release(200, 200, 1010);
  gesture.press(201, 200, 1020);
  gesture.release(201, 200, 1100);
  assert(gesture.poll(1100 + TOUCHPAD_TAP_CONFIRM_MS) == TouchpadAction::SELECT);
  assert(gesture.poll(3000) == TouchpadAction::NONE);
  // A bounce followed by resting the finger does nothing.
  gesture.press(200, 200, 1000);
  gesture.release(200, 200, 1010);
  gesture.press(200, 200, 1020);
  gesture.release(200, 200, 1020 + espcontrol::remote::TOUCHPAD_TAP_MAX_MS + 1);
  assert(gesture.poll(5000) == TouchpadAction::NONE);

  // A short flick sends one step in its direction.
  assert((slide(gesture, 200, 200, 50, 0) == std::vector<TouchpadAction>{TouchpadAction::RIGHT}));
  assert((slide(gesture, 200, 200, 0, -40) == std::vector<TouchpadAction>{TouchpadAction::UP}));
  // A long slide sends one step per 80 px and nothing extra on release.
  assert((slide(gesture, 50, 200, 330, 20) == std::vector<TouchpadAction>(4, TouchpadAction::RIGHT)));
  assert((slide(gesture, 200, 380, -15, -250) == std::vector<TouchpadAction>(3, TouchpadAction::UP)));
  assert((slide(gesture, 200, 20, 0, 170) == std::vector<TouchpadAction>(2, TouchpadAction::DOWN)));
  assert((slide(gesture, 350, 200, -90, 0) == std::vector<TouchpadAction>{TouchpadAction::LEFT}));

  // Changing direction mid-slide follows the finger.
  gesture.press(100, 100, 1000);
  std::vector<TouchpadAction> turns;
  for (int x = 110; x <= 260; x += 10) {
    TouchpadAction action = gesture.move(x, 100);
    if (action != TouchpadAction::NONE) turns.push_back(action);
  }
  for (int y = 110; y <= 260; y += 10) {
    TouchpadAction action = gesture.move(260, y);
    if (action != TouchpadAction::NONE) turns.push_back(action);
  }
  assert(gesture.release(260, 260, 1500) == TouchpadAction::NONE);
  assert((turns == std::vector<TouchpadAction>{TouchpadAction::RIGHT, TouchpadAction::RIGHT,
                                               TouchpadAction::DOWN, TouchpadAction::DOWN}));

  // A press that slides off and is lost sends nothing.
  gesture.press(200, 200, 1000);
  gesture.move(230, 200);
  gesture.cancel();
  assert(gesture.release(230, 200, 1100) == TouchpadAction::NONE);
  assert(gesture.move(400, 200) == TouchpadAction::NONE);

  // Small pads keep usable minimums.
  TouchpadGesture small;
  small.configure(100);
  assert(small.dead_zone == 12 && small.step == 40);

  // Each action maps to the remote card's command option.
  assert(std::strcmp(espcontrol::remote::touchpad_action_option(TouchpadAction::SELECT), "ok") == 0);
  assert(std::strcmp(espcontrol::remote::touchpad_action_option(TouchpadAction::LEFT), "lt") == 0);
  assert(std::strcmp(espcontrol::remote::touchpad_action_option(TouchpadAction::DOWN), "dn") == 0);
  assert(espcontrol::remote::touchpad_action_option(TouchpadAction::NONE) == nullptr);
  return 0;
}
