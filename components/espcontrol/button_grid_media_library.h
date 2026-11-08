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
#define ESPCONTROL_MEDIA_LIBRARY_MAX_ITEMS 3000
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

// The All / Favorites / Recent choice of each tab, kept while the panel runs.
inline espcontrol::media::LibraryFilter &media_library_filter(espcontrol::media::LibraryKind kind) {
  static std::array<espcontrol::media::LibraryFilter, espcontrol::media::LIBRARY_KIND_COUNT> filters{};
  return filters[static_cast<size_t>(kind)];
}

// Recent only shows the most recently played page.
inline size_t media_library_max_items(espcontrol::media::LibraryFilter filter) {
  return espcontrol::media::library_filter_alphabetical(filter) ? MEDIA_LIBRARY_MAX_ITEMS
                                                                : espcontrol::media::LIBRARY_PAGE_SIZE;
}

inline espcontrol::media::LibraryList &media_library_list(
    const std::string &entity_id, espcontrol::media::LibraryKind kind,
    espcontrol::media::LibraryFilter filter) {
  using Lists = std::array<espcontrol::media::LibraryList,
                           espcontrol::media::LIBRARY_KIND_COUNT * espcontrol::media::LIBRARY_FILTER_COUNT>;
  static std::map<std::string, Lists> lists;
  return lists[entity_id][static_cast<size_t>(kind) * espcontrol::media::LIBRARY_FILTER_COUNT +
                          static_cast<size_t>(filter)];
}

// The list the open tab shows.
inline espcontrol::media::LibraryList &media_library_shown_list(MediaControlCtx *ctx) {
  const auto kind = media_library_kind_for_tab(media_control_modal_ui().tab);
  return media_library_list(ctx->entity_id, kind, media_library_filter(kind));
}

inline uint32_t next_media_library_call_id() {
  static uint32_t call_id = 700000;
  return call_id++;
}

inline bool media_library_tab_shows(const std::string &entity_id,
                                    espcontrol::media::LibraryKind kind,
                                    espcontrol::media::LibraryFilter filter) {
  const MediaControlModalUi &ui = media_control_modal_ui();
  return ui.active && ui.library_list && ui.active->entity_id == entity_id &&
         ui.tab == media_library_tab_for_kind(kind) && media_library_filter(kind) == filter;
}

inline void media_library_render(MediaControlCtx *ctx);

inline void media_library_handle_response(const std::string &entity_id,
                                          espcontrol::media::LibraryKind kind,
                                          espcontrol::media::LibraryFilter filter,
                                          uint32_t call_id, size_t offset, size_t limit,
                                          const esphome::api::ActionResponse &response) {
  espcontrol::media::LibraryList &list = media_library_list(entity_id, kind, filter);
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
    if (offset == 0) {
      list.items.clear();
      list.fetched = 0;
      list.last_fetched.clear();
    }
    // A page for an older position (the list was reset meanwhile) is dropped.
    if (offset == list.fetched) {
      const size_t max_items = media_library_max_items(filter);
      const size_t before = list.items.size();
      const size_t lines = espcontrol::media::library_parse_page(
        payload ? payload : "", list.items, max_items);
      list.fetched += lines;
      if (list.items.size() > before) {
        list.last_fetched = espcontrol::media::library_page_progress(list.items, before);
      }
      list.complete = espcontrol::media::library_page_complete(
        lines, limit, list.items.size(), max_items);
      if (espcontrol::media::library_filter_alphabetical(filter) && list.items.size() > before) {
        espcontrol::media::library_sort(list.items);
        // Entries may have moved under the rows, so relabel them all and
        // find the entry being started again.
        if (media_library_tab_shows(entity_id, kind, filter)) {
          MediaControlModalUi &ui = media_control_modal_ui();
          for (lv_obj_t *row : ui.library_rows) lv_obj_set_user_data(row, nullptr);
          if (ui.library_starting_index >= 0) {
            ui.library_starting_index = -1;
            for (size_t i = 0; i < list.items.size(); ++i) {
              if (std::string_view(list.items[i].uri) == ui.library_starting_uri) {
                ui.library_starting_index = static_cast<int>(i);
                break;
              }
            }
          }
        }
      }
      list.failed = false;
      list.loaded_ms = esphome::millis();
      ESP_LOGI("media_library", "Loaded %u %s entries for %s (total %u%s)",
               (unsigned) lines, espcontrol::media::library_media_type(kind),
               entity_id.c_str(), (unsigned) list.items.size(),
               list.complete ? ", complete" : "");
    }
  }
  if (media_library_tab_shows(entity_id, kind, filter)) media_library_render(media_control_modal_ui().active);
}

