#pragma once

// Music Assistant search for the media control modal: result kinds, filters,
// keyboard rows, the response template, and the parser for its output.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace espcontrol::media {

enum class SearchKind : uint8_t { ARTIST, ALBUM, TRACK, PLAYLIST, RADIO };

struct SearchItem {
  SearchKind kind = SearchKind::TRACK;
  std::string name;
  std::string detail;  // Artist, and album for tracks.
  std::string uri;
};

// Filter buttons above the results; ALL shows every kind.
enum class SearchFilter : uint8_t { ALL, ARTIST, ALBUM, TRACK, PLAYLIST, RADIO };
constexpr size_t SEARCH_FILTER_COUNT = 6;

inline const char *search_filter_label(SearchFilter filter) {
  switch (filter) {
    case SearchFilter::ALL: return "All";
    case SearchFilter::ARTIST: return "Artists";
    case SearchFilter::ALBUM: return "Albums";
    case SearchFilter::TRACK: return "Tracks";
    case SearchFilter::PLAYLIST: return "Playlists";
    case SearchFilter::RADIO: return "Radio";
  }
  return "All";
}

inline bool search_filter_matches(SearchFilter filter, SearchKind kind) {
  return filter == SearchFilter::ALL ||
         static_cast<uint8_t>(filter) == static_cast<uint8_t>(kind) + 1;
}

inline size_t search_filter_count(const std::vector<SearchItem> &items, SearchFilter filter) {
  size_t count = 0;
  for (const auto &item : items) count += search_filter_matches(filter, item.kind) ? 1 : 0;
  return count;
}

// Media type for music_assistant.play_media.
inline const char *search_kind_media_type(SearchKind kind) {
  switch (kind) {
    case SearchKind::ARTIST: return "artist";
    case SearchKind::ALBUM: return "album";
    case SearchKind::TRACK: return "track";
    case SearchKind::PLAYLIST: return "playlist";
    case SearchKind::RADIO: return "radio";
  }
  return "track";
}

inline const char *search_kind_icon_name(SearchKind kind) {
  switch (kind) {
    case SearchKind::ARTIST: return "Account Music";
    case SearchKind::ALBUM: return "Album";
    case SearchKind::TRACK: return "Music Note";
    case SearchKind::PLAYLIST: return "Playlist Music";
    case SearchKind::RADIO: return "Radio";
  }
  return "Music Note";
}

inline bool search_kind_from_token(const std::string &token, SearchKind &kind) {
  for (uint8_t i = 0; i <= static_cast<uint8_t>(SearchKind::RADIO); ++i) {
    const auto candidate = static_cast<SearchKind>(i);
    if (token == search_kind_media_type(candidate)) {
      kind = candidate;
      return true;
    }
  }
  return false;
}

// QWERTY rows under the query box; shorter rows are centred with spacers.
constexpr size_t SEARCH_KEY_ROW_WIDTH = 10;
constexpr size_t SEARCH_KEY_ROW_COUNT = 4;
constexpr const char *SEARCH_KEY_ROWS[SEARCH_KEY_ROW_COUNT] = {
  "1234567890", "QWERTYUIOP", "ASDFGHJKL'", "ZXCVBNM-&",
};

// Home Assistant renders the search response as one line per result, in the
// order artists, albums, tracks, playlists, radio. Tabs and line breaks in
// names are replaced so they cannot split a line.
inline const char *search_response_template() {
  return "{% set data = response if response is defined and response is mapping else {} %}"
         "{% for key, kind in [('artists','artist'),('albums','album'),('tracks','track'),"
         "('playlists','playlist'),('radio','radio')] %}"
         "{% for item in (data.get(key) or []) if item is mapping %}"
         "{% set ns = namespace(names=[]) %}"
         "{% for a in (item.get('artists') or []) %}"
         "{% set ns.names = ns.names + [((a.get('name') if a is mapping else a) or '') | string] %}"
         "{% endfor %}"
         "{% set album = item.get('album') %}"
         "{% set album_name = ((album.get('name') if album is mapping else '') or '') | string %}"
         "{% set detail = ns.names | reject('equalto', '') | join(', ') %}"
         "{% if kind == 'track' and album_name %}"
         "{% set detail = detail ~ (' - ' if detail else '') ~ album_name %}{% endif %}"
         "{{ kind }}\t{{ (item.get('name') or '') | string | replace('\\t', ' ') | replace('\\n', ' ') }}"
         "\t{{ detail | replace('\\t', ' ') | replace('\\n', ' ') }}"
         "\t{{ item.get('uri') or '' }}\n"
         "{% endfor %}{% endfor %}";
}

inline std::string search_trim(const std::string &value) {
  const size_t first = value.find_first_not_of(" \r");
  if (first == std::string::npos) return "";
  return value.substr(first, value.find_last_not_of(" \r") - first + 1);
}

// Appends one item per rendered line; lines without a known kind or a URI are
// skipped.
inline void search_parse_results(const char *payload, std::vector<SearchItem> &out) {
  if (!payload) return;
  const char *line = payload;
  while (*line) {
    const char *end = line;
    while (*end && *end != '\n') ++end;
    std::string fields[4];
    size_t field = 0;
    for (const char *p = line; p < end; ++p) {
      if (*p == '\t' && field < 3) ++field;
      else fields[field].push_back(*p);
    }
    SearchItem item;
    const std::string uri = search_trim(fields[3]);
    if (field == 3 && !uri.empty() && search_kind_from_token(search_trim(fields[0]), item.kind)) {
      item.name = search_trim(fields[1]);
      item.detail = search_trim(fields[2]);
      item.uri = uri;
      if (item.name.empty()) item.name = item.uri;
      out.push_back(std::move(item));
    }
    line = *end ? end + 1 : end;
  }
}

}  // namespace espcontrol::media
