#pragma once

// Search tab for the media control modal. A QWERTY keyboard builds the query;
// music_assistant.search looks it up in the library and every connected
// provider of the Music Assistant instance that owns the card's player, and a
// tapped result replaces the queue with music_assistant.play_media.
//
// Home Assistant renders the response into "kind<TAB>name<TAB>detail<TAB>uri"
// lines, so the firmware never parses the search JSON itself. One search asks
// for every media type; the filter buttons then switch on the panel.

#include <array>
#include <string>
#include <vector>

#include "media_search.h"

#ifdef ESPCONTROL_LOW_HEAP_MEDIA_CONTROL
constexpr int MEDIA_SEARCH_LIMIT = 5;
#else
constexpr int MEDIA_SEARCH_LIMIT = 10;
#endif
constexpr size_t MEDIA_SEARCH_QUERY_MAX = 48;

// The last search, kept while the panel runs so the tab reopens on it.
struct MediaSearchState {
  std::string entity_id;
  std::string query;
  std::string searched;  // Query the results belong to.
  std::vector<espcontrol::media::SearchItem> results;
  espcontrol::media::SearchFilter filter = espcontrol::media::SearchFilter::ALL;
  bool editing = true;
  bool loading = false;
  bool failed = false;
  uint32_t call_id = 0;
  uint32_t request_ms = 0;
};

inline MediaSearchState &media_search_state() {
  static MediaSearchState state;
  return state;
}

// Objects of the open Search tab. All are children of the content box.
struct MediaSearchUi {
  lv_obj_t *keyboard = nullptr;
  lv_obj_t *results = nullptr;
  lv_obj_t *edit_query_lbl = nullptr;
  lv_obj_t *results_query_lbl = nullptr;
  lv_obj_t *delete_btn = nullptr;
  lv_obj_t *search_btn = nullptr;
  lv_obj_t *list = nullptr;
  lv_obj_t *status_lbl = nullptr;
  std::array<lv_obj_t *, espcontrol::media::SEARCH_FILTER_COUNT> filter_btns{};
  // Result index of each list row, in row order.
  std::vector<size_t> row_items;
  std::vector<lv_obj_t *> rows;
};

inline MediaSearchUi &media_search_ui() {
  static MediaSearchUi ui;
  return ui;
}

inline void media_search_clear_ui() { media_search_ui() = MediaSearchUi(); }

inline bool media_search_tab_shows(const std::string &entity_id) {
  const MediaControlModalUi &ui = media_control_modal_ui();
  return ui.active && ui.active->entity_id == entity_id && ui.tab == MediaControlTab::SEARCH &&
         media_search_ui().keyboard;
}

// The state for this player; another player starts from an empty search.
inline MediaSearchState &media_search_state_for(MediaControlCtx *ctx) {
  MediaSearchState &state = media_search_state();
  if (ctx && state.entity_id != ctx->entity_id) {
    if (state.call_id != 0) ha_cancel_action_response_callback(state.call_id, "player changed");
    state = MediaSearchState();
    state.entity_id = ctx->entity_id;
  }
  return state;
}

