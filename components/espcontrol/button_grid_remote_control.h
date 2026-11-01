#pragma once

// Remote card modal. Each key sends remote.send_command with the command text
// saved in the card options; keys without a command are not shown.

enum class RemoteKeyGroup : uint8_t {
  DPAD = 0,
  VOLUME = 1,
  ROW = 2,
};

struct RemoteKeyDef {
  const char *option;
  const char *icon;
  RemoteKeyGroup group;
  uint8_t position;  // Cell in the 3x3 pad, or row in the volume column.
};

inline const RemoteKeyDef *remote_key_defs(size_t *count) {
  static const RemoteKeyDef defs[] = {
    {"up", "Chevron Up", RemoteKeyGroup::DPAD, 1},
    {"lt", "Chevron Left", RemoteKeyGroup::DPAD, 3},
    {"ok", "Check", RemoteKeyGroup::DPAD, 4},
    {"rt", "Chevron Right", RemoteKeyGroup::DPAD, 5},
    {"dn", "Chevron Down", RemoteKeyGroup::DPAD, 7},
    {"vu", "Volume Plus", RemoteKeyGroup::VOLUME, 0},
    {"mt", "Volume Mute", RemoteKeyGroup::VOLUME, 1},
    {"vd", "Volume Minus", RemoteKeyGroup::VOLUME, 2},
    {"bk", "Remote Back", RemoteKeyGroup::ROW, 0},
    {"hm", "Home", RemoteKeyGroup::ROW, 0},
    {"mn", "Menu", RemoteKeyGroup::ROW, 0},
    {"rw", "Rewind", RemoteKeyGroup::ROW, 0},
    {"pp", "Play Pause", RemoteKeyGroup::ROW, 0},
    {"ff", "Fast Forward", RemoteKeyGroup::ROW, 0},
    {"pw", "Power", RemoteKeyGroup::ROW, 0},
  };
  if (count) *count = sizeof(defs) / sizeof(defs[0]);
  return defs;
}

inline const RemoteKeyDef *remote_key_def(const std::string &option) {
  size_t count = 0;
  const RemoteKeyDef *defs = remote_key_defs(&count);
  for (size_t i = 0; i < count; ++i) {
    if (option == defs[i].option) return &defs[i];
  }
  return nullptr;
}

// Saved "ro" order for the button row, falling back to the default order and
// appending any row key the saved order leaves out.
inline std::vector<std::string> remote_row_order(const std::string &saved) {
  static const char *const defaults[] = {"bk", "hm", "mn", "rw", "pp", "ff", "pw"};
  std::vector<std::string> order;
  size_t start = 0;
  while (start <= saved.size() && !saved.empty()) {
    size_t end = saved.find('|', start);
    if (end == std::string::npos) end = saved.size();
    std::string key = saved.substr(start, end - start);
    const RemoteKeyDef *def = remote_key_def(key);
    if (def && def->group == RemoteKeyGroup::ROW &&
        std::find(order.begin(), order.end(), key) == order.end()) {
      order.push_back(key);
    }
    start = end + 1;
  }
  for (const char *key : defaults) {
    if (std::find(order.begin(), order.end(), key) == order.end()) order.push_back(key);
  }
  return order;
}

struct RemoteModalKey {
  lv_obj_t *btn = nullptr;
  const RemoteKeyDef *def = nullptr;
  std::string command;
};

struct RemoteModalUi {
  lv_obj_t *overlay = nullptr;
  lv_obj_t *panel = nullptr;
  lv_obj_t *back_btn = nullptr;
  lv_obj_t *volume_bar = nullptr;  // Shared pill behind the volume keys.
  std::string entity;
  uint32_t generation = 0;
  std::vector<RemoteModalKey> keys;
};

inline RemoteModalUi &remote_modal_ui() {
  static RemoteModalUi ui;
  return ui;
}

inline const lv_font_t *&remote_icon_font_ref() {
  static const lv_font_t *font = nullptr;
  return font;
}

inline void set_remote_icon_font(const lv_font_t *font) {
  remote_icon_font_ref() = font;
}

inline void remote_hide_modal() {
  RemoteModalUi &ui = remote_modal_ui();
  control_modal_delete_overlay(ControlModalKind::REMOTE_CONTROL, ui.overlay);
  ui = RemoteModalUi();
}

inline bool remote_card_has_commands(const ParsedCfg &config) {
  size_t count = 0;
  const RemoteKeyDef *defs = remote_key_defs(&count);
  for (size_t i = 0; i < count; ++i) {
    if (!cfg_option_value(config.options, defs[i].option).empty()) return true;
  }
  return false;
}

