#pragma once

// Music Assistant library browsing for the media control modal. The panel asks
// Home Assistant for one page of playlists, artists, or radio stations at a
// time with music_assistant.get_library. Home Assistant renders the response
// into compact "name<TAB>uri" lines, so the firmware never parses the large
// JSON library payload itself.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

namespace espcontrol::media {

enum class LibraryKind : uint8_t {
  PLAYLIST = 0,
  ARTIST = 1,
  RADIO = 2,
};

constexpr size_t LIBRARY_KIND_COUNT = 3;
constexpr size_t LIBRARY_PAGE_SIZE = 50;
// After the first page the rest of the library loads in larger pages, so the
// letter keys know every entry after a few round trips.
constexpr size_t LIBRARY_BULK_PAGE_SIZE = 200;
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

// Library entries live in PSRAM. Plain allocations on the panels only use
// the small internal heap, where a few thousand entries do not fit.
template<typename T>
struct LibraryAllocator {
  using value_type = T;
  LibraryAllocator() noexcept = default;
  template<typename U>
  LibraryAllocator(const LibraryAllocator<U> &) noexcept {}

  T *allocate(size_t count) {
    const size_t bytes = count * sizeof(T);
    void *result = nullptr;
#ifdef ESP_PLATFORM
    result = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
    if (result == nullptr) result = std::malloc(bytes);
    if (result == nullptr && bytes != 0) std::abort();
    return static_cast<T *>(result);
  }
  void deallocate(T *pointer, size_t) noexcept { std::free(pointer); }

