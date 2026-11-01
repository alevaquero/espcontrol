#pragma once

#include "card_modal_target.h"

// Lifecycle driver for Remote cards. The tile follows the remote entity's
// on/off state; pressing it opens the remote modal.
// Contract coverage marker: "remote".

namespace espcontrol::cards {

inline bool remote_control_driver_matches(const Context &context) {
  return context.runtime.driver == card_runtime::CardDriverId::REMOTE;
}

inline bool remote_control_driver_setup_visual(
    BtnSlot &slot, const ParsedCfg &config, const Context &context) {
  if (!remote_control_driver_matches(context)) return false;
  setup_toggle_visual(slot, config);
  if (slot.icon_lbl && config.entity.empty() &&
      (config.icon.empty() || config.icon == "Auto")) {
    lv_label_set_display_text(slot.icon_lbl, find_icon("Remote TV"));
  }
  return true;
}

inline bool remote_control_driver_cleanup(BtnSlot &, const ParsedCfg &, const Context &) {
  return false;
}

inline void remote_control_driver_bind_state(BtnSlot &slot, const ParsedCfg &config) {
  if (config.entity.empty()) return;
  if (config.label.empty()) subscribe_friendly_name(slot.text_lbl, config.entity);
  lv_obj_t *btn = slot.btn;
  ha_subscribe_state(config.entity, [btn](esphome::StringRef state) {
    set_card_checked_state(btn, is_entity_on_ref(state));
  });
}

inline bool remote_control_driver_bind_main(
    BtnSlot &slot, const ParsedCfg &config, const Context &context) {
  if (!remote_control_driver_matches(context)) return false;
  remote_control_driver_bind_state(slot, config);
  return true;
}

inline bool remote_control_driver_bind_subpage(
    BtnSlot &slot, const ParsedCfg &config, const Context &context) {
  if (!remote_control_driver_matches(context)) return false;
  remote_control_driver_bind_state(slot, config);
  ParsedCfg *stored = grid_delete_with_owner(slot.btn, new ParsedCfg(config));
  lv_obj_add_event_cb(slot.btn, [](lv_event_t *event) {
    ParsedCfg *card = static_cast<ParsedCfg *>(lv_event_get_user_data(event));
    if (card) remote_open_modal(*card, static_cast<lv_obj_t *>(lv_event_get_target(event)));
  }, LV_EVENT_CLICKED, stored);
  return true;
}

inline bool remote_control_driver_handle_main_click(
    const Context &context, const ParsedCfg &config, lv_obj_t *button) {
  if (!remote_control_driver_matches(context)) return false;
  remote_open_modal(config, button);
  return true;
}

inline ModalTarget remote_control_driver_modal_target(
    const Context &context, const ParsedCfg &config, lv_obj_t *button) {
  if (!remote_control_driver_matches(context) || config.entity.empty()) return {};
  ModalTarget target;
  target.entity = config.entity;
  target.kind = ControlModalKind::REMOTE_CONTROL;
  target.available = button && remote_card_has_commands(config);
  target.open = [config, button]() { remote_open_modal(config, button); };
  return target;
}

}  // namespace espcontrol::cards