inline void remote_send_key(size_t index) {
  RemoteModalUi &ui = remote_modal_ui();
  if (index >= ui.keys.size()) return;
  // A grid rebuild may have replaced this card while the modal stayed open.
  if (ui.generation != ha_subscription_generation()) {
    remote_hide_modal();
    return;
  }
  const RemoteModalKey &key = ui.keys[index];
  if (!ha_send_entity_action(ui.entity, "remote.send_command", "command",
                             key.command.c_str())) {
    ESP_LOGW("remote", "Unable to send remote command %s", key.def->option);
  }
}

struct RemoteModalGeometry {
  lv_coord_t key = 0;
  lv_coord_t gap = 0;
  bool row_beside = false;
  int row_cols = 0;
  int row_lines = 0;
};

// Fit the pad, volume column, and button row inside the content box. The row
// goes below the pad, or beside it in a grid when that allows larger keys.
inline RemoteModalGeometry remote_modal_geometry(lv_coord_t width, lv_coord_t height,
                                                 lv_coord_t short_side, bool has_pad,
                                                 bool has_volume, int row_count) {
  RemoteModalGeometry best;
  lv_coord_t gap = control_modal_scaled_px(14, short_side);
  if (gap < 6) gap = 6;
  best.gap = gap;
  const int cluster_cols = (has_pad ? 3 : 0) + (has_volume ? 1 : 0);
  // Gaps inside the pad, plus the wider pad-to-volume gap used by the placement.
  const int cluster_gaps = (has_pad ? 2 : 0) + (has_pad && has_volume ? 3 : 0);
  const int cluster_rows = cluster_cols > 0 ? 3 : 0;

  // Largest key that fits `count` keys and `gaps` gaps into `space`.
  auto key_for = [gap](lv_coord_t space, int count, int gaps) -> lv_coord_t {
    return count > 0 ? (space - gaps * gap) / count : 32767;
  };
  auto consider = [&](bool beside, int cols, int lines) {
    lv_coord_t key = 32767;
    const bool has_cluster = cluster_cols > 0;
    const bool has_row = lines > 0;
    if (beside && has_cluster && has_row) {
      key = std::min(key, key_for(width, cluster_cols + cols, cluster_gaps + 3 + (cols - 1)));
      key = std::min(key, key_for(height, std::max(cluster_rows, lines),
                                  std::max(cluster_rows, lines) - 1));
    } else {
      if (has_cluster) key = std::min(key, key_for(width, cluster_cols, cluster_gaps));
      if (has_row) key = std::min(key, key_for(width, cols, cols - 1));
      const int rows = (has_cluster ? cluster_rows : 0) + (has_row ? lines : 0);
      const int row_gaps = (has_cluster ? cluster_rows - 1 : 0) + (has_row ? lines - 1 : 0) +
                           (has_cluster && has_row ? 3 : 0);
      key = std::min(key, key_for(height, rows, row_gaps));
    }
    if (key == 32767) return;
    if (key > best.key) {
      best.key = key;
      best.row_beside = beside && has_cluster && has_row;
      best.row_cols = cols;
      best.row_lines = lines;
    }
  };

  if (row_count <= 0) {
    consider(false, 0, 0);
  } else {
    // Widest rows first so a tie keeps the buttons on as few lines as possible.
    for (int cols = row_count; cols >= 1; --cols) {
      const int lines = (row_count + cols - 1) / cols;
      consider(false, cols, lines);
      if (cluster_cols > 0) consider(true, cols, lines);
    }
  }
  lv_coord_t max_key = control_modal_scaled_px(120, short_side);
  if (best.key > max_key) best.key = max_key;
  if (best.key < 24) best.key = 24;
  return best;
}

struct RemoteModalPlacement {
  lv_coord_t left = 0;
  lv_coord_t top = 0;
  lv_coord_t pad_w = 0;
  lv_coord_t cluster_gap = 0;
  lv_coord_t cluster_x = 0;
  lv_coord_t cluster_y = 0;
  lv_coord_t row_x = 0;
  lv_coord_t row_y = 0;
};