  template<typename U>
  bool operator==(const LibraryAllocator<U> &) const noexcept { return true; }
  template<typename U>
  bool operator!=(const LibraryAllocator<U> &) const noexcept { return false; }
};

using LibraryString = std::basic_string<char, std::char_traits<char>, LibraryAllocator<char>>;

struct LibraryItem {
  LibraryString name;
  LibraryString uri;
  // Sort name built from the shown name; alphabetical lists are ordered by it.
  LibraryString sort;
};

using LibraryItems = std::vector<LibraryItem, LibraryAllocator<LibraryItem>>;

// Which part of a library a tab shows. Recent is ordered by last play, so it
// has no letter keys and stops after one page.
enum class LibraryFilter : uint8_t {
  ALL = 0,
  FAVORITES = 1,
  RECENT = 2,
};

constexpr size_t LIBRARY_FILTER_COUNT = 3;

inline const char *library_order_by(LibraryFilter filter) {
  return filter == LibraryFilter::RECENT ? "last_played_desc" : "sort_name";
}

inline bool library_filter_alphabetical(LibraryFilter filter) {
  return filter != LibraryFilter::RECENT;
}

// Loaded entries for one library kind of one media player.
struct LibraryList {
  LibraryItems items;
  // Entries received so far, in Music Assistant's order: the next page's
  // offset, and the sort name of the last one, which tells how far into the
  // alphabet loading has got.
  size_t fetched = 0;
  LibraryString last_fetched;
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

// Plain letter for an accented Latin-1 letter, given the UTF-8 byte after
// 0xC3 (U+00C0..U+00FF); 0 when the character has no plain form.
inline char library_plain_latin1(unsigned char second) {
  static constexpr std::string_view MAP =
    "aaaaaa0ceeeeiiii"  // U+00C0
    "0nooooo00uuuuy00"  // U+00D0
    "aaaaaa0ceeeeiiii"  // U+00E0
    "0nooooo00uuuuy0y"; // U+00F0
  if (second < 0x80 || second > 0xBF) return 0;
  const char ch = MAP[second - 0x80];
  return ch == '0' ? 0 : ch;
}

// Mirrors Music Assistant's create_sort_name: lower case, accents dropped,
// leading punctuation removed, and a leading article such as "the " moved to
// the end. Music Assistant may sort an artist by a different name (Tori Amos
// as "Amos, Tori"), so the panel orders alphabetical lists by this value
// itself; letter keys then always match the names on screen.
inline LibraryString library_sort_name(std::string_view name) {
  LibraryString out;
  out.reserve(name.size() + 6);
  for (size_t i = 0; i < name.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(name[i]);
    if (ch == 0xC3 && i + 1 < name.size()) {
      const unsigned char second = static_cast<unsigned char>(name[i + 1]);
      const char plain = library_plain_latin1(second);
      // Capitals without a plain form (such as Æ) still become lower case.
      const bool capital = second >= 0x80 && second <= 0x9E && second != 0x97;
      out.push_back(static_cast<char>(ch));
      if (plain) out.back() = plain;
      else out.push_back(static_cast<char>(capital ? second + 0x20 : second));
      ++i;
      continue;
    }
    out.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : static_cast<char>(ch));
  }
  const auto is_space = [](char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; };
  while (!out.empty() && is_space(out.back())) out.pop_back();
  size_t start = 0;
  while (start < out.size() && is_space(out[start])) ++start;
  static constexpr std::string_view LEADING = ",.:!?([{<>)]}/`'\"";
  while (start < out.size() && LEADING.find(out[start]) != std::string_view::npos) ++start;
  out.erase(0, start);
  static constexpr std::string_view ARTICLES[] = {
    "the ", "de ", "les ", "dj ", "las ", "los ", "le ", "la ", "el ", "a ", "an "};
  for (std::string_view article : ARTICLES) {
    if (std::string_view(out).substr(0, article.size()) == article) {
      out.erase(0, article.size());
      out += ", ";
      out.append(article.data(), article.size());
      break;
    }
  }
  size_t begin = 0;
  while (begin < out.size() && is_space(out[begin])) ++begin;
  size_t end = out.size();
  while (end > begin && is_space(out[end - 1])) --end;
  return out.substr(begin, end - begin);
}

// Appends the entries of one rendered page. Lines without a URI are skipped.
// Returns how many lines the page contained, including skipped ones, so the
// caller can tell whether Music Assistant returned a full page.
inline size_t library_parse_page(const char *payload, LibraryItems &out,
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
        LibraryString name(line, tab - line);
        LibraryString uri(tab + 1, end - tab - 1);
        while (!uri.empty() && (uri.back() == '\r' || uri.back() == ' ')) uri.pop_back();
        const size_t first = name.find_first_not_of(' ');
        name = first == LibraryString::npos ? LibraryString() : name.substr(first);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (!uri.empty() && out.size() < max_items) {
          LibraryString shown = name.empty() ? uri : name;
          LibraryString sort = library_sort_name(shown);
          out.push_back({std::move(shown), std::move(uri), std::move(sort)});
        }
      }
    }
    line = *end ? end + 1 : end;
  }
  return lines;
}

// Home Assistant trims the rendered template, so a page shorter than the one
// requested means the end of the library. Loading also stops at the cap.
inline bool library_page_complete(size_t page_lines, size_t page_size, size_t loaded,
                                  size_t max_items) {
  return page_lines < page_size || loaded >= max_items;
}

inline bool library_should_load_more(const LibraryList &list, size_t max_items) {
  return !list.loading && !list.complete && !list.failed && list.items.size() < max_items;
}

// First item index shown at a scroll offset for fixed-pitch rows.
inline size_t library_first_visible(int scroll_y, int row_pitch, size_t count) {
  if (row_pitch <= 0 || count == 0 || scroll_y <= 0) return 0;
  const size_t first = static_cast<size_t>(scroll_y / row_pitch);
  return first >= count ? count - 1 : first;
}

// Letter keys for jumping through a library. Keys are '#' (anything that is
// not a letter) and 'A'..'Z'. Tapping several keys in a row builds a prefix
// such as "AN", matched against the start of each entry's sort name.
constexpr const char *LIBRARY_INDEX_KEYS = "#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
constexpr size_t LIBRARY_INDEX_KEY_COUNT = 27;
constexpr size_t LIBRARY_PREFIX_MAX = 12;
// Typed letters are forgotten this long after the last key tap.
constexpr uint32_t LIBRARY_PREFIX_TIMEOUT_MS = 3000;

