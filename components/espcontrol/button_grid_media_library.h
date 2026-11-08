#pragma once

// Playlists, Artists, and Radios tabs for the media control modal. Each tab
// lists one Music Assistant library for the card's player and plays the
// tapped entry with music_assistant.play_media.
//
// Libraries can hold thousands of entries, so the list is virtual: a spacer
// gives the scroll area its full height, and a small pool of row buttons is
// moved and relabelled as the list scrolls. Loaded entries are cached per
// media player so reopening a tab does not fetch them again.

#include <array>
#include <map>

#include "media_library.h"

// Loaded entries kept per tab and player; low-heap panels keep fewer.
#ifdef ESPCONTROL_LOW_HEAP_MEDIA_CONTROL
#define ESPCONTROL_MEDIA_LIBRARY_MAX_ITEMS 200
#else
#define ESPCONTROL_MEDIA_LIBRARY_MAX_ITEMS 1000
#endif
constexpr size_t MEDIA_LIBRARY_MAX_ITEMS = ESPCONTROL_MEDIA_LIBRARY_MAX_ITEMS;

inline espcontrol::media::LibraryKind media_library_kind_for_tab(MediaControlTab tab) {
  switch (tab) {
    case MediaControlTab::ARTISTS: return espcontrol::media::LibraryKind::ARTIST;
    case MediaControlTab::RADIOS: return espcontrol::media::LibraryKind::RADIO;
    default: return espcontrol::media::LibraryKind::PLAYLIST;
  }
}

inline MediaControlTab media_library_tab_for_kind(espcontrol::media::LibraryKind kind) {
  switch (kind) {
    case espcontrol::media::LibraryKind::ARTIST: return MediaControlTab::ARTISTS;
    case espcontrol::media::LibraryKind::RADIO: return MediaControlTab::RADIOS;
    case espcontrol::media::LibraryKind::PLAYLIST: break;
  }
  return MediaControlTab::PLAYLISTS;
}

inline espcontrol::media::LibraryList &media_library_list(
    const std::string &entity_id, espcontrol::media::LibraryKind kind) {
  static std::map<std::string, std::array<espcontrol::media::LibraryList,
                                          espcontrol::media::LIBRARY_KIND_COUNT>> lists;
  return lists[entity_id][static_cast<size_t>(kind)];
}

inline uint32_t next_media_library_call_id() {
  static uint32_t call_id = 700000;
  return call_id++;
}

inline bool media_library_tab_shows(const std::string &entity_id,
                                    espcontrol::media::LibraryKind kind) {
  const MediaControlModalUi &ui = media_control_modal_ui();
  return ui.active && ui.library_list && ui.active->entity_id == entity_id &&
         ui.tab == media_library_tab_for_kind(kind);
}

inline void media_library_render(MediaControlCtx *ctx);

inline void media_library_handle_response(const std::string &entity_id,
                                          espcontrol::media::LibraryKind kind,
                                          uint32_t call_id, size_t offset,
                                          const esphome::api::ActionResponse &response) {
  espcontrol::media::LibraryList &list = media_library_list(entity_id, kind);
  if (list.call_id != call_id) return;
  list.loading = false;
  list.call_id = 0;
  if (!response.is_success()) {
    list.failed = true;
    ESP_LOGW("media_library", "Could not load %s library for %s: %s",
             espcontrol::media::library_media_type(kind), entity_id.c_str(),
             response.get_error_message().c_str());
  } else {
    auto json = response.get_json();
    const char *payload = json["response"].as<const char *>();
    if (offset == 0) list.items.clear();
    // A page for an older position (the list was reset meanwhile) is dropped.
    if (offset == list.items.size()) {
      const size_t lines = espcontrol::media::library_parse_page(
        payload ? payload : "", list.items, MEDIA_LIBRARY_MAX_ITEMS);
      list.complete = espcontrol::media::library_page_complete(
        lines, list.items.size(), MEDIA_LIBRARY_MAX_ITEMS);
      list.failed = false;
      list.loaded_ms = esphome::millis();
      ESP_LOGI("media_library", "Loaded %u %s entries for %s (total %u%s)",
               (unsigned) lines, espcontrol::media::library_media_type(kind),
               entity_id.c_str(), (unsigned) list.items.size(),
               list.complete ? ", complete" : "");
    }
  }
  if (media_library_tab_shows(entity_id, kind)) media_library_render(media_control_modal_ui().active);
}