inline RemoteModalPlacement remote_modal_place(
    const RemoteModalGeometry &geometry, bool has_pad, bool has_volume,
    lv_coord_t content_left, lv_coord_t content_top,
    lv_coord_t content_w, lv_coord_t content_h) {
  const lv_coord_t k = geometry.key;
  const lv_coord_t g = geometry.gap;
  RemoteModalPlacement placement;
  placement.cluster_gap = g * 3;
  placement.pad_w = has_pad ? k * 3 + g * 2 : 0;
  const lv_coord_t volume_w = has_volume ? k : 0;
  const lv_coord_t cluster_w = placement.pad_w + volume_w +
    (has_pad && has_volume ? placement.cluster_gap : 0);
  const lv_coord_t cluster_h = (has_pad || has_volume) ? k * 3 + g * 2 : 0;
  const lv_coord_t row_w = geometry.row_cols > 0 ? k * geometry.row_cols + g * (geometry.row_cols - 1) : 0;
  const lv_coord_t row_h = geometry.row_lines > 0 ? k * geometry.row_lines + g * (geometry.row_lines - 1) : 0;
  const bool beside = geometry.row_beside;
  const lv_coord_t between = (cluster_w > 0 && row_w > 0) ? placement.cluster_gap : 0;
  const lv_coord_t total_w = beside ? cluster_w + between + row_w : std::max(cluster_w, row_w);
  const lv_coord_t total_h = beside ? std::max(cluster_h, row_h) : cluster_h + between + row_h;
  placement.left = content_left + (content_w - total_w) / 2;
  placement.top = std::max<lv_coord_t>(content_top, content_top + (content_h - total_h) / 2);
  placement.cluster_x = beside ? placement.left : placement.left + (total_w - cluster_w) / 2;
  placement.cluster_y = beside ? placement.top + (total_h - cluster_h) / 2 : placement.top;
  placement.row_x = beside ? placement.left + cluster_w + between : placement.left + (total_w - row_w) / 2;
  placement.row_y = beside ? placement.top + (total_h - row_h) / 2 : placement.top + cluster_h + between;
  return placement;
}

inline void remote_layout_modal(const ControlModalLayout &layout) {
  RemoteModalUi &ui = remote_modal_ui();
  if (!ui.panel) return;
  bool has_pad = false, has_volume = false;
  int row_count = 0;
  for (const RemoteModalKey &key : ui.keys) {
    if (key.def->group == RemoteKeyGroup::DPAD) has_pad = true;
    else if (key.def->group == RemoteKeyGroup::VOLUME) has_volume = true;
    else ++row_count;
  }

  // Keep the keys clear of the Back button either with equal side margins,
  // which keeps them vertically centered, or with a strip below it. Use
  // the strip only when the centered layout would make keys noticeably smaller.
  lv_coord_t back_gap = control_modal_scaled_px(24, layout.short_side);
  if (back_gap < 10) back_gap = 10;
  const lv_coord_t back_right = layout.back_inset_x + layout.back_size + back_gap;
  const lv_coord_t back_bottom = layout.back_inset_y + layout.back_size + back_gap;
  const lv_coord_t side_w = layout.panel_w - back_right * 2;
  const lv_coord_t side_h = layout.panel_h - layout.inset * 2;
  const lv_coord_t below_w = layout.panel_w - layout.inset * 2;
  const lv_coord_t below_h = layout.panel_h - layout.inset - back_bottom;
  const RemoteModalGeometry side = remote_modal_geometry(
    side_w, side_h, layout.short_side, has_pad, has_volume, row_count);
  const RemoteModalGeometry below = remote_modal_geometry(
    below_w, below_h, layout.short_side, has_pad, has_volume, row_count);
  const bool use_side = side_w > 0 && side.key * 4 >= below.key * 3;
  const RemoteModalGeometry geometry = use_side ? side : below;
  const RemoteModalPlacement placement = use_side
    ? remote_modal_place(geometry, has_pad, has_volume, back_right, layout.inset, side_w, side_h)
    : remote_modal_place(geometry, has_pad, has_volume, layout.inset, back_bottom, below_w, below_h);

  const lv_coord_t k = geometry.key;
  const lv_coord_t g = geometry.gap;
  int row_index = 0;
  for (RemoteModalKey &key : ui.keys) {
    if (!key.btn) continue;
    lv_coord_t x = 0, y = 0;
    if (key.def->group == RemoteKeyGroup::DPAD) {
      const lv_coord_t pad_x = placement.cluster_x + (has_volume ? k + placement.cluster_gap : 0);
      x = pad_x + (key.def->position % 3) * (k + g);
      y = placement.cluster_y + (key.def->position / 3) * (k + g);
    } else if (key.def->group == RemoteKeyGroup::VOLUME) {
      // Volume keys sit inside the bar, so their position is bar-relative.
      x = 0;
      y = key.def->position * (k + g);
    } else {
      const int cols = std::max(1, geometry.row_cols);
      x = placement.row_x + (row_index % cols) * (k + g);
      y = placement.row_y + (row_index / cols) * (k + g);
      ++row_index;
    }
    lv_obj_set_size(key.btn, k, k);
    lv_obj_set_style_radius(key.btn, k / 2, LV_PART_MAIN);
    lv_obj_set_pos(key.btn, x, y);
  }
  if (ui.volume_bar) {
    lv_obj_set_size(ui.volume_bar, k, k * 3 + g * 2);
    lv_obj_set_style_radius(ui.volume_bar, k / 2, LV_PART_MAIN);
    lv_obj_set_pos(ui.volume_bar, placement.cluster_x, placement.cluster_y);
  }
  if (ui.back_btn) lv_obj_move_foreground(ui.back_btn);
}

