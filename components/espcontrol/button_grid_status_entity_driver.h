#pragma once

// Shared lifecycle driver for the "door_window" and "presence" runtime types.
// Both cards use the same visual, binding, interaction, layout, and cleanup
// stages; only their state interpretation, icons, and default label differ.

#include <memory>

namespace espcontrol::cards {

inline bool status_entity_driver_matches(const Context &context) {
  return context.runtime.driver == card_runtime::CardDriverId::STATUS_ENTITY;
}

inline bool status_entity_driver_state_active(
    card_runtime::CardTypeId type, esphome::StringRef state) {
  using Type = card_runtime::CardTypeId;
  switch (type) {
    case Type::DOOR_WINDOW: return is_entity_on_ref(state);
    case Type::PRESENCE: return presence_detected_ref(state);
    default: return false;
  }
}

inline const char *status_entity_driver_inactive_icon(
    const ParsedCfg &config, const Context &context) {
  using Type = card_runtime::CardTypeId;
  switch (context.runtime.type) {
    case Type::DOOR_WINDOW: return door_window_closed_icon(config);
    case Type::PRESENCE: return presence_clear_icon(config);
    default: return find_icon("Auto");
  }
}

inline const char *status_entity_driver_active_icon(
    const ParsedCfg &config, const Context &context) {
  using Type = card_runtime::CardTypeId;
  switch (context.runtime.type) {
    case Type::DOOR_WINDOW: return door_window_open_icon(config);
    case Type::PRESENCE: return presence_detected_icon(config);
    default: return find_icon("Auto");
  }
}

inline std::string status_entity_driver_default_label(
    const ParsedCfg &config, const Context &context) {
  using Type = card_runtime::CardTypeId;
  switch (context.runtime.type) {
    case Type::DOOR_WINDOW:
      return normalize_door_window_subtype(config.precision) == "window"
        ? espcontrol_i18n(std::string("Window"))
        : espcontrol_i18n(std::string("Door"));
    case Type::PRESENCE:
      return espcontrol_i18n(std::string("Presence"));
    default:
      return "";
  }
}

inline bool status_entity_driver_active_color_enabled(
    const ParsedCfg &config, const Context &context) {
  using Type = card_runtime::CardTypeId;
  switch (context.runtime.type) {
    case Type::DOOR_WINDOW: return door_window_active_color_enabled(config);
    case Type::PRESENCE: return presence_active_color_enabled(config);
    default: return false;
  }
}

// ---- Doors & Windows second sensor ------------------------------------------
// With "sides" set to "right" or "both", the card splits into two halves. The
// left half keeps the card's own icon and label (blank when only the right
// side is shown); the right half mirrors them for the second sensor. Each half lights independently through a clipped background layer
// that sits behind the card content. All objects here are dynamic children of
// the card and are deleted when the slot is reset.

struct DoorWindowSplitUi {
  lv_obj_t *layer = nullptr;   // Full-card, corner-clipped background layer.
  lv_obj_t *left = nullptr;
  lv_obj_t *right = nullptr;
  lv_obj_t *icon2 = nullptr;
  lv_obj_t *label2 = nullptr;
};

inline bool door_window_has_second_sensor(const ParsedCfg &config, const Context &context) {
  return context.runtime.type == card_runtime::CardTypeId::DOOR_WINDOW &&
         normalize_door_window_sides(config.options) != "left" &&
         !cfg_option_value(config.options, "sensor2").empty();
}

inline bool door_window_shows_left(const ParsedCfg &config) {
  return normalize_door_window_sides(config.options) != "right" && !config.sensor.empty();
}

// A right-only card has no sensor of its own but is still configured.
inline bool status_entity_driver_has_sensor(const ParsedCfg &config, const Context &context) {
  return !config.sensor.empty() || door_window_has_second_sensor(config, context);
}

inline const char *door_window_second_icon(const ParsedCfg &config, bool open) {
  const std::string icon = cfg_option_value(config.options, open ? "icon_on2" : "icon2");
  if (!icon.empty() && icon != "Auto") return find_icon(icon.c_str());
  const std::string subtype = normalize_door_window_subtype(cfg_option_value(config.options, "subtype2"));
  return find_icon(open ? door_window_open_icon_name(subtype) : door_window_closed_icon_name(subtype));
}

inline std::string door_window_second_subtype(const ParsedCfg &config) {
  return normalize_door_window_subtype(cfg_option_value(config.options, "subtype2"));
}

// The layer is stored on the hidden sensor container, whose user data the
// slot reset clears; the halves and right-side labels are its children.
inline DoorWindowSplitUi door_window_split_ui(const BtnSlot &slot) {
  DoorWindowSplitUi ui;
  ui.layer = slot.sensor_container
    ? static_cast<lv_obj_t *>(lv_obj_get_user_data(slot.sensor_container)) : nullptr;
  if (!ui.layer || !lv_obj_is_valid(ui.layer) || lv_obj_get_child_cnt(ui.layer) < 4) {
    return DoorWindowSplitUi();
  }
  ui.left = lv_obj_get_child(ui.layer, 0);
  ui.right = lv_obj_get_child(ui.layer, 1);
  ui.icon2 = lv_obj_get_child(ui.layer, 2);
  ui.label2 = lv_obj_get_child(ui.layer, 3);
  return ui;
}

inline lv_obj_t *door_window_split_plain_obj(lv_obj_t *parent) {
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
  return obj;
}

inline void door_window_split_create(BtnSlot &slot) {
  if (!slot.btn || !slot.sensor_container) return;
  lv_obj_t *layer = lv_obj_create(slot.btn);
  lv_obj_remove_style_all(layer);
  lv_obj_clear_flag(layer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(layer, LV_OBJ_FLAG_IGNORE_LAYOUT);
  lv_obj_set_style_clip_corner(layer, true, LV_PART_MAIN);
  door_window_split_plain_obj(layer);
  door_window_split_plain_obj(layer);
  lv_obj_t *icon2 = lv_label_create(layer);
  lv_obj_set_style_text_font(icon2, lv_obj_get_style_text_font(slot.icon_lbl, LV_PART_MAIN), LV_PART_MAIN);
  lv_obj_t *label2 = lv_label_create(layer);
  lv_obj_set_style_text_font(label2, lv_obj_get_style_text_font(slot.text_lbl, LV_PART_MAIN), LV_PART_MAIN);
  lv_obj_set_style_text_align(label2, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
  lv_label_set_long_mode(label2, LV_LABEL_LONG_WRAP);
  lv_obj_move_background(layer);
  lv_obj_set_user_data(slot.sensor_container, layer);
}

// Size the layer to the whole card (including its padding), split it down the
// middle, and keep both labels inside their own half.
inline void door_window_split_layout(BtnSlot &slot) {
  DoorWindowSplitUi ui = door_window_split_ui(slot);
  if (!ui.layer) return;
  lv_obj_update_layout(slot.btn);
  const lv_coord_t width = lv_obj_get_width(slot.btn);
  const lv_coord_t height = lv_obj_get_height(slot.btn);
  if (width <= 0 || height <= 0) return;
  const lv_coord_t pad_left = lv_obj_get_style_pad_left(slot.btn, LV_PART_MAIN);
  const lv_coord_t pad_right = lv_obj_get_style_pad_right(slot.btn, LV_PART_MAIN);
  const lv_coord_t pad_top = lv_obj_get_style_pad_top(slot.btn, LV_PART_MAIN);
  const lv_coord_t pad_bottom = lv_obj_get_style_pad_bottom(slot.btn, LV_PART_MAIN);
  lv_obj_set_pos(ui.layer, -pad_left, -pad_top);
  lv_obj_set_size(ui.layer, width, height);
  lv_obj_set_style_radius(ui.layer, lv_obj_get_style_radius(slot.btn, LV_PART_MAIN), LV_PART_MAIN);
  const lv_coord_t half = width / 2;
  lv_obj_set_pos(ui.left, 0, 0);
  lv_obj_set_size(ui.left, half, height);
  lv_obj_set_pos(ui.right, half, 0);
  lv_obj_set_size(ui.right, width - half, height);

  const lv_coord_t gap = 6;
  const lv_coord_t content_w = width - pad_left - pad_right;
  const lv_coord_t label_w = std::max<lv_coord_t>(1, (content_w - gap) / 2);
  if (slot.text_lbl) {
    lv_obj_set_style_pad_right(slot.text_lbl, content_w - label_w, LV_PART_MAIN);
  }
  // Match the display's width compensation applied to the left-side labels.
  lv_obj_set_style_transform_scale_x(ui.icon2,
    lv_obj_get_style_transform_scale_x(slot.icon_lbl, LV_PART_MAIN), LV_PART_MAIN);
  lv_obj_set_style_transform_scale_y(ui.icon2,
    lv_obj_get_style_transform_scale_y(slot.icon_lbl, LV_PART_MAIN), LV_PART_MAIN);
  lv_obj_set_style_transform_scale_x(ui.label2,
    lv_obj_get_style_transform_scale_x(slot.text_lbl, LV_PART_MAIN), LV_PART_MAIN);
  lv_obj_set_style_transform_scale_y(ui.label2,
    lv_obj_get_style_transform_scale_y(slot.text_lbl, LV_PART_MAIN), LV_PART_MAIN);
  lv_obj_align(ui.icon2, LV_ALIGN_TOP_RIGHT, -pad_right, pad_top);
  lv_obj_set_width(ui.label2, label_w);
  lv_obj_align(ui.label2, LV_ALIGN_BOTTOM_RIGHT, -pad_right, -pad_bottom);
}

inline uint32_t door_window_inactive_color(const CardPalette &palette) {
  return palette.custom_background ? palette.surface_sensor_val : current_grid_sensor_surface_color();
}

// Colour one half; the left half recolours the card text, the right half its
// own labels. A half that is not lit keeps the card's normal sensor colour.
inline void door_window_paint_half(lv_obj_t *half, lv_obj_t *text_owner, lv_obj_t *second_text,
                                   bool lit, uint32_t on_color, uint32_t inactive_color,
                                   bool custom_background) {
  if (!half || !lv_obj_is_valid(half)) return;
  const uint32_t color = lit ? on_color : inactive_color;
  lv_obj_set_style_bg_color(half, lv_color_hex(color), LV_PART_MAIN);
  const uint32_t text_color = lit || custom_background
    ? display_text_color_for_bg(color) : current_theme().text_primary;
  if (text_owner) {
    lv_obj_set_style_text_color(text_owner, lv_color_hex(text_color), LV_PART_MAIN);
    sync_card_checked_text_color(text_owner);
  }
  if (second_text && lv_obj_is_valid(second_text)) {
    lv_obj_set_style_text_color(second_text, lv_color_hex(text_color), LV_PART_MAIN);
  }
}

inline bool status_entity_driver_setup_visual(
    BtnSlot &slot, const ParsedCfg &config, const Context &context,
    const CardPalette &palette) {
  if (!status_entity_driver_matches(context)) return false;
  if (palette.has_sensor_color) {
    lv_obj_set_style_bg_color(
      slot.btn, lv_color_hex(palette.surface_sensor_val),
      static_cast<lv_style_selector_t>(LV_PART_MAIN) |
        static_cast<lv_style_selector_t>(LV_STATE_DEFAULT));
  }
  if (!status_entity_driver_has_sensor(config, context)) return true;
  lv_obj_clear_flag(slot.icon_lbl, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(slot.sensor_container, LV_OBJ_FLAG_HIDDEN);
  lv_label_set_display_text(slot.icon_lbl, find_icon("Minus"));
  set_card_disabled_state(slot.btn, true);
  const std::string label = config.label.empty()
    ? status_entity_driver_default_label(config, context)
    : config.label;
  lv_label_set_display_text(slot.text_lbl, label.c_str());
  if (door_window_has_second_sensor(config, context)) {
    if (!door_window_shows_left(config)) {
      lv_label_set_display_text(slot.icon_lbl, "");
      lv_label_set_display_text(slot.text_lbl, "");
    }
    door_window_split_create(slot);
    DoorWindowSplitUi ui = door_window_split_ui(slot);
    if (ui.layer) {
      lv_label_set_display_text(ui.icon2, find_icon("Minus"));
      const std::string label2 = cfg_option_value(config.options, "label2");
      lv_label_set_display_text(ui.label2, label2.empty()
        ? espcontrol_i18n(std::string(door_window_second_subtype(config) == "window" ? "Window" : "Door")).c_str()
        : label2.c_str());
      const uint32_t inactive = door_window_inactive_color(palette);
      door_window_paint_half(ui.left, nullptr, nullptr, false, palette.on_val, inactive, palette.custom_background);
      door_window_paint_half(ui.right, nullptr, ui.label2, false, palette.on_val, inactive, palette.custom_background);
      door_window_paint_half(ui.right, nullptr, ui.icon2, false, palette.on_val, inactive, palette.custom_background);
    }
  }
  return true;
}

inline bool status_entity_driver_attach_interaction(
    BtnSlot &slot, const ParsedCfg &config, const Context &context) {
  if (!status_entity_driver_matches(context)) return false;
  if (!status_entity_driver_has_sensor(config, context)) return true;
  lv_obj_clear_flag(slot.btn, LV_OBJ_FLAG_CLICKABLE);
  return true;
}

inline bool status_entity_driver_refresh_layout(
    BtnSlot &slot, const ParsedCfg &config, const Context &context, int, int) {
  if (!status_entity_driver_matches(context)) return false;
  // Single-sensor cards use the standard card layout.
  if (door_window_has_second_sensor(config, context)) door_window_split_layout(slot);
  return true;
}

inline bool status_entity_driver_cleanup(
    BtnSlot &, const ParsedCfg &, const Context &context) {
  // This pilot owns no dynamic allocations. The explicit cleanup stage keeps
  // lifecycle ownership consistent with drivers that will need it later.
  return status_entity_driver_matches(context);
}

// Two independent door/window sensors. The card shows as unavailable only when
// both sensors are; each half shows the Minus icon while its sensor is.
inline void status_entity_driver_bind_split(
    BtnSlot &slot, const ParsedCfg &config, const CardPalette &palette) {
  DoorWindowSplitUi ui = door_window_split_ui(slot);
  if (!ui.layer) return;
  struct SplitState {
    lv_obj_t *btn, *icon, *left, *right, *icon2, *label2;
    const char *closed[2], *open[2];
    bool active_color[2];
    bool show_left;
    bool active[2] = {false, false};
    bool unavailable[2] = {true, true};
    uint32_t on_color, inactive_color;
    bool custom_background;
    uint32_t generation;
    bool valid() const {
      return generation == ha_subscription_generation() && lv_obj_is_valid(left) &&
             lv_obj_is_valid(right) && lv_obj_is_valid(icon2) && lv_obj_is_valid(label2);
    }
    void render() const {
      if (!valid()) return;
      set_card_disabled_state(btn, show_left ? unavailable[0] && unavailable[1] : unavailable[1]);
      if (show_left) {
        lv_label_set_display_text(icon, unavailable[0]
          ? find_icon("Minus") : (active[0] ? open[0] : closed[0]));
      }
      lv_label_set_display_text(icon2, unavailable[1]
        ? find_icon("Minus") : (active[1] ? open[1] : closed[1]));
      theme_set_content_background(btn, custom_background);
      door_window_paint_half(left, btn, nullptr, show_left && active_color[0] && active[0],
                             on_color, inactive_color, custom_background);
      door_window_paint_half(right, nullptr, label2, active_color[1] && active[1],
                             on_color, inactive_color, custom_background);
      door_window_paint_half(right, nullptr, icon2, active_color[1] && active[1],
                             on_color, inactive_color, custom_background);
    }
  };
  auto state = std::make_shared<SplitState>();
  state->btn = slot.btn;
  state->icon = slot.icon_lbl;
  state->left = ui.left;
  state->right = ui.right;
  state->icon2 = ui.icon2;
  state->label2 = ui.label2;
  state->show_left = door_window_shows_left(config);
  state->closed[0] = door_window_closed_icon(config);
  state->open[0] = door_window_open_icon(config);
  state->closed[1] = door_window_second_icon(config, false);
  state->open[1] = door_window_second_icon(config, true);
  state->active_color[0] = door_window_active_color_enabled(config);
  state->active_color[1] = cfg_option_token_present(config.options, "active_color2");
  state->on_color = palette.on_val;
  state->inactive_color = door_window_inactive_color(palette);
  state->custom_background = palette.custom_background;
  state->generation = ha_subscription_generation();
  state->render();

  const std::string sensor2 = cfg_option_value(config.options, "sensor2");
  for (int index = state->show_left ? 0 : 1; index < 2; ++index) {
    ha_subscribe_state(
      index == 0 ? config.sensor : sensor2,
      std::function<void(esphome::StringRef)>([state, index](esphome::StringRef value) {
        state->unavailable[index] = ha_state_unavailable_ref(value);
        state->active[index] = !state->unavailable[index] && is_entity_on_ref(value);
        state->render();
      }));
  }
  if (state->show_left && config.label.empty()) subscribe_friendly_name(slot.text_lbl, config.sensor);
  if (cfg_option_value(config.options, "label2").empty()) {
    ha_subscribe_attribute(
      sensor2, std::string("friendly_name"),
      std::function<void(esphome::StringRef)>([state](esphome::StringRef name) {
        if (!state->valid()) return;
        lv_label_set_text_limited(state->label2, name, HA_FRIENDLY_NAME_MAX_LEN);
      }));
  }
}

inline bool status_entity_driver_bind_data(
    BtnSlot &slot, const ParsedCfg &config, const Context &context,
    const CardPalette &palette) {
  if (!status_entity_driver_matches(context)) return false;
  if (door_window_has_second_sensor(config, context)) {
    status_entity_driver_bind_split(slot, config, palette);
    return true;
  }
  if (config.sensor.empty()) return true;

  const auto type = context.runtime.type;
  const char *inactive_icon = status_entity_driver_inactive_icon(config, context);
  const char *active_icon = status_entity_driver_active_icon(config, context);
  const bool active_color =
    status_entity_driver_active_color_enabled(config, context);
  lv_label_set_display_text(slot.icon_lbl, find_icon("Minus"));
  set_card_disabled_state(slot.btn, true);
  ha_subscribe_state(
    config.sensor,
    std::function<void(esphome::StringRef)>(
      [btn = slot.btn, icon = slot.icon_lbl, type, inactive_icon, active_icon,
       active_color, on_color = palette.on_val,
       sensor_color = palette.surface_sensor_val, custom_background = palette.custom_background](esphome::StringRef state) {
        const bool unavailable = ha_state_unavailable_ref(state);
        const bool active = !unavailable &&
          status_entity_driver_state_active(type, state);
        set_card_disabled_state(btn, unavailable);
        lv_label_set_display_text(icon, unavailable
          ? find_icon("Minus") : (active ? active_icon : inactive_icon));
        if (btn && active_color) {
          const uint32_t inactive_color = custom_background ? sensor_color : current_grid_sensor_surface_color();
          theme_set_content_background(btn, active || custom_background);
          lv_obj_set_style_bg_color(
            btn, lv_color_hex(active ? on_color : inactive_color),
            static_cast<lv_style_selector_t>(LV_PART_MAIN) |
              static_cast<lv_style_selector_t>(LV_STATE_DEFAULT));
          lv_obj_set_style_text_color(
            btn, lv_color_hex(active || custom_background
                ? display_text_color_for_bg(active ? on_color : inactive_color) : current_theme().text_primary),
            LV_PART_MAIN);
          sync_card_checked_text_color(btn);
        }
      })
  );
  if (config.label.empty()) {
    subscribe_friendly_name(slot.text_lbl, config.sensor);
  }
  return true;
}

}  // namespace espcontrol::cards