inline void media_search_set_status(const char *text) {
  lv_obj_t *label = media_search_ui().status_lbl;
  if (!label) return;
  if (!text) {
    lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_label_set_display_text(label, text);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
}

inline void media_search_style_row(MediaControlCtx *ctx, lv_obj_t *row, bool starting) {
  const uint32_t bg = starting ? ctx->accent_color
                               : theme_display_color(current_theme().surface_primary);
  const uint32_t text = starting ? readable_text_color_for_bg(bg) : current_theme().text_primary;
  theme_set_content_background(row, starting);
  lv_obj_set_style_bg_color(row, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_text_color(row, lv_color_hex(text), LV_PART_MAIN);
  // Children: icon, text column (name, detail), spinner.
  lv_obj_t *spinner = lv_obj_get_child(row, 2);
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

inline void media_search_play(MediaControlCtx *ctx, size_t index);

inline lv_obj_t *media_search_create_row(MediaControlCtx *ctx, lv_obj_t *parent,
                                         const espcontrol::media::SearchItem &item,
                                         lv_coord_t height, lv_coord_t short_side) {
  lv_obj_t *row = lv_btn_create(parent);
  lv_obj_set_size(row, LV_PCT(100), height);
  lv_obj_set_style_radius(row, control_modal_card_radius(ctx->btn), LV_PART_MAIN);
  lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(row, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
  const lv_coord_t pad_x = control_modal_scaled_px(18, short_side);
  lv_obj_set_style_pad_left(row, pad_x, LV_PART_MAIN);
  lv_obj_set_style_pad_right(row, pad_x, LV_PART_MAIN);
  lv_obj_set_style_pad_ver(row, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_column(row, control_modal_scaled_px(16, short_side), LV_PART_MAIN);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
  lv_obj_set_style_flex_cross_place(row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_flex_track_place(row, LV_FLEX_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(row, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  control_modal_apply_pressed_fill(row);

  lv_obj_t *icon = lv_label_create(row);
  lv_label_set_display_text(icon, find_icon(espcontrol::media::search_kind_icon_name(item.kind)));
  if (ctx->icon_font) lv_obj_set_style_text_font(icon, ctx->icon_font, LV_PART_MAIN);

  lv_obj_t *text = lv_obj_create(row);
  lv_obj_remove_style_all(text);
  lv_obj_set_flex_grow(text, 1);
  lv_obj_set_height(text, LV_SIZE_CONTENT);
  lv_obj_set_layout(text, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(text, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
  lv_obj_clear_flag(text, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(text, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(text, LV_OBJ_FLAG_EVENT_BUBBLE);
  lv_obj_t *name = lv_label_create(text);
  lv_obj_set_width(name, LV_PCT(100));
  lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
  if (ctx->label_font) lv_obj_set_style_text_font(name, ctx->label_font, LV_PART_MAIN);
  lv_label_set_display_text(name, item.name.c_str());
  if (!item.detail.empty()) {
    lv_obj_t *detail = lv_label_create(text);
    lv_obj_set_width(detail, LV_PCT(100));
    lv_label_set_long_mode(detail, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_color(detail, lv_color_hex(current_theme().text_muted), LV_PART_MAIN);
    lv_obj_set_style_text_opa(detail, LV_OPA_80, LV_PART_MAIN);
    if (ctx->label_font) lv_obj_set_style_text_font(detail, ctx->label_font, LV_PART_MAIN);
    lv_label_set_display_text(detail, item.detail.c_str());
  }

  // Spinner shown while this row's item is starting.
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
    MediaControlModalUi &ui = media_control_modal_ui();
    if (!ui.active || ui.library_play_call_id != 0) return;
    if (esphome::millis() - ui.library_last_scroll_ms < 250) return;
    const uintptr_t tag = reinterpret_cast<uintptr_t>(lv_obj_get_user_data(row));
    if (tag == 0) return;
    media_search_play(ui.active, tag - 1);
  }, LV_EVENT_CLICKED, nullptr);
  return row;
}

// Rebuild the list for the selected filter.
inline void media_search_fill_list(MediaControlCtx *ctx) {
  MediaSearchUi &sui = media_search_ui();
  if (!ctx || !sui.list) return;
  const MediaSearchState &state = media_search_state_for(ctx);
  lv_obj_clean(sui.list);
  sui.rows.clear();
  sui.row_items.clear();
  const ControlModalLayout layout = control_modal_calc_layout(ctx->width_compensation_percent);
  lv_coord_t row_h = control_modal_scaled_px(84, layout.short_side);
  if (row_h < 64) row_h = 64;
  for (size_t i = 0; i < state.results.size(); ++i) {
    const auto &item = state.results[i];
    if (!espcontrol::media::search_filter_matches(state.filter, item.kind)) continue;
    lv_obj_t *row = media_search_create_row(ctx, sui.list, item, row_h, layout.short_side);
    lv_obj_set_user_data(row, reinterpret_cast<void *>(static_cast<uintptr_t>(i + 1)));
    sui.rows.push_back(row);
    sui.row_items.push_back(i);
  }
  lv_obj_scroll_to_y(sui.list, 0, LV_ANIM_OFF);
}

// Show the keyboard or the results, and refresh what each shows.
inline void media_search_render(MediaControlCtx *ctx) {
  MediaSearchUi &sui = media_search_ui();
  MediaControlModalUi &ui = media_control_modal_ui();
  if (!ctx || ui.active != ctx || !sui.keyboard || !sui.results) return;
  const MediaSearchState &state = media_search_state_for(ctx);
  if (state.editing) {
    lv_obj_clear_flag(sui.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(sui.results, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(sui.keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(sui.results, LV_OBJ_FLAG_HIDDEN);
  }
  const bool empty = state.query.empty();
  if (sui.edit_query_lbl) {
    lv_label_set_display_text(
      sui.edit_query_lbl, empty ? espcontrol_i18n("Search Music Assistant") : state.query.c_str());
    lv_obj_set_style_text_color(
      sui.edit_query_lbl,
      lv_color_hex(empty ? current_theme().text_muted : current_theme().text_primary), LV_PART_MAIN);
  }
  if (sui.results_query_lbl) lv_label_set_display_text(sui.results_query_lbl, state.searched.c_str());
  media_library_set_state(sui.delete_btn, LV_STATE_DISABLED, empty);
  media_library_set_state(sui.search_btn, LV_STATE_DISABLED, empty);

  for (size_t i = 0; i < sui.filter_btns.size(); ++i) {
    const auto filter = static_cast<espcontrol::media::SearchFilter>(i);
    const bool has = espcontrol::media::search_filter_count(state.results, filter) > 0;
    media_library_set_state(sui.filter_btns[i], LV_STATE_CHECKED, filter == state.filter);
    media_library_set_state(sui.filter_btns[i], LV_STATE_DISABLED,
                            filter != espcontrol::media::SearchFilter::ALL && !has);
  }
  for (size_t i = 0; i < sui.rows.size(); ++i) {
    media_search_style_row(ctx, sui.rows[i],
                           static_cast<int>(sui.row_items[i]) == ui.library_starting_index);
  }
  if (ui.library_play_failed) media_search_set_status(espcontrol_i18n("Playback failed"));
  else if (state.loading) media_search_set_status(espcontrol_i18n("Searching"));
  else if (state.failed) media_search_set_status(espcontrol_i18n("Could not search"));
  else if (sui.rows.empty()) media_search_set_status(espcontrol_i18n("Nothing found"));
  else media_search_set_status(nullptr);
}

inline uint32_t next_media_search_call_id() {
  static uint32_t call_id = 760000;
  return call_id++;
}

inline void media_search_handle_response(const std::string &entity_id, uint32_t call_id,
                                         const esphome::api::ActionResponse &response) {
  MediaSearchState &state = media_search_state();
  if (state.entity_id != entity_id || state.call_id != call_id) return;
  state.loading = false;
  state.call_id = 0;
  state.results.clear();
  if (!response.is_success()) {
    state.failed = true;
    ESP_LOGW("media_search", "music_assistant.search failed for %s: %s", entity_id.c_str(),
             response.get_error_message().c_str());
  } else {
    auto json = response.get_json();
    const char *payload = json["response"].as<const char *>();
    espcontrol::media::search_parse_results(payload ? payload : "", state.results);
    state.failed = false;
    ESP_LOGI("media_search", "Found %u results for %s", (unsigned) state.results.size(),
             entity_id.c_str());
  }
  if (!media_search_tab_shows(entity_id)) return;
  MediaControlCtx *ctx = media_control_modal_ui().active;
  media_search_fill_list(ctx);
  media_search_render(ctx);
}

inline void media_search_request(MediaControlCtx *ctx) {
  MediaSearchState &state = media_search_state_for(ctx);
  if (!ctx || state.query.empty()) return;
  if (state.call_id != 0) ha_cancel_action_response_callback(state.call_id, "new search");
  state.call_id = 0;
  state.loading = false;
  state.failed = true;
  state.results.clear();
  state.searched = state.query;
  state.filter = espcontrol::media::SearchFilter::ALL;
  if (!espcontrol::media::library_entity_id_safe(ctx->entity_id)) return;
  esphome::api::HomeassistantActionRequest req;
  const uint32_t call_id = next_media_search_call_id();
  if (!ha_action_begin(req, "music_assistant.search", false, 1, call_id)) return;
  req.wants_response = true;
  req.response_template = decltype(req.response_template)(
    espcontrol::media::search_response_template());
  ha_action_add_data(req, "name", state.query.c_str());
  const std::string config_entry = espcontrol::media::library_config_entry_template(ctx->entity_id);
  const std::string limit_template = "{{ " + std::to_string(MEDIA_SEARCH_LIMIT) + " }}";
  req.data_template.init(2);
  ha_action_add_data_template(req, "config_entry_id", config_entry.c_str());
  ha_action_add_data_template(req, "limit", limit_template.c_str());
  const std::string entity_id = ctx->entity_id;
  if (!ha_register_action_response_callback(
        call_id, [entity_id, call_id](const esphome::api::ActionResponse &response) {
          media_search_handle_response(entity_id, call_id, response);
        })) return;
  state.loading = true;
  state.failed = false;
  state.call_id = call_id;
  state.request_ms = esphome::millis();
  ESP_LOGI("media_search", "Searching \"%s\" for %s", state.query.c_str(), entity_id.c_str());
  if (!ha_action_send(req)) ha_cancel_action_response_callback(call_id, "send failed");
}

// Replace the queue with the tapped item and start it. Playback bookkeeping
// is shared with the library tabs, so a started track returns to Controls.
inline void media_search_play(MediaControlCtx *ctx, size_t index) {
  MediaControlModalUi &ui = media_control_modal_ui();
  const MediaSearchState &state = media_search_state_for(ctx);
  if (!ctx || index >= state.results.size()) return;
  const auto &item = state.results[index];
  ESP_LOGI("media_search", "Playing %s %s on %s", espcontrol::media::search_kind_media_type(item.kind),
           item.uri.c_str(), ctx->entity_id.c_str());
  esphome::api::HomeassistantActionRequest req;
  const uint32_t call_id = next_media_library_call_id();
  ui.library_play_failed = false;
  if (!ha_action_begin(req, "music_assistant.play_media", false, 4, call_id)) {
    ui.library_play_failed = true;
    media_search_render(ctx);
    return;
  }
  ha_action_add_entity(req, ctx->entity_id);
  ha_action_add_data(req, "media_id", item.uri.c_str());
  ha_action_add_data(req, "media_type", espcontrol::media::search_kind_media_type(item.kind));
  ha_action_add_data(req, "enqueue", "replace");
  const std::string entity_id = ctx->entity_id;
  if (!ha_register_action_response_callback(
        call_id, [entity_id, call_id](const esphome::api::ActionResponse &response) {
          media_library_play_result(entity_id, call_id, response);
        })) {
    ui.library_play_failed = true;
    media_search_render(ctx);
    return;
  }
  ui.library_play_call_id = call_id;
  ui.library_play_ms = esphome::millis();
  ui.library_starting_index = static_cast<int>(index);
  ui.library_starting_uri = item.uri;
  ui.library_play_title = ctx->title + "\n" + ctx->artist;
  ui.library_play_was_playing = ctx->playing;
  media_search_render(ctx);
  if (!ha_action_send(req)) ha_cancel_action_response_callback(call_id, "send failed");
}

inline void media_search_timer_cb(lv_timer_t *) {
  MediaControlModalUi &ui = media_control_modal_ui();
  MediaControlCtx *ctx = ui.active;
  if (!ctx || !media_search_ui().keyboard) return;
  MediaSearchState &state = media_search_state_for(ctx);
  if (state.loading && state.call_id != 0 &&
      esphome::millis() - state.request_ms >= espcontrol::media::LIBRARY_REQUEST_TIMEOUT_MS) {
    ha_cancel_action_response_callback(state.call_id, "timeout");
  }
  if (ui.library_play_call_id != 0 &&
      esphome::millis() - ui.library_play_ms >= espcontrol::media::LIBRARY_PLAY_TIMEOUT_MS) {
    ha_cancel_action_response_callback(ui.library_play_call_id, MEDIA_LIBRARY_PLAY_TIMEOUT);
  }
}

inline void media_search_type(char key) {
  MediaControlCtx *ctx = media_control_modal_ui().active;
  if (!ctx) return;
  MediaSearchState &state = media_search_state_for(ctx);
  if (state.query.size() >= MEDIA_SEARCH_QUERY_MAX) return;
  if (key == ' ' && (state.query.empty() || state.query.back() == ' ')) return;
  state.query.push_back(key >= 'A' && key <= 'Z' ? static_cast<char>(key - 'A' + 'a') : key);
  media_search_render(ctx);
}

inline void media_search_delete(bool all) {
  MediaControlCtx *ctx = media_control_modal_ui().active;
  if (!ctx) return;
  MediaSearchState &state = media_search_state_for(ctx);
  if (state.query.empty()) return;
  if (all) state.query.clear();
  else state.query.pop_back();
  media_search_render(ctx);
}

// Search, or show the earlier results again when the query is unchanged.
inline void media_search_submit() {
  MediaControlCtx *ctx = media_control_modal_ui().active;
  if (!ctx) return;
  MediaSearchState &state = media_search_state_for(ctx);
  while (!state.query.empty() && state.query.back() == ' ') state.query.pop_back();
  if (state.query.empty()) {
    media_search_render(ctx);
    return;
  }
  const bool reuse = state.query == state.searched && !state.failed &&
                     (state.loading || !state.results.empty());
  state.editing = false;
  if (!reuse) {
    media_search_request(ctx);
    media_search_fill_list(ctx);
  }
  media_search_render(ctx);
}

inline void media_search_select_filter(espcontrol::media::SearchFilter filter) {
  MediaControlCtx *ctx = media_control_modal_ui().active;
  if (!ctx) return;
  MediaSearchState &state = media_search_state_for(ctx);
  if (state.filter == filter) return;
  state.filter = filter;
  media_search_fill_list(ctx);
  media_search_render(ctx);
}

inline lv_obj_t *media_search_create_panel(lv_obj_t *parent, lv_coord_t gap) {
  lv_obj_t *panel = lv_obj_create(parent);
  lv_obj_set_width(panel, LV_PCT(100));
  lv_obj_set_flex_grow(panel, 1);
  lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(panel, gap, LV_PART_MAIN);
  lv_obj_set_layout(panel, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(panel, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

inline lv_obj_t *media_search_create_row_box(lv_obj_t *parent, lv_coord_t gap) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, LV_PCT(100));
  lv_obj_set_flex_grow(row, 1);
  lv_obj_set_style_pad_column(row, gap, LV_PART_MAIN);
  lv_obj_set_layout(row, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(row, LV_FLEX_FLOW_ROW, LV_PART_MAIN);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
  return row;
}

// The query box: a key-styled field whose label shows the text.
inline lv_obj_t *media_search_create_field(MediaControlCtx *ctx, lv_obj_t *parent,
                                           lv_coord_t pad_x, lv_obj_t **label_out) {
  lv_obj_t *field = media_library_create_key(ctx, parent, 10);
  lv_obj_set_height(field, LV_PCT(100));
  lv_obj_set_flex_grow(field, 8);
  lv_obj_set_style_pad_hor(field, pad_x, LV_PART_MAIN);
  lv_obj_t *icon = lv_label_create(field);
  lv_label_set_display_text(icon, find_icon("Magnify"));
  if (ctx->icon_font) lv_obj_set_style_text_font(icon, ctx->icon_font, LV_PART_MAIN);
  lv_obj_set_style_text_color(icon, lv_color_hex(current_theme().text_muted), LV_PART_MAIN);
  lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t *label = lv_label_create(field);
  lv_obj_set_width(label, LV_PCT(88));
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  if (ctx->label_font) lv_obj_set_style_text_font(label, ctx->label_font, LV_PART_MAIN);
  lv_obj_align(label, LV_ALIGN_RIGHT_MID, 0, 0);
  *label_out = label;
  return field;
}

inline lv_obj_t *media_search_create_text_key(MediaControlCtx *ctx, lv_obj_t *row, char key,
                                              int grow) {
  lv_obj_t *btn = media_library_create_key(ctx, row, 10);
  lv_obj_set_height(btn, LV_PCT(100));
  lv_obj_set_flex_grow(btn, grow);
  const char text[2] = {key, 0};
  media_library_create_key_label(btn, ctx->label_font, key == ' ' ? "" : text);
  lv_obj_add_event_cb(btn, [](lv_event_t *event) {
    media_search_type(static_cast<char>(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))));
  }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(static_cast<unsigned char>(key))));
  return btn;
}

inline void media_search_create_spacer(lv_obj_t *row, int grow) {
  lv_obj_t *spacer = lv_obj_create(row);
  lv_obj_remove_style_all(spacer);
  lv_obj_set_height(spacer, LV_PCT(100));
  lv_obj_set_flex_grow(spacer, grow);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_CLICKABLE);
}

inline void media_search_create_keyboard(MediaControlCtx *ctx, lv_coord_t gap, lv_coord_t pad_x) {
  MediaSearchUi &sui = media_search_ui();
  lv_obj_t *panel = sui.keyboard;

  lv_obj_t *top = media_search_create_row_box(panel, gap);
  media_search_create_field(ctx, top, pad_x, &sui.edit_query_lbl);
  sui.delete_btn = media_library_create_key(ctx, top, 10);
  lv_obj_set_height(sui.delete_btn, LV_PCT(100));
  lv_obj_set_flex_grow(sui.delete_btn, 1);
  media_library_create_key_label(sui.delete_btn, ctx->icon_font, find_icon("Backspace"));
  lv_obj_add_event_cb(sui.delete_btn, [](lv_event_t *) { media_search_delete(false); },
                      LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(sui.delete_btn, [](lv_event_t *) { media_search_delete(true); },
                      LV_EVENT_LONG_PRESSED, nullptr);

  // Each key grows by 2; half-key spacers indent the shorter rows.
  for (size_t r = 0; r < espcontrol::media::SEARCH_KEY_ROW_COUNT; ++r) {
    const char *keys = espcontrol::media::SEARCH_KEY_ROWS[r];
    const size_t count = std::char_traits<char>::length(keys);
    const int indent = static_cast<int>(espcontrol::media::SEARCH_KEY_ROW_WIDTH - count);
    lv_obj_t *row = media_search_create_row_box(panel, gap);
    if (indent > 0) media_search_create_spacer(row, indent);
    for (size_t i = 0; i < count; ++i) media_search_create_text_key(ctx, row, keys[i], 2);
    if (indent > 0) media_search_create_spacer(row, indent);
  }

  lv_obj_t *bottom = media_search_create_row_box(panel, gap);
  media_search_create_spacer(bottom, 3);
  lv_obj_t *space = media_search_create_text_key(ctx, bottom, ' ', 9);
  (void) space;
  sui.search_btn = media_library_create_key(ctx, bottom, 10);
  lv_obj_set_height(sui.search_btn, LV_PCT(100));
  lv_obj_set_flex_grow(sui.search_btn, 5);
  lv_obj_add_state(sui.search_btn, LV_STATE_CHECKED);  // accent colour
  media_library_create_key_label(sui.search_btn, ctx->label_font, espcontrol_i18n("Search"));
  lv_obj_add_event_cb(sui.search_btn, [](lv_event_t *) { media_search_submit(); },
                      LV_EVENT_CLICKED, nullptr);
  media_search_create_spacer(bottom, 3);
}

inline void media_search_create_results(MediaControlCtx *ctx, lv_coord_t gap, lv_coord_t pad_x,
                                        lv_coord_t bar_h) {
  MediaSearchUi &sui = media_search_ui();
  lv_obj_t *panel = sui.results;

  lv_obj_t *top = media_search_create_row_box(panel, gap);
  lv_obj_set_flex_grow(top, 0);
  lv_obj_set_height(top, bar_h);
  lv_obj_t *field = media_search_create_field(ctx, top, pad_x, &sui.results_query_lbl);
  lv_obj_add_event_cb(field, [](lv_event_t *) {
    MediaControlCtx *ctx = media_control_modal_ui().active;
    if (!ctx) return;
    media_search_state_for(ctx).editing = true;
    media_search_render(ctx);
  }, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *filters = media_search_create_row_box(panel, 6);
  lv_obj_set_flex_grow(filters, 0);
  lv_obj_set_height(filters, bar_h);
  for (size_t i = 0; i < espcontrol::media::SEARCH_FILTER_COUNT; ++i) {
    const auto filter = static_cast<espcontrol::media::SearchFilter>(i);
    lv_obj_t *btn = media_library_create_key(ctx, filters, 10);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_set_flex_grow(btn, 1);
    media_library_create_key_label(btn, ctx->label_font,
                                   espcontrol_i18n(espcontrol::media::search_filter_label(filter)));
    sui.filter_btns[i] = btn;
    lv_obj_add_event_cb(btn, [](lv_event_t *event) {
      media_search_select_filter(static_cast<espcontrol::media::SearchFilter>(
        reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))));
    }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
  }

  sui.list = lv_obj_create(panel);
  lv_obj_set_width(sui.list, LV_PCT(100));
  lv_obj_set_flex_grow(sui.list, 1);
  lv_obj_set_style_bg_opa(sui.list, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(sui.list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(sui.list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(sui.list, 8, LV_PART_MAIN);
  lv_obj_set_layout(sui.list, LV_LAYOUT_FLEX);
  lv_obj_set_style_flex_flow(sui.list, LV_FLEX_FLOW_COLUMN, LV_PART_MAIN);
  lv_obj_add_flag(sui.list, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(sui.list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(sui.list, LV_SCROLLBAR_MODE_OFF);
  lv_obj_add_event_cb(sui.list, [](lv_event_t *) {
    media_control_modal_ui().library_last_scroll_ms = esphome::millis();
  }, LV_EVENT_SCROLL, nullptr);

  // The status floats over the top of the list.
  sui.status_lbl = lv_label_create(panel);
  lv_obj_add_flag(sui.status_lbl, LV_OBJ_FLAG_FLOATING);
  lv_obj_set_width(sui.status_lbl, LV_PCT(100));
  lv_obj_align(sui.status_lbl, LV_ALIGN_TOP_MID, 0, (bar_h + gap) * 2);
  lv_obj_set_style_text_align(sui.status_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_style_text_color(sui.status_lbl, lv_color_hex(current_theme().text_muted), LV_PART_MAIN);
  lv_obj_set_style_pad_ver(sui.status_lbl, 8, LV_PART_MAIN);
  if (ctx->label_font) lv_obj_set_style_text_font(sui.status_lbl, ctx->label_font, LV_PART_MAIN);
  lv_obj_add_flag(sui.status_lbl, LV_OBJ_FLAG_HIDDEN);
}

inline void media_control_create_search_tab_content(MediaControlCtx *ctx) {
  MediaControlModalUi &ui = media_control_modal_ui();
  MediaSearchUi &sui = media_search_ui();
  if (!ctx || !ui.content_box || sui.keyboard) return;
  const ControlModalLayout layout = control_modal_calc_layout(ctx->width_compensation_percent);
  lv_obj_set_layout(ui.content_box, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(ui.content_box, LV_FLEX_FLOW_COLUMN);
  const lv_coord_t gap = control_modal_scaled_px(10, layout.short_side);
  const lv_coord_t pad_x = control_modal_scaled_px(18, layout.short_side);
  lv_coord_t bar_h = control_modal_scaled_px(60, layout.short_side);
  if (bar_h < 48) bar_h = 48;

  sui.keyboard = media_search_create_panel(ui.content_box, gap);
  sui.results = media_search_create_panel(ui.content_box, gap);
  media_search_create_keyboard(ctx, gap, pad_x);
  media_search_create_results(ctx, gap, pad_x, bar_h);
  ui.library_timer = lv_timer_create(media_search_timer_cb, 500, nullptr);

  MediaSearchState &state = media_search_state_for(ctx);
  if (state.searched.empty()) state.editing = true;
  media_search_fill_list(ctx);
  media_search_render(ctx);
}