// Position in key order: '#' and other non-letters first, then A..Z.
inline int library_key_rank(char ch) {
  if (ch >= 'a' && ch <= 'z') return ch - 'a' + 1;
  if (ch >= 'A' && ch <= 'Z') return ch - 'A' + 1;
  return 0;
}

inline char library_key_for_rank(int rank) {
  return rank <= 0 ? '#' : static_cast<char>('A' + rank - 1);
}

// Compares the start of a sort name with a typed prefix in key order:
// negative if the entry comes before every match, 0 if it matches, positive
// if it comes after.
inline int library_prefix_compare(std::string_view sort, std::string_view prefix) {
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (i >= sort.size()) return -1;
    const int diff = library_key_rank(sort[i]) - library_key_rank(prefix[i]);
    if (diff != 0) return diff;
  }
  return 0;
}

// First loaded entry at or after the prefix, or -1 if every loaded entry
// comes before it (more pages may still need loading).
inline int library_prefix_find(const LibraryItems &items, std::string_view prefix) {
  for (size_t i = 0; i < items.size(); ++i) {
    if (library_prefix_compare(items[i].sort, prefix) >= 0) return static_cast<int>(i);
  }
  return -1;
}

// Keys that can follow the prefix, as bits in LIBRARY_INDEX_KEYS order. While
// the list is still loading, keys at or after the last received entry stay
// available because their entries may be in later pages.
inline uint32_t library_next_keys(const LibraryItems &items, std::string_view prefix, bool complete,
                                  std::string_view last_fetched) {
  uint32_t keys = 0;
  for (const LibraryItem &item : items) {
    if (item.sort.size() <= prefix.size() || library_prefix_compare(item.sort, prefix) != 0) continue;
    keys |= 1u << library_key_rank(item.sort[prefix.size()]);
  }
  if (complete) return keys;
  std::string candidate(prefix);
  candidate.push_back('#');
  for (size_t rank = 0; rank < LIBRARY_INDEX_KEY_COUNT; ++rank) {
    candidate.back() = library_key_for_rank(static_cast<int>(rank));
    if (items.empty() || library_prefix_compare(last_fetched, candidate) <= 0) keys |= 1u << rank;
  }
  return keys;
}

// Order used for alphabetical lists, consistent with the letter keys:
// characters compare by key first ('#' group, then A..Z), then by byte.
inline bool library_sort_less(const LibraryItem &a, const LibraryItem &b) {
  const size_t n = std::min(a.sort.size(), b.sort.size());
  for (size_t i = 0; i < n; ++i) {
    const int diff = library_key_rank(a.sort[i]) - library_key_rank(b.sort[i]);
    if (diff != 0) return diff < 0;
    const unsigned char ca = static_cast<unsigned char>(a.sort[i]);
    const unsigned char cb = static_cast<unsigned char>(b.sort[i]);
    if (ca != cb) return ca < cb;
  }
  if (a.sort.size() != b.sort.size()) return a.sort.size() < b.sort.size();
  return a.uri < b.uri;
}

inline void library_sort(LibraryItems &items) {
  std::sort(items.begin(), items.end(), library_sort_less);
}

// How far into the alphabet a page reached: the lowest sort name among its
// last few entries, so one entry Music Assistant filed elsewhere (Tori Amos
// among the A's) does not make loading look further along than it is.
inline LibraryString library_page_progress(const LibraryItems &items, size_t page_start) {
  constexpr size_t SAMPLE = 5;
  const size_t from = items.size() - page_start > SAMPLE ? items.size() - SAMPLE : page_start;
  size_t lowest = items.size() - 1;
  for (size_t i = from; i < items.size(); ++i) {
    if (library_sort_less(items[i], items[lowest])) lowest = i;
  }
  return items[lowest].sort;
}

// True once loading has gone past every entry that can start with the
// prefix. Entries Music Assistant files elsewhere can still turn up later,
// but they are rare, so a jump does not wait for the whole library.
inline bool library_loaded_past(const LibraryList &list, std::string_view prefix) {
  return list.complete || (list.fetched > 0 && library_prefix_compare(list.last_fetched, prefix) > 0);
}

}  // namespace espcontrol::media
