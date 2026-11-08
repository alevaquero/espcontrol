#pragma once

// Music Assistant library browsing for the media control modal. The panel asks
// Home Assistant for one page of playlists, artists, or radio stations at a
// time with music_assistant.get_library. Home Assistant renders the response
// into compact "name<TAB>uri" lines, so the firmware never parses the large
// JSON library payload itself.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace espcontrol::media {

enum class LibraryKind : uint8_t {
  PLAYLIST = 0,
  ARTIST = 1,
  RADIO = 2,
};

constexpr size_t LIBRARY_KIND_COUNT = 3;
constexpr size_t LIBRARY_PAGE_SIZE = 50;
// Request the next page once the visible rows come this close to the end.
constexpr size_t LIBRARY_PREFETCH_ROWS = 10;
constexpr uint32_t LIBRARY_REQUEST_TIMEOUT_MS = 15000;
// Libraries are re-read when a tab is opened after this long.
constexpr uint32_t LIBRARY_STALE_MS = 10UL * 60UL * 1000UL;

inline const char *library_media_type(LibraryKind kind) {
  switch (kind) {
    case LibraryKind::PLAYLIST: return "playlist";
    case LibraryKind::ARTIST: return "artist";
    case LibraryKind::RADIO: return "radio";
  }
  return "playlist";
}

inline const char *library_icon_name(LibraryKind kind) {
  switch (kind) {
    case LibraryKind::PLAYLIST: return "Playlist Music";
    case LibraryKind::ARTIST: return "Account Music";
    case LibraryKind::RADIO: return "Radio";
  }
  return "Playlist Music";
}

struct LibraryItem {
  std::string name;
  std::string uri;
};

// Loaded entries for one library kind of one media player.
struct LibraryList {
  std::vector<LibraryItem> items;
  bool loading = false;
  bool complete = false;
  bool failed = false;
  uint32_t call_id = 0;
  uint32_t request_ms = 0;
  uint32_t loaded_ms = 0;
};

// Entity ids are inserted into a Home Assistant template, so only accept the
// characters Home Assistant itself allows in an entity id.
inline bool library_entity_id_safe(const std::string &entity_id) {
  const size_t dot = entity_id.find('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= entity_id.size()) return false;
  for (char ch : entity_id) {
    const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
                    ch == '_' || ch == '.';
    if (!ok) return false;
  }
  return entity_id.find('.', dot + 1) == std::string::npos;
}

// The Music Assistant config entry that owns the card's media player.
inline std::string library_config_entry_template(const std::string &entity_id) {
  return "{{ config_entry_id('" + entity_id + "') }}";
}

inline const char *library_response_template() {
  return "{% set data = response if response is defined and response is mapping else {} %}"
         "{% for item in data.get('items', []) %}"
         "{{ (item.get('name') or '') | string | replace('\\t', ' ') | replace('\\n', ' ') }}"
         "\t{{ item.get('uri') or '' }}\n"
         "{% endfor %}";
}

// Appends the entries of one rendered page. Lines without a URI are skipped.
// Returns how many lines the page contained, including skipped ones, so the
// caller can tell whether Music Assistant returned a full page.
inline size_t library_parse_page(const char *payload, std::vector<LibraryItem> &out,
                                 size_t max_items) {
  if (!payload) return 0;
  size_t lines = 0;
  const char *line = payload;
  while (*line) {
    const char *end = line;
    while (*end && *end != '\n') ++end;
    const char *tab = line;
    while (tab < end && *tab != '\t') ++tab;
    if (end > line) {
      ++lines;
      if (tab < end) {
        std::string name(line, tab - line);
        std::string uri(tab + 1, end - tab - 1);
        while (!uri.empty() && (uri.back() == '\r' || uri.back() == ' ')) uri.pop_back();
        const size_t first = name.find_first_not_of(' ');
        name = first == std::string::npos ? std::string() : name.substr(first);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (!uri.empty() && out.size() < max_items) {
          out.push_back({name.empty() ? uri : name, uri});
        }
      }
    }
    line = *end ? end + 1 : end;
  }
  return lines;
}

// Home Assistant trims the rendered template, so a short page means the end of
// the library. A page may also come back short when it hits the memory cap.
inline bool library_page_complete(size_t page_lines, size_t loaded, size_t max_items) {
  return page_lines < LIBRARY_PAGE_SIZE || loaded >= max_items;
}

inline bool library_should_fetch_more(const LibraryList &list, size_t last_visible_index,
                                      size_t max_items) {
  if (list.loading || list.complete || list.failed || list.items.size() >= max_items) return false;
  return last_visible_index + LIBRARY_PREFETCH_ROWS >= list.items.size();
}

// First item index shown at a scroll offset for fixed-pitch rows.
inline size_t library_first_visible(int scroll_y, int row_pitch, size_t count) {
  if (row_pitch <= 0 || count == 0 || scroll_y <= 0) return 0;
  const size_t first = static_cast<size_t>(scroll_y / row_pitch);
  return first >= count ? count - 1 : first;
}

// Letter index for jumping through a library. Keys are '#' (anything that
// does not start with a letter) and 'A'..'Z'. A leading "The " is skipped,
// matching how Music Assistant sorts names.
constexpr const char *LIBRARY_INDEX_KEYS = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";

inline char library_index_key(const std::string &name) {
  size_t start = name.find_first_not_of(' ');
  if (start == std::string::npos) return '#';
  if (name.size() > start + 4) {
    const char t = name[start], h = name[start + 1], e = name[start + 2];
    if ((t == 'T' || t == 't') && (h == 'H' || h == 'h') && (e == 'E' || e == 'e') &&
        name[start + 3] == ' ') {
      const size_t next = name.find_first_not_of(' ', start + 4);
      if (next != std::string::npos) start = next;
    }
  }
  const char ch = name[start];
  if (ch >= 'a' && ch <= 'z') return static_cast<char>(ch - 'a' + 'A');
  if (ch >= 'A' && ch <= 'Z') return ch;
  return '#';
}

// Position of a key in index order: '#' first, then A..Z.
inline int library_index_rank(char key) {
  return key >= 'A' && key <= 'Z' ? key - 'A' + 1 : 0;
}

// First loaded entry at or after the key, or -1 if every loaded entry comes
// before it (more pages may still need loading).
inline int library_index_find(const std::vector<LibraryItem> &items, char key) {
  const int target = library_index_rank(key);
  for (size_t i = 0; i < items.size(); ++i) {
    if (library_index_rank(library_index_key(items[i].name)) >= target) return static_cast<int>(i);
  }
  return -1;
}

}  // namespace espcontrol::media
