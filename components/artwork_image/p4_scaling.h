#pragma once

#include <cstdint>

namespace esphome {
namespace artwork_image {
namespace p4 {

static constexpr uint32_t SCALE_FRACTION_STEPS = 16;
static constexpr uint32_t MAX_SCALE_STEPS = 16 * SCALE_FRACTION_STEPS;

// PPA stores each scale factor as an integer plus a four-bit fraction. Return
// the exact hardware step count, or zero when the requested output size would
// be rounded or is outside the supported [1/16, 16) range.
constexpr uint32_t exact_scale_steps(uint32_t input_size, uint32_t output_size) {
  if (input_size == 0 || output_size == 0) return 0;
  uint64_t numerator = static_cast<uint64_t>(output_size) * SCALE_FRACTION_STEPS;
  if (numerator % input_size != 0) return 0;
  uint64_t steps = numerator / input_size;
  return steps > 0 && steps < MAX_SCALE_STEPS ? static_cast<uint32_t>(steps) : 0;
}

struct StreamScalePlan {
  uint32_t steps{0};
  uint32_t crop_x{0};
  uint32_t crop_y{0};
  uint32_t crop_width{0};
  uint32_t crop_height{0};
  uint32_t out_width{0};
  uint32_t out_height{0};
};

// Live frames use one PPA step on both axes. Crops are whole 16-pixel blocks
// so the output size is exact, and the output never exceeds the area: cover
// trims the source to fill it, fit leaves bars around the whole picture.
inline bool stream_scale_plan(uint32_t source_width, uint32_t source_height, uint32_t area_width,
                              uint32_t area_height, bool cover, StreamScalePlan &plan) {
  plan = {};
  if (source_width < 16 || source_height < 16 || area_width < 16 || area_height < 16) return false;
  const uint32_t fit_x = area_width * SCALE_FRACTION_STEPS / source_width;
  const uint32_t fit_y = area_height * SCALE_FRACTION_STEPS / source_height;
  uint32_t steps = fit_x < fit_y ? fit_x : fit_y;
  if (cover) {
    const uint32_t fill_x = (area_width * SCALE_FRACTION_STEPS + source_width - 1) / source_width;
    const uint32_t fill_y = (area_height * SCALE_FRACTION_STEPS + source_height - 1) / source_height;
    steps = fill_x > fill_y ? fill_x : fill_y;
  }
  if (steps == 0) steps = 1;
  if (steps >= MAX_SCALE_STEPS) steps = MAX_SCALE_STEPS - 1;
  uint32_t crop_width = source_width & ~15U;
  uint32_t crop_height = source_height & ~15U;
  const uint32_t max_width = (area_width * SCALE_FRACTION_STEPS / steps) & ~15U;
  const uint32_t max_height = (area_height * SCALE_FRACTION_STEPS / steps) & ~15U;
  if (crop_width > max_width) crop_width = max_width;
  if (crop_height > max_height) crop_height = max_height;
  if (crop_width == 0 || crop_height == 0) return false;
  plan.steps = steps;
  plan.crop_width = crop_width;
  plan.crop_height = crop_height;
  plan.crop_x = (source_width - crop_width) / 2;
  plan.crop_y = (source_height - crop_height) / 2;
  plan.out_width = crop_width * steps / SCALE_FRACTION_STEPS;
  plan.out_height = crop_height * steps / SCALE_FRACTION_STEPS;
  return true;
}

}  // namespace p4
}  // namespace artwork_image
}  // namespace esphome
