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
    {"up", "Remote Up", RemoteKeyGroup::DPAD, 1},
    {"lt", "Remote Left", RemoteKeyGroup::DPAD, 3},
    {"ok", "Remote OK", RemoteKeyGroup::DPAD, 4},
    {"rt", "Remote Right", RemoteKeyGroup::DPAD, 5},
    {"dn", "Remote Down", RemoteKeyGroup::DPAD, 7},
    {"vu", "Volume Plus", RemoteKeyGroup::VOLUME, 0},
    {"mt", "Volume Mute", RemoteKeyGroup::VOLUME, 1},
    {"vd", "Volume Minus", RemoteKeyGroup::VOLUME, 2},
    {"bk", "Remote Back", RemoteKeyGroup::ROW, 0},
    {"hm", "Remote Home", RemoteKeyGroup::ROW, 0},
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
  const int cluster_gaps = cluster_cols > 0 ? cluster_cols - 1 + (has_pad && has_volume ? 1 : 0) : 0;
  const int cluster_rows = cluster_cols > 0 ? 3 : 0;

  auto consider = [&](bool beside, int cols, int lines) {
    int total_cols = cluster_cols;
    int total_col_gaps = cluster_gaps;
    int total_rows = cluster_rows;
    int total_row_gaps = cluster_rows > 0 ? cluster_rows - 1 : 0;
    if (lines > 0) {
      if (beside) {
        total_cols += cols;
        total_col_gaps += (cols - 1) + (cluster_cols > 0 ? 2 : 0);
        if (lines > total_rows) {
          total_row_gaps = lines - 1;
          total_rows = lines;
        }
      } else {
        if (cols > total_cols) {
          total_cols = cols;
          total_col_gaps = cols - 1;
        }
        total_rows += lines;
        total_row_gaps += (lines - 1) + (cluster_rows > 0 ? 2 : 0);
      }
    }
    if (total_cols <= 0 || total_rows <= 0) return;
    const lv_coord_t by_width = (width - total_col_gaps * gap) / total_cols;
    const lv_coord_t by_height = (height - total_row_gaps * gap) / total_rows;
    const lv_coord_t key = std::min(by_width, by_height);
    if (key > best.key) {
      best.key = key;
      best.row_beside = beside;
      best.row_cols = cols;
      best.row_lines = lines;
    }
  };

  if (row_count <= 0) {
    consider(false, 0, 0);
  } else {
    for (int cols = 1; cols <= row_count; ++cols) {
      const int lines = (row_count + cols - 1) / cols;
      consider(false, cols, lines);
      if (cluster_cols > 0) consider(true, cols, lines);
    }
  }
  lv_coord_t max_key = control_modal_scaled_px(150, short_side);
  if (best.key > max_key) best.key = max_key;
  if (best.key < 24) best.key = 24;
  return best;
}

inline void remote_layout_modal(const ControlModalLayout &layout) {
  RemoteModalUi &ui = remote_modal_ui();
  if (!ui.panel) return;
  lv_coord_t top_gap = control_modal_scaled_px(24, layout.short_side);
  if (top_gap < 10) top_gap = 10;
  const lv_coord_t content_top = layout.back_inset_y + layout.back_size + top_gap;
  const lv_coord_t content_left = layout.inset;
  const lv_coord_t content_w = layout.panel_w - layout.inset * 2;
  const lv_coord_t content_h = layout.panel_h - layout.inset - content_top;

  bool has_pad = false, has_volume = false;
  int row_count = 0;
  for (const RemoteModalKey &key : ui.keys) {
    if (key.def->group == RemoteKeyGroup::DPAD) has_pad = true;
    else if (key.def->group == RemoteKeyGroup::VOLUME) has_volume = true;
    else ++row_count;
  }
  const RemoteModalGeometry geometry = remote_modal_geometry(
    content_w, content_h, layout.short_side, has_pad, has_volume, row_count);
  const lv_coord_t k = geometry.key;
  const lv_coord_t g = geometry.gap;
  const lv_coord_t cluster_gap = g * 3;

  const lv_coord_t pad_w = has_pad ? k * 3 + g * 2 : 0;
  const lv_coord_t volume_w = has_volume ? k : 0;
  const lv_coord_t cluster_w = pad_w + volume_w + (has_pad && has_volume ? cluster_gap : 0);
  const lv_coord_t cluster_h = (has_pad || has_volume) ? k * 3 + g * 2 : 0;
  const lv_coord_t row_w = geometry.row_cols > 0 ? k * geometry.row_cols + g * (geometry.row_cols - 1) : 0;
  const lv_coord_t row_h = geometry.row_lines > 0 ? k * geometry.row_lines + g * (geometry.row_lines - 1) : 0;
  const lv_coord_t between = (cluster_w > 0 && row_w > 0) ? cluster_gap : 0;
  const lv_coord_t total_w = geometry.row_beside ? cluster_w + between + row_w : std::max(cluster_w, row_w);
  const lv_coord_t total_h = geometry.row_beside ? std::max(cluster_h, row_h) : cluster_h + between + row_h;
  const lv_coord_t origin_x = content_left + (content_w - total_w) / 2;
  lv_coord_t origin_y = content_top + (content_h - total_h) / 2;
  if (origin_y < content_top) origin_y = content_top;

  const lv_coord_t cluster_x = geometry.row_beside ? origin_x : origin_x + (total_w - cluster_w) / 2;
  const lv_coord_t cluster_y = geometry.row_beside ? origin_y + (total_h - cluster_h) / 2 : origin_y;
  const lv_coord_t row_x = geometry.row_beside ? origin_x + cluster_w + between : origin_x + (total_w - row_w) / 2;
  const lv_coord_t row_y = geometry.row_beside ? origin_y + (total_h - row_h) / 2 : origin_y + cluster_h + between;

  int row_index = 0;
  for (RemoteModalKey &key : ui.keys) {
    if (!key.btn) continue;
    lv_coord_t x = 0, y = 0;
    if (key.def->group == RemoteKeyGroup::DPAD) {
      x = cluster_x + (key.def->position % 3) * (k + g);
      y = cluster_y + (key.def->position / 3) * (k + g);
    } else if (key.def->group == RemoteKeyGroup::VOLUME) {
      x = cluster_x + pad_w + (has_pad ? cluster_gap : 0);
      y = cluster_y + key.def->position * (k + g);
    } else {
      const int cols = std::max(1, geometry.row_cols);
      x = row_x + (row_index % cols) * (k + g);
      y = row_y + (row_index / cols) * (k + g);
      ++row_index;
    }
    lv_obj_set_size(key.btn, k, k);
    lv_obj_set_style_radius(key.btn, k / 2, LV_PART_MAIN);
    lv_obj_set_pos(key.btn, x, y);
  }
  if (ui.back_btn) lv_obj_move_foreground(ui.back_btn);
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
  for (size_t i = 0; i < ui.keys.size(); ++i) {
    RemoteModalKey &key = ui.keys[i];
    key.btn = control_modal_create_round_button(
      ui.panel, 48, find_icon(key.def->icon), remote_icon_font_ref(),
      theme_display_color(current_theme().border), surface,
      icon_width_compensation_percent());
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