inline void media_library_request(const std::string &entity_id,
                                  espcontrol::media::LibraryKind kind, size_t offset) {
  espcontrol::media::LibraryList &list = media_library_list(entity_id, kind);
  if (list.loading) return;
  list.failed = true;
  if (!espcontrol::media::library_entity_id_safe(entity_id)) return;
  esphome::api::HomeassistantActionRequest req;
  const uint32_t call_id = next_media_library_call_id();
  if (!ha_action_begin(req, "music_assistant.get_library", false, 1, call_id)) return;
  req.wants_response = true;
  req.response_template = decltype(req.response_template)(
    espcontrol::media::library_response_template());
  ha_action_add_data(req, "media_type", espcontrol::media::library_media_type(kind));
  // Templates render to numbers, which the action's schema expects. The
  // config entry is the Music Assistant instance that owns this player.
  const std::string config_entry = espcontrol::media::library_config_entry_template(entity_id);
  const std::string limit = "{{ " + std::to_string(espcontrol::media::LIBRARY_PAGE_SIZE) + " }}";
  const std::string offset_template = "{{ " + std::to_string(offset) + " }}";
  req.data_template.init(3);
  ha_action_add_data_template(req, "config_entry_id", config_entry.c_str());
  ha_action_add_data_template(req, "limit", limit.c_str());
  ha_action_add_data_template(req, "offset", offset_template.c_str());
  if (!ha_register_action_response_callback(
        call_id,
        [entity_id, kind, call_id, offset](const esphome::api::ActionResponse &response) {
          media_library_handle_response(entity_id, kind, call_id, offset, response);
        })) return;
  list.loading = true;
  list.failed = false;
  list.call_id = call_id;
  list.request_ms = esphome::millis();
  if (!ha_action_send(req)) ha_cancel_action_response_callback(call_id, "send failed");
}

// Give up on a request Home Assistant never answered.
inline void media_library_expire_request(espcontrol::media::LibraryList &list) {
  if (!list.loading || list.call_id == 0) return;
  if (esphome::millis() - list.request_ms < espcontrol::media::LIBRARY_REQUEST_TIMEOUT_MS) return;
  ha_cancel_action_response_callback(list.call_id, "timeout");
}

inline void media_library_style_row(MediaControlCtx *ctx, lv_obj_t *row, bool starting) {
  const uint32_t bg = starting ? ctx->accent_color
                               : theme_display_color(current_theme().surface_primary);
  const uint32_t text = starting ? readable_text_color_for_bg(bg) : current_theme().text_primary;
  theme_set_content_background(row, starting);
  lv_obj_set_style_bg_color(row, lv_color_hex(bg), LV_PART_MAIN);
  for (uint32_t i = 0; i < lv_obj_get_child_cnt(row); ++i) {
    lv_obj_set_style_text_color(lv_obj_get_child(row, i), lv_color_hex(text), LV_PART_MAIN);
  }
}

