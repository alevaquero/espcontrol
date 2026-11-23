#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include "media_search.h"

using namespace espcontrol::media;

int main() {
  std::vector<SearchItem> items;
  search_parse_results(
    "artist\tQueen\t\tlibrary://artist/1\n"
    "album\tNews of the World\tQueen\tspotify://album/2\n"
    "track\tWe Are The Champions\tQueen - News of the World\tspotify://track/3\r\n"
    "track\tNo uri\tQueen\t\n"
    "podcast\tUnknown kind\t\tx://4\n"
    "broken line\n"
    "radio\t\t\ttunein://radio/5",
    items);
  assert(items.size() == 4);
  assert(items[0].kind == SearchKind::ARTIST && items[0].name == "Queen" && items[0].detail.empty());
  assert(items[2].kind == SearchKind::TRACK && items[2].uri == "spotify://track/3");
  assert(items[2].detail == "Queen - News of the World");
  assert(items[3].kind == SearchKind::RADIO && items[3].name == "tunein://radio/5");

  assert(search_filter_count(items, SearchFilter::ALL) == 4);
  assert(search_filter_count(items, SearchFilter::TRACK) == 1);
  assert(search_filter_count(items, SearchFilter::PLAYLIST) == 0);
  assert(search_filter_matches(SearchFilter::ALBUM, SearchKind::ALBUM));
  assert(!search_filter_matches(SearchFilter::ALBUM, SearchKind::TRACK));
  assert(std::strcmp(search_kind_media_type(SearchKind::PLAYLIST), "playlist") == 0);
  for (const char *row : SEARCH_KEY_ROWS) assert(std::strlen(row) <= SEARCH_KEY_ROW_WIDTH);
  return 0;
}