inline void media_library_request(const std::string &entity_id,
                                  espcontrol::media::LibraryKind kind,
                                  espcontrol::media::LibraryFilter filter, size_t offset,
                                  size_t limit) {
  espcontrol::media::LibraryList &list = media_library_list(entity_id, kind, filter);
  if (list.loading) return;
  list.failed = true;
  if (!espcontrol::media::library_entity_id_safe(entity_id)) return;
  esphome::api::HomeassistantActionRequest req;
  const uint32_t call_id = next_media_library_call_id();
  const bool favorites = filter == espcontrol::media::LibraryFilter::FAVORITES;
  if (!ha_action_begin(req, "music_assistant.get_library", false, favorites ? 3 : 2, call_id)) return;
  req.wants_response = true;
  req.response_template = decltype(req.response_template)(
    espcontrol::media::library_response_template());
  ha_action_add_data(req, "media_type", espcontrol::media::library_media_type(kind));
  ha_action_add_data(req, "order_by", espcontrol::media::library_order_by(filter));
  // Templates render to numbers and booleans, which the action's schema
  // expects. The config entry is the Music Assistant instance that owns this
  // player.
  const std::string config_entry = espcontrol::media::library_config_entry_template(entity_id);
  const std::string limit_template = "{{ " + std::to_string(limit) + " }}";
  const std::string offset_template = "{{ " + std::to_string(offset) + " }}";
  req.data_template.init(favorites ? 4 : 3);
  ha_action_add_data_template(req, "config_entry_id", config_entry.c_str());
  ha_action_add_data_template(req, "limit", limit_template.c_str());
  ha_action_add_data_template(req, "offset", offset_template.c_str());
  if (favorites) ha_action_add_data_template(req, "favorite", "{{ true }}");
  if (!ha_register_action_response_callback(
        call_id,
        [entity_id, kind, filter, call_id, offset, limit](const esphome::api::ActionResponse &response) {
          media_library_handle_response(entity_id, kind, filter, call_id, offset, limit, response);
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

inline void media_library_spin_cb(void *arc, int32_t angle) {
  lv_arc_set_rotation(static_cast<lv_obj_t *>(arc), angle);
}

// The row being started is filled with the accent colour and shows a
// spinner until Music Assistant answers.
inline void media_library_style_row(MediaControlCtx *ctx, lv_obj_t *row, bool starting) {
  const uint32_t bg = starting ? ctx->accent_color
                               : theme_display_color(current_theme().surface_primary);
  const uint32_t text = starting ? readable_text_color_for_bg(bg) : current_theme().text_primary;
  theme_set_content_background(row, starting);
  lv_obj_set_style_bg_color(row, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_t *name = lv_obj_get_child(row, 0);
  if (name) lv_obj_set_style_text_color(name, lv_color_hex(text), LV_PART_MAIN);
  lv_obj_t *spinner = lv_obj_get_child(row, 1);
  if (!spinner) return;
  if (!starting) {
    lv_anim_delete(spinner, media_library_spin_cb);
    lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_set_style_arc_color(spinner, lv_color_hex(text), LV_PART_INDICATOR);
  lv_obj_clear_flag(spinner, LV_OBJ_FLAG_HIDDEN);
  if (lv_anim_get(spinner, media_library_spin_cb)) return;
  lv_anim_t anim;
  lv_anim_init(&anim);
  lv_anim_set_var(&anim, spinner);
  lv_anim_set_exec_cb(&anim, media_library_spin_cb);
  lv_anim_set_values(&anim, 0, 360);
  lv_anim_set_duration(&anim, 1000);
  lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&anim);
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

inline void media_library_set_state(lv_obj_t *obj, lv_state_t state, bool on) {
  if (!obj || lv_obj_has_state(obj, state) == on) return;
  if (on) lv_obj_add_state(obj, state);
  else lv_obj_clear_state(obj, state);
}

// Dims keys that cannot lead to an entry, lights the typed letters, and shows
// the typed prefix. Work is skipped while nothing that affects it changed.
inline void media_library_update_keys(espcontrol::media::LibraryFilter filter,
                                      const espcontrol::media::LibraryList &list) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ui.library_index) return;
  if (ui.library_keys_count != list.items.size() || ui.library_keys_complete != list.complete) {
    ui.library_keys_enabled = UINT32_MAX;  // recompute below
  }
  if (ui.library_keys_enabled == UINT32_MAX) {
    ui.library_keys_count = list.items.size();
    ui.library_keys_complete = list.complete;
    uint32_t enabled = 0;
    if (espcontrol::media::library_filter_alphabetical(filter) &&
        ui.library_prefix.size() < espcontrol::media::LIBRARY_PREFIX_MAX) {
      enabled = espcontrol::media::library_next_keys(list.items, ui.library_prefix, list.complete,
                                                     list.last_fetched);
    }
    // Keep the value distinct from the "recompute" marker.
    ui.library_keys_enabled = enabled & ((1u << espcontrol::media::LIBRARY_INDEX_KEY_COUNT) - 1);
    for (size_t i = 0; i < espcontrol::media::LIBRARY_INDEX_KEY_COUNT; ++i) {
      media_library_set_state(ui.library_keys[i], LV_STATE_DISABLED, !(ui.library_keys_enabled & (1u << i)));
    }
  }
  uint32_t lit = 0;
  for (char ch : ui.library_prefix) lit |= 1u << espcontrol::media::library_key_rank(ch);
  if (lit != ui.library_keys_lit) {
    ui.library_keys_lit = lit;
    for (size_t i = 0; i < espcontrol::media::LIBRARY_INDEX_KEY_COUNT; ++i) {
      media_library_set_state(ui.library_keys[i], LV_STATE_CHECKED, lit & (1u << i));
    }
    if (ui.library_prefix_lbl) lv_label_set_text(ui.library_prefix_lbl, ui.library_prefix.c_str());
    media_library_set_state(ui.library_delete_btn, LV_STATE_DISABLED, ui.library_prefix.empty());
  }
}

// Place the row pool over the entries around the current scroll position.
// Rows are only relabelled and restyled when they move to a different entry
// or the entry being started changes, so plain scrolling stays cheap.
inline void media_library_render(MediaControlCtx *ctx) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ctx || ui.active != ctx || !ui.library_list || ui.library_row_pitch <= 0) return;
  const auto kind = media_library_kind_for_tab(ui.tab);
  const auto filter = media_library_filter(kind);
  const size_t max_items = media_library_max_items(filter);
  espcontrol::media::LibraryList &list = media_library_list(ctx->entity_id, kind, filter);
  const size_t count = list.items.size();
  const lv_coord_t pitch = ui.library_row_pitch;
  const lv_coord_t gap = pitch - ui.library_row_h;
  const lv_coord_t total_h = count ? static_cast<lv_coord_t>(count) * pitch - gap : 0;
  if (lv_obj_get_height(ui.library_spacer) != total_h) lv_obj_set_height(ui.library_spacer, total_h);
  if (ui.library_jump_pending) {
    const int index = espcontrol::media::library_prefix_find(list.items, ui.library_jump_target);
    // Wait until loading has passed the prefix, so the jump lands on the
    // first match rather than one that happened to load early.
    const bool loaded = espcontrol::media::library_loaded_past(list, ui.library_jump_target);
    if ((index >= 0 && loaded) || list.complete || list.failed || count >= max_items) {
      // Found, or nothing later exists: show the match, else the last entry.
      ui.library_jump_pending = false;
      const size_t target = index >= 0 ? static_cast<size_t>(index) : (count ? count - 1 : 0);
      lv_obj_update_layout(ui.library_list);
      const lv_coord_t max_y = total_h - lv_obj_get_content_height(ui.library_list);
      lv_coord_t y = static_cast<lv_coord_t>(target) * pitch;
      if (y > max_y) y = max_y > 0 ? max_y : 0;
      // A real scroll re-enters this function through LV_EVENT_SCROLL; the
      // rest of this pass then places rows for the new position anyway.
      lv_obj_scroll_to_y(ui.library_list, y, LV_ANIM_OFF);
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
  else if (ui.library_jump_pending) media_library_set_status(espcontrol_i18n("Loading"));
  else if (count) media_library_set_status(nullptr);
  else if (list.loading) media_library_set_status(espcontrol_i18n("Loading"));
  else if (list.failed) media_library_set_status(espcontrol_i18n("Could not load"));
  else media_library_set_status(espcontrol_i18n("Nothing found"));

  media_library_update_keys(filter, list);

  // Keep loading in the background so the letter keys know every entry. The
  // first page is small so the list appears quickly.
  if (espcontrol::media::library_should_load_more(list, max_items)) {
    const size_t room = max_items - count;
    const size_t page = list.fetched == 0 ? espcontrol::media::LIBRARY_PAGE_SIZE
                                          : espcontrol::media::LIBRARY_BULK_PAGE_SIZE;
    media_library_request(ctx->entity_id, kind, filter, list.fetched, room < page ? room : page);
  }
}

// Starts loading the shown list when it is missing, failed, or out of date.
inline void media_library_open_list(MediaControlCtx *ctx) {
  const auto kind = media_library_kind_for_tab(media_control_modal_ui().tab);
  const auto filter = media_library_filter(kind);
  espcontrol::media::LibraryList &list = media_library_list(ctx->entity_id, kind, filter);
  media_library_expire_request(list);
  const uint32_t now = esphome::millis();
  const bool stale = list.loaded_ms != 0 &&
    now - list.loaded_ms >= espcontrol::media::LIBRARY_STALE_MS;
  if (!list.loading && (list.failed || stale || (list.items.empty() && !list.complete))) {
    list.complete = false;
    list.failed = false;
    media_library_request(ctx->entity_id, kind, filter, 0, espcontrol::media::LIBRARY_PAGE_SIZE);
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
  ui.library_starting_uri.clear();
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
  espcontrol::media::LibraryList &list = media_library_shown_list(ctx);
  if (index >= list.items.size()) return;
  const std::string uri(list.items[index].uri.data(), list.items[index].uri.size());
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
  ui.library_starting_uri = uri;
  media_library_render(ctx);
  if (!ha_action_send(req)) ha_cancel_action_response_callback(call_id, "send failed");
}

inline void media_library_timer_cb(lv_timer_t *) {
  MediaControlModalUi &ui = media_control_modal_ui();
  MediaControlCtx *ctx = ui.active;
  if (!ctx || !ui.library_list) return;
  media_library_expire_request(media_library_shown_list(ctx));
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

  // Spinner shown while this row's entry is starting.
  const lv_coord_t spinner_size = height * 2 / 5;
  lv_obj_t *spinner = lv_arc_create(row);
  lv_obj_set_size(spinner, spinner_size, spinner_size);
  lv_obj_remove_style(spinner, nullptr, LV_PART_KNOB);
  lv_obj_clear_flag(spinner, LV_OBJ_FLAG_CLICKABLE);
  lv_arc_set_bg_angles(spinner, 0, 360);
  lv_arc_set_angles(spinner, 0, 90);
  const lv_coord_t spinner_width = spinner_size / 8 > 2 ? spinner_size / 8 : 2;
  lv_obj_set_style_arc_width(spinner, spinner_width, LV_PART_MAIN);
  lv_obj_set_style_arc_width(spinner, spinner_width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_opa(spinner, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(spinner, true, LV_PART_INDICATOR);
  lv_obj_add_flag(spinner, LV_OBJ_FLAG_HIDDEN);

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

// Key styling: lit (checked) keys use the accent colour, keys that cannot
// lead to an entry are dimmed and ignore taps. The label inherits its text
// colour from the key, so state changes recolour it too.
inline lv_obj_t *media_library_create_key(MediaControlCtx *ctx, lv_obj_t *parent,
                                          lv_coord_t width_pct) {
  const ThemePalette &theme = current_theme();
  const uint32_t surface = theme_display_color(theme.surface_primary);
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, LV_PCT(width_pct), LV_PCT(15));
  lv_obj_set_style_radius(btn, control_modal_card_radius(ctx->btn), LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(btn, lv_color_hex(surface), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_color(btn, lv_color_hex(theme.text_primary), LV_PART_MAIN);
  control_modal_apply_pressed_fill(btn);
  const auto checked = static_cast<lv_style_selector_t>(LV_PART_MAIN) |
                       static_cast<lv_style_selector_t>(LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(ctx->accent_color), checked);
  lv_obj_set_style_text_color(btn, lv_color_hex(readable_text_color_for_bg(ctx->accent_color)), checked);
  const auto disabled = static_cast<lv_style_selector_t>(LV_PART_MAIN) |
                        static_cast<lv_style_selector_t>(LV_STATE_DISABLED);
  lv_obj_set_style_bg_opa(btn, LV_OPA_40, disabled);
  lv_obj_set_style_text_color(btn, lv_color_hex(theme.text_muted), disabled);
  return btn;
}

inline lv_obj_t *media_library_create_key_label(lv_obj_t *btn, const lv_font_t *font, const char *text) {
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, text);
  if (font) lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_center(label);
  return label;
}

inline void media_library_type_key(char key) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ui.active || !ui.library_list ||
      ui.library_prefix.size() >= espcontrol::media::LIBRARY_PREFIX_MAX) return;
  ui.library_prefix.push_back(key);
  ui.library_jump_target = ui.library_prefix;
  ui.library_jump_pending = true;
  ui.library_keys_enabled = UINT32_MAX;
  media_library_render(ui.active);
}

// Removes the last typed letter and jumps back to the shorter prefix, or
// clears every letter on a long press.
inline void media_library_delete_key(bool all) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ui.active || !ui.library_list || ui.library_prefix.empty()) return;
  if (all) ui.library_prefix.clear();
  else ui.library_prefix.pop_back();
  ui.library_keys_enabled = UINT32_MAX;
  if (!ui.library_prefix.empty()) {
    ui.library_jump_target = ui.library_prefix;
    ui.library_jump_pending = true;
  }
  media_library_render(ui.active);
}

// Letter keys beside the library list, with the typed letters and a delete
// key after Z. Tapping keys in a row builds a prefix ("A", then "AN"); the
// list scrolls to the first entry starting with it.
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
  // 27 keys in 5 columns by 6 rows; the last row ends with the typed letters
  // and the delete key.
  for (size_t i = 0; i < espcontrol::media::LIBRARY_INDEX_KEY_COUNT; ++i) {
    const char key = espcontrol::media::LIBRARY_INDEX_KEYS[i];
    lv_obj_t *btn = media_library_create_key(ctx, index, 18);
    ui.library_keys[i] = btn;
    const char text[2] = {key, 0};
    media_library_create_key_label(btn, ctx->label_font, text);
    lv_obj_add_event_cb(btn, [](lv_event_t *event) {
      media_library_type_key(static_cast<char>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))));
    }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(static_cast<unsigned char>(key))));
  }

  // The typed letters fill the two spare key spaces before the delete key.
  lv_obj_t *prefix_box = lv_obj_create(index);
  lv_obj_remove_style_all(prefix_box);
  lv_obj_set_size(prefix_box, LV_PCT(36), LV_PCT(15));
  lv_obj_clear_flag(prefix_box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(prefix_box, LV_OBJ_FLAG_SCROLLABLE);
  ui.library_prefix_lbl = lv_label_create(prefix_box);
  lv_obj_set_width(ui.library_prefix_lbl, LV_PCT(100));
  lv_obj_set_style_text_align(ui.library_prefix_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui.library_prefix_lbl, lv_color_hex(ctx->accent_color), LV_PART_MAIN);
  lv_label_set_long_mode(ui.library_prefix_lbl, LV_LABEL_LONG_CLIP);
  if (ctx->label_font) lv_obj_set_style_text_font(ui.library_prefix_lbl, ctx->label_font, LV_PART_MAIN);
  lv_label_set_text(ui.library_prefix_lbl, "");
  lv_obj_center(ui.library_prefix_lbl);

  ui.library_delete_btn = media_library_create_key(ctx, index, 18);
  media_library_create_key_label(ui.library_delete_btn, ctx->icon_font, find_icon("Backspace"));
  lv_obj_add_state(ui.library_delete_btn, LV_STATE_DISABLED);
  lv_obj_add_event_cb(ui.library_delete_btn, [](lv_event_t *) {
    media_library_delete_key(false);
  }, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(ui.library_delete_btn, [](lv_event_t *) {
    media_library_delete_key(true);
  }, LV_EVENT_LONG_PRESSED, nullptr);
}

inline void media_library_select_filter(espcontrol::media::LibraryFilter filter) {
  MediaControlModalUi &ui = media_control_modal_ui();
  MediaControlCtx *ctx = ui.active;
  if (!ctx || !ui.library_list) return;
  const auto kind = media_library_kind_for_tab(ui.tab);
  if (media_library_filter(kind) == filter) return;
  media_library_filter(kind) = filter;
  for (size_t i = 0; i < espcontrol::media::LIBRARY_FILTER_COUNT; ++i) {
    media_library_set_state(ui.library_filter_btns[i], LV_STATE_CHECKED, i == static_cast<size_t>(filter));
  }
  ui.library_prefix.clear();
  ui.library_jump_pending = false;
  ui.library_keys_enabled = UINT32_MAX;
  ui.library_starting_index = -1;
  ui.library_play_failed = false;
  // A different list: start at the top and relabel every row.
  for (lv_obj_t *row : ui.library_rows) lv_obj_set_user_data(row, nullptr);
  lv_obj_scroll_to_y(ui.library_list, 0, LV_ANIM_OFF);
  media_library_open_list(ctx);
  media_library_render(ctx);
}

// All / Favorites / Recent buttons above the list.
inline lv_obj_t *media_library_create_filter_bar(MediaControlCtx *ctx, lv_obj_t *parent,
                                                 lv_coord_t height) {
  MediaControlModalUi &ui = media_control_modal_ui();
  lv_obj_t *bar = lv_obj_create(parent);
  lv_obj_set_size(bar, LV_PCT(100), height);
  lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(bar, 6, LV_PART_MAIN);
  lv_obj_set_layout(bar, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(bar, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  static const char *const LABELS[espcontrol::media::LIBRARY_FILTER_COUNT] = {
    "All", "Favorites", "Recent"};
  const auto selected = media_library_filter(media_library_kind_for_tab(ui.tab));
  for (size_t i = 0; i < espcontrol::media::LIBRARY_FILTER_COUNT; ++i) {
    lv_obj_t *btn = media_library_create_key(ctx, bar, 30);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_set_flex_grow(btn, 1);
    media_library_create_key_label(btn, ctx->label_font, espcontrol_i18n(LABELS[i]));
    if (i == static_cast<size_t>(selected)) lv_obj_add_state(btn, LV_STATE_CHECKED);
    ui.library_filter_btns[i] = btn;
    lv_obj_add_event_cb(btn, [](lv_event_t *event) {
      media_library_select_filter(static_cast<espcontrol::media::LibraryFilter>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))));
    }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
  }
  return bar;
}

inline void media_control_create_library_tab_content(MediaControlCtx *ctx) {
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ctx || !ui.content_box || ui.library_list) return;
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
  media_library_create_index(ctx, body);

  lv_coord_t row_h = control_modal_scaled_px(76, layout.short_side);
  if (row_h < 60) row_h = 60;
  const lv_coord_t gap = 8;

  // Right column: filter buttons above the list.
  lv_obj_t *column = lv_obj_create(body);
  lv_obj_set_height(column, LV_PCT(100));
  lv_obj_set_flex_grow(column, 55);
  lv_obj_set_style_bg_opa(column, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(column, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(column, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(column, gap, LV_PART_MAIN);
  lv_obj_set_layout(column, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(column, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
  lv_obj_clear_flag(column, LV_OBJ_FLAG_SCROLLABLE);
  const lv_coord_t bar_h = row_h * 3 / 4;
  media_library_create_filter_bar(ctx, column, bar_h);

  ui.library_list = lv_obj_create(column);
  lv_obj_set_width(ui.library_list, LV_PCT(100));
  lv_obj_set_flex_grow(ui.library_list, 1);
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
  ui.library_status_lbl = lv_label_create(column);
  lv_obj_add_flag(ui.library_status_lbl, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_width(ui.library_status_lbl, LV_PCT(100));
  lv_obj_align(ui.library_status_lbl, LV_ALIGN_TOP_MID, 0, bar_h + gap);
  lv_obj_set_style_text_align(ui.library_status_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(ui.library_status_lbl, lv_color_hex(current_theme().text_muted), LV_PART_MAIN);
  lv_obj_set_style_bg_color(ui.library_status_lbl,
    lv_color_hex(theme_display_color(current_theme().surface_secondary)), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(ui.library_status_lbl, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(ui.library_status_lbl, 8, LV_PART_MAIN);
  if (ctx->label_font) lv_obj_set_style_text_font(ui.library_status_lbl, ctx->label_font, LV_PART_MAIN);
  lv_obj_add_flag(ui.library_status_lbl, LV_OBJ_FLAG_HIDDEN);
  ui.library_timer = lv_timer_create(media_library_timer_cb, 500, nullptr);

  media_library_open_list(ctx);
  media_library_render(ctx);
}