inline void media_library_set_status(const char *text) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ui.library_status_lbl) return;
  if (!text) {
    lv_obj_add_flag(ui.library_status_lbl, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_label_set_display_text(ui.library_status_lbl, text);
  lv_obj_clear_flag(ui.library_status_lbl, LV_OBJ_FLAG_HIDDEN);
}

// Place the row pool over the entries around the current scroll position.
// Rows are only relabelled and restyled when they move to a different entry
// or the entry being started changes, so plain scrolling stays cheap.
inline void media_library_render(MediaControlCtx *ctx) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ctx || ui.active != ctx || !ui.library_list || ui.library_row_pitch <= 0) return;
  const auto kind = media_library_kind_for_tab(ui.tab);
  espcontrol::media::LibraryList &list = media_library_list(ctx->entity_id, kind);
  const size_t count = list.items.size();
  const lv_coord_t pitch = ui.library_row_pitch;
  const lv_coord_t gap = pitch - ui.library_row_h;
  const lv_coord_t total_h = count ? static_cast<lv_coord_t>(count) * pitch - gap : 0;
  if (lv_obj_get_height(ui.library_spacer) != total_h) lv_obj_set_height(ui.library_spacer, total_h);
  if (ui.library_pending_key) {
    const int index = espcontrol::media::library_index_find(list.items, ui.library_pending_key);
    if (index >= 0 || list.complete || list.failed ||
        count >= MEDIA_LIBRARY_MAX_ITEMS) {
      // Found, or nothing later exists: show the match, else the last entry.
      ui.library_pending_key = 0;
      const size_t target = index >= 0 ? static_cast<size_t>(index) : (count ? count - 1 : 0);
      lv_obj_update_layout(ui.library_list);
      const lv_coord_t max_y = total_h - lv_obj_get_content_height(ui.library_list);
      lv_coord_t y = static_cast<lv_coord_t>(target) * pitch;
      if (y > max_y) y = max_y > 0 ? max_y : 0;
      // A real scroll re-enters this function through LV_EVENT_SCROLL; the
      // rest of this pass then places rows for the new position anyway.
      lv_obj_scroll_to_y(ui.library_list, y, LV_ANIM_OFF);
    } else if (!list.loading) {
      media_library_request(ctx->entity_id, kind, count);
    }
  }

  const size_t first = espcontrol::media::library_first_visible(
    lv_obj_get_scroll_y(ui.library_list), pitch, count);
  const bool starting_changed = ui.library_rendered_starting != ui.library_starting_index;
  ui.library_rendered_starting = ui.library_starting_index;
  for (size_t i = 0; i < ui.library_rows.size(); ++i) {
    lv_obj_t *row = ui.library_rows[i];
    const size_t index = first + i;
    if (index >= count) {
      lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_user_data(row, nullptr);
      continue;
    }
    // User data holds index + 1 so an unused row stays distinguishable.
    void *tag = reinterpret_cast<void *>(static_cast<uintptr_t>(index + 1));
    const bool moved = lv_obj_get_user_data(row) != tag || lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN);
    if (moved) {
      lv_obj_set_user_data(row, tag);
      lv_obj_set_y(row, static_cast<lv_coord_t>(index) * pitch);
      lv_obj_t *name = lv_obj_get_child(row, 0);
      if (name) lv_label_set_display_text(name, list.items[index].name.c_str());
      lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
    if (moved || starting_changed) {
      media_library_style_row(ctx, row, static_cast<int>(index) == ui.library_starting_index);
    }
  }

  if (ui.library_play_failed) media_library_set_status(espcontrol_i18n("Playback failed"));
  else if (ui.library_pending_key) media_library_set_status(espcontrol_i18n("Loading"));
  else if (count) media_library_set_status(nullptr);
  else if (list.loading) media_library_set_status(espcontrol_i18n("Loading"));
  else if (list.failed) media_library_set_status(espcontrol_i18n("Could not load"));
  else media_library_set_status(espcontrol_i18n("Nothing found"));

  const lv_coord_t list_h = lv_obj_get_height(ui.library_list);
  const size_t visible = list_h > 0 ? static_cast<size_t>(list_h / pitch) + 1 : ui.library_rows.size();
  if (espcontrol::media::library_should_fetch_more(list, first + visible, MEDIA_LIBRARY_MAX_ITEMS)) {
    media_library_request(ctx->entity_id, kind, count);
  }
}

inline void media_library_play_result(const std::string &entity_id, uint32_t call_id,
                                      const esphome::api::ActionResponse &response) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!response.is_success()) {
    ESP_LOGW("media_library", "music_assistant.play_media failed for %s: %s",
             entity_id.c_str(), response.get_error_message().c_str());
  }
  if (!ui.active || ui.active->entity_id != entity_id || ui.library_play_call_id != call_id) return;
  ui.library_play_call_id = 0;
  ui.library_starting_index = -1;
  if (response.is_success() &&
      media_control_visible_tabs(ui.active).contains(MediaControlTab::CONTROLS)) {
    media_control_select_tab(MediaControlTab::CONTROLS);
    return;
  }
  ui.library_play_failed = !response.is_success();
  media_library_render(ui.active);
}

inline void media_library_play(MediaControlCtx *ctx, size_t index) {
  MediaControlModalUi &ui = media_control_modal_ui();
  const auto kind = media_library_kind_for_tab(ui.tab);
  espcontrol::media::LibraryList &list = media_library_list(ctx->entity_id, kind);
  if (index >= list.items.size()) return;
  const std::string uri = list.items[index].uri;
  ESP_LOGI("media_library", "Playing %s %s on %s", espcontrol::media::library_media_type(kind),
           uri.c_str(), ctx->entity_id.c_str());
  esphome::api::HomeassistantActionRequest req;
  const uint32_t call_id = next_media_library_call_id();
  ui.library_play_failed = false;
  if (!ha_action_begin(req, "music_assistant.play_media", false, 3, call_id)) {
    ui.library_play_failed = true;
    media_library_render(ctx);
    return;
  }
  ha_action_add_entity(req, ctx->entity_id);
  ha_action_add_data(req, "media_id", uri.c_str());
  ha_action_add_data(req, "media_type", espcontrol::media::library_media_type(kind));
  const std::string entity_id = ctx->entity_id;
  if (!ha_register_action_response_callback(
        call_id, [entity_id, call_id](const esphome::api::ActionResponse &response) {
          media_library_play_result(entity_id, call_id, response);
        })) {
    ui.library_play_failed = true;
    media_library_render(ctx);
    return;
  }
  ui.library_play_call_id = call_id;
  ui.library_play_ms = esphome::millis();
  ui.library_starting_index = static_cast<int>(index);
  media_library_render(ctx);
  if (!ha_action_send(req)) ha_cancel_action_response_callback(call_id, "send failed");
}

