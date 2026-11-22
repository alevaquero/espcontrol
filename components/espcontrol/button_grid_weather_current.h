#pragma once

#include <memory>

// Optional current temperature on a Current Conditions weather card. Top right
// uses the sensor number font with its unit; bottom right matches the label
// font and keeps the condition text clear of the value.

inline std::string weather_current_temperature_position(const ParsedCfg &config) {
  return config.precision.empty()
    ? normalize_weather_current_temperature(cfg_option_value(config.options, "current_temperature"))
    : "";
}

inline void weather_current_temperature_layout(BtnSlot &slot, const std::string &position) {
  if (!slot.sensor_container || position.empty()) return;
  lv_obj_clear_flag(slot.sensor_container, LV_OBJ_FLAG_HIDDEN);
  if (position == "bottom_right") {
    if (slot.sensor_lbl && slot.text_lbl) {
      lv_obj_set_style_text_font(slot.sensor_lbl,
        lv_obj_get_style_text_font(slot.text_lbl, LV_PART_MAIN), LV_PART_MAIN);
    }
    if (slot.unit_lbl) lv_obj_add_flag(slot.unit_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(slot.sensor_container, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  } else {
    if (slot.unit_lbl) lv_obj_clear_flag(slot.unit_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(slot.sensor_container, LV_ALIGN_TOP_RIGHT, 0, 0);
  }
  lv_obj_move_foreground(slot.sensor_container);
}

inline void subscribe_weather_current_temperature(BtnSlot &slot, const std::string &entity,
                                                  const std::string &position) {
  struct CurrentTemperatureState {
    lv_obj_t *container, *value, *unit, *label;
    bool bottom;
    uint32_t generation;
    std::string reading, source_unit;
    void render() const {
      if (generation != ha_subscription_generation() || !lv_obj_is_valid(container) ||
          !lv_obj_is_valid(value) || !lv_obj_is_valid(unit) || !lv_obj_is_valid(label)) return;
      char *end = nullptr;
      const float parsed = std::strtof(reading.c_str(), &end);
      const bool valid = !reading.empty() && end != reading.c_str() && std::isfinite(parsed);
      std::string text = "--";
      if (valid) {
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%d", static_cast<int>(std::round(
          convert_temperature_value_for_display_float(parsed, source_unit))));
        text = buffer;
      }
      if (bottom) {
        if (valid) text += "°";
        lv_label_set_display_text(value, text.c_str());
        // Wrap the condition before it reaches the temperature.
        lv_obj_update_layout(container);
        lv_obj_set_style_pad_right(label, lv_obj_get_width(container) + 6, LV_PART_MAIN);
      } else {
        lv_label_set_display_text(value, text.c_str());
        lv_label_set_display_text(unit, valid ? display_temperature_unit_symbol() : "");
      }
      notify_dashboard_content_changed();
    }
  };
  if (entity.empty() || position.empty() || !slot.sensor_container || !slot.sensor_lbl ||
      !slot.unit_lbl || !slot.text_lbl) return;
  auto state = std::make_shared<CurrentTemperatureState>();
  state->container = slot.sensor_container;
  state->value = slot.sensor_lbl;
  state->unit = slot.unit_lbl;
  state->label = slot.text_lbl;
  state->bottom = position == "bottom_right";
  state->generation = ha_subscription_generation();
  state->render();
  ha_subscribe_attribute(entity, "temperature", [state](esphome::StringRef value) {
    state->reading = string_ref_limited(value, HA_SHORT_STATE_MAX_LEN);
    state->render();
  });
  ha_subscribe_attribute(entity, "temperature_unit", [state](esphome::StringRef value) {
    state->source_unit = string_ref_limited(value, HA_SHORT_STATE_MAX_LEN);
    state->render();
  });
}