inline lv_obj_t *remote_create_volume_bar(lv_obj_t *panel, uint32_t surface) {
  lv_obj_t *bar = lv_obj_create(panel);
  if (!bar) return nullptr;
  lv_obj_set_style_bg_color(bar, lv_color_hex(surface), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(bar, 0, LV_PART_MAIN);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  apply_width_compensation(bar, icon_width_compensation_percent());
  return bar;
}

// A transparent key inside the volume bar; pressing it tints just that part.
inline lv_obj_t *remote_create_volume_key(lv_obj_t *bar, const char *icon) {
  lv_obj_t *btn = lv_btn_create(bar);
  if (!btn) return nullptr;
  lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN);
  const lv_style_selector_t pressed =
    static_cast<lv_style_selector_t>(LV_PART_MAIN) | static_cast<lv_style_selector_t>(LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(theme_display_color(current_theme().border)), pressed);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, pressed);
  apply_push_button_transition(btn);
  lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *label = lv_label_create(btn);
  if (label) {
    lv_label_set_display_text(label, icon);
    lv_obj_set_style_text_color(label, lv_color_hex(current_theme().text_primary), LV_PART_MAIN);
    if (remote_icon_font_ref()) lv_obj_set_style_text_font(label, remote_icon_font_ref(), LV_PART_MAIN);
    lv_obj_center(label);
  }
  return btn;
}

inline void remote_open_modal(const ParsedCfg &config, lv_obj_t *owner) {
  if (config.entity.empty()) {
    ESP_LOGW("remote", "Remote card has no remote entity");
    return;
  }
  ControlModalShell shell = control_modal_open_shell(
    ControlModalKind::REMOTE_CONTROL, owner, 100, remote_icon_font_ref(), remote_hide_modal);
  if (!shell.overlay || !shell.panel || !shell.close_btn) return;
  if (!config.label.empty()) set_clock_bar_modal_label(config.label);

  RemoteModalUi &ui = remote_modal_ui();
  ui.overlay = shell.overlay;
  ui.panel = shell.panel;
  ui.back_btn = shell.close_btn;
  ui.entity = config.entity;
  ui.generation = ha_subscription_generation();

  std::vector<const RemoteKeyDef *> ordered;
  size_t count = 0;
  const RemoteKeyDef *defs = remote_key_defs(&count);
  for (size_t i = 0; i < count; ++i) {
    if (defs[i].group != RemoteKeyGroup::ROW) ordered.push_back(&defs[i]);
  }
  for (const std::string &option : remote_row_order(cfg_option_value(config.options, "ro"))) {
    ordered.push_back(remote_key_def(option));
  }
  for (const RemoteKeyDef *def : ordered) {
    std::string command = cfg_option_value(config.options, def->option);
    if (command.empty()) continue;
    RemoteModalKey key;
    key.def = def;
    key.command = command;
    ui.keys.push_back(key);
  }

  const uint32_t surface = theme_display_color(current_theme().surface_primary);
  for (const RemoteModalKey &key : ui.keys) {
    if (key.def->group == RemoteKeyGroup::VOLUME && !ui.volume_bar) {
      ui.volume_bar = remote_create_volume_bar(ui.panel, surface);
    }
  }
  for (size_t i = 0; i < ui.keys.size(); ++i) {
    RemoteModalKey &key = ui.keys[i];
    if (key.def->group == RemoteKeyGroup::VOLUME) {
      key.btn = ui.volume_bar ? remote_create_volume_key(ui.volume_bar, find_icon(key.def->icon)) : nullptr;
    } else {
      key.btn = control_modal_create_round_button(
        ui.panel, 48, find_icon(key.def->icon), remote_icon_font_ref(),
        theme_display_color(current_theme().border), surface,
        icon_width_compensation_percent());
    }
    if (!key.btn) continue;
    if (std::strcmp(key.def->option, "ok") == 0) {
      lv_obj_set_style_bg_color(key.btn, lv_color_hex(DEFAULT_ACCENT_COLOR), LV_PART_MAIN);
    }
    lv_obj_add_event_cb(key.btn, [](lv_event_t *event) {
      remote_send_key(reinterpret_cast<uintptr_t>(lv_event_get_user_data(event)));
    }, LV_EVENT_CLICKED, reinterpret_cast<void *>(static_cast<uintptr_t>(i)));
  }
  remote_layout_modal(shell.layout);
}