inline void media_library_timer_cb(lv_timer_t *) {
  MediaControlModalUi &ui = media_control_modal_ui();
  MediaControlCtx *ctx = ui.active;
  if (!ctx || !ui.library_list) return;
  media_library_expire_request(media_library_list(ctx->entity_id, media_library_kind_for_tab(ui.tab)));
  if (ui.library_play_call_id != 0 &&
      esphome::millis() - ui.library_play_ms >= espcontrol::media::LIBRARY_REQUEST_TIMEOUT_MS) {
    ha_cancel_action_response_callback(ui.library_play_call_id, "timeout");
  }
}

inline lv_obj_t *media_library_create_row(MediaControlCtx *ctx, lv_obj_t *parent,
                                          lv_coord_t height, lv_coord_t short_side) {
  lv_obj_t *row = lv_btn_create(parent);
  lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_size(row, LV_PCT(100), height);
  lv_obj_set_style_radius(row, control_modal_card_radius(ctx->btn), LV_PART_MAIN);
  lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
  const lv_coord_t pad_x = control_modal_scaled_px(18, short_side);
  lv_obj_set_style_pad_left(row, pad_x, LV_PART_MAIN);
  lv_obj_set_style_pad_right(row, pad_x, LV_PART_MAIN);
  lv_obj_set_style_pad_top(row, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(row, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(row, control_modal_scaled_px(14, short_side), LV_PART_MAIN);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
  lv_obj_set_style_flex_cross_place(row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_flex_track_place(row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  control_modal_apply_pressed_fill(row);

  lv_obj_t *name = lv_label_create(row);
  lv_obj_set_flex_grow(name, 1);
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
  lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  if (ctx->label_font) lv_obj_set_style_text_font(name, ctx->label_font, LV_PART_MAIN);

  lv_obj_add_event_cb(row, [](lv_event_t *event) {
    lv_obj_t *row = static_cast<lv_obj_t *>(lv_event_get_current_target(event));
    const uintptr_t tag = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row));
    MediaControlModalUi &ui = media_control_modal_ui();
    if (!ui.active || tag == 0 || ui.library_play_call_id != 0) return;
    if (esphome::millis() - ui.library_last_scroll_ms < 250) return;
    media_library_play(ui.active, tag - 1);
  }, LV_EVENT_CLICKED, nullptr);
  return row;
}

// Letter keys beside the Artists list. Tapping one scrolls to the first
// entry at or after that letter, loading further pages first if needed.
inline void media_library_create_index(MediaControlCtx *ctx, lv_obj_t *parent) {
  MediaControlModalUi &ui = media_control_modal_ui();
  ui.library_index = lv_obj_create(parent);
  lv_obj_t *index = ui.library_index;
  lv_obj_set_height(index, LV_PCT(100));
  lv_obj_set_flex_grow(index, 45);
  lv_obj_set_style_bg_opa(index, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(index, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(index, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(index, 6, LV_PART_MAIN);
  lv_obj_set_style_pad_column(index, 6, LV_PART_MAIN);
  lv_obj_set_layout(index, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(index, LV_FLEX_FLOW_ROW_WRAP, LV_PART_MAIN);
  lv_obj_set_style_flex_main_place(index, LV_FLEX_ALIGN_START, LV_PART_MAIN);
  lv_obj_set_style_flex_track_place(index, LV_FLEX_ALIGN_START, LV_PART_MAIN);
  lv_obj_clear_flag(index, LV_OBJ_FLAG_SCROLLABLE);
  // 27 keys in 5 columns by 6 rows.
  for (const char *key = espcontrol::media::LIBRARY_INDEX_KEYS; *key; ++key) {
    lv_obj_t *btn = lv_btn_create(index);
    lv_obj_set_size(btn, LV_PCT(18), LV_PCT(15));
    lv_obj_set_style_radius(btn, control_modal_card_radius(ctx->btn), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(btn, lv_color_hex(theme_display_color(current_theme().surface_primary)), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    control_modal_apply_pressed_fill(btn);
    lv_obj_t *label = lv_label_create(btn);
    const char text[2] = {*key, 0};
    lv_label_set_text(label, text);
    if (ctx->label_font) lv_obj_set_style_text_font(label, ctx->label_font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(current_theme().text_primary), LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, [](lv_event_t *event) {
      MediaControlModalUi &ui = media_control_modal_ui();
      if (!ui.active || !ui.library_list) return;
      ui.library_pending_key = static_cast<char>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
      media_library_render(ui.active);
    }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(static_cast<unsigned char>(*key))));
  }
}

inline void media_control_create_library_tab_content(MediaControlCtx *ctx) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ctx || !ui.content_box || ui.library_list) return;
  const auto kind = media_library_kind_for_tab(ui.tab);
  const ControlModalLayout layout = control_modal_calc_layout(ctx->width_compensation_percent);

  lv_obj_set_layout(ui.content_box, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ui.content_box, LV_FLEX_FLOW_COLUMN);

  lv_obj_t *body = lv_obj_create(ui.content_box);
  lv_obj_set_width(body, LV_PCT(100));
  lv_obj_set_flex_grow(body, 1);
  lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(body, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(body, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(body, control_modal_scaled_px(18, layout.short_side), LV_PART_MAIN);
  lv_obj_set_layout(body, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(body, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
  lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
  const bool has_index = kind == espcontrol::media::LibraryKind::ARTIST;
  if (has_index) media_library_create_index(ctx, body);

  ui.library_list = lv_obj_create(body);
  lv_obj_set_height(ui.library_list, LV_PCT(100));
  lv_obj_set_flex_grow(ui.library_list, 55);
  lv_obj_set_style_bg_opa(ui.library_list, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(ui.library_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(ui.library_list, 0, LV_PART_MAIN);
  lv_obj_add_flag(ui.library_list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(ui.library_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(ui.library_list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(ui.library_list, [](lv_event_t *) {
    MediaControlModalUi &ui = media_control_modal_ui();
    ui.library_last_scroll_ms = esphome::millis();
    media_library_render(ui.active);
  }, LV_EVENT_SCROLL, nullptr);
  lv_obj_add_event_cb(ui.library_list, [](lv_event_t *) {
    media_library_render(media_control_modal_ui().active);
  }, LV_EVENT_SIZE_CHANGED, nullptr);

  // The spacer sets the scrollable height for every loaded entry.
  ui.library_spacer = lv_obj_create(ui.library_list);
  lv_obj_remove_style_all(ui.library_spacer);
  lv_obj_clear_flag(ui.library_spacer, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(ui.library_spacer, 1, 0);
  lv_obj_set_pos(ui.library_spacer, 0, 0);

  lv_coord_t row_h = control_modal_scaled_px(76, layout.short_side);
  if (row_h < 60) row_h = 60;
  const lv_coord_t gap = 8;
  ui.library_row_h = row_h;
  ui.library_row_pitch = row_h + gap;
  // Enough rows to cover the tallest possible list area plus one partly
  // scrolled row at each edge.
  const size_t pool = static_cast<size_t>(layout.panel_h / ui.library_row_pitch) + 2;
  for (size_t i = 0; i < pool; ++i) {
    ui.library_rows.push_back(media_library_create_row(ctx, ui.library_list, row_h, layout.short_side));
  }
  // The status floats over the top of the list instead of taking a row in
  // the layout, so showing "Loading" during a letter jump moves nothing.
  ui.library_status_lbl = lv_label_create(body);
  lv_obj_add_flag(ui.library_status_lbl, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_width(ui.library_status_lbl, has_index ? LV_PCT(55) : LV_PCT(100));
  lv_obj_align(ui.library_status_lbl, LV_ALIGN_TOP_RIGHT, 0, 0);
  lv_obj_set_style_text_align(ui.library_status_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui.library_status_lbl, lv_color_hex(current_theme().text_muted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui.library_status_lbl,
    lv_color_hex(theme_display_color(current_theme().surface_secondary)), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui.library_status_lbl, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(ui.library_status_lbl, 8, LV_PART_MAIN);
  if (ctx->label_font) lv_obj_set_style_text_font(ui.library_status_lbl, ctx->label_font, LV_PART_MAIN);
  lv_obj_add_flag(ui.library_status_lbl, LV_OBJ_FLAG_HIDDEN);
  ui.library_timer = lv_timer_create(media_library_timer_cb, 500, nullptr);

  espcontrol::media::LibraryList &list = media_library_list(ctx->entity_id, kind);
  media_library_expire_request(list);
  const uint32_t now = esphome::millis();
  const bool stale = list.loaded_ms != 0 &&
    now - list.loaded_ms >= espcontrol::media::LIBRARY_STALE_MS;
  if (!list.loading && (list.failed || stale || (list.items.empty() && !list.complete))) {
    list.complete = false;
    media_library_request(ctx->entity_id, kind, 0);
  }
  media_library_render(ctx);
}
