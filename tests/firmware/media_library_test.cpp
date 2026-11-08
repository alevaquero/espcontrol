#include <cassert>
#include <string>
#include <vector>
#include "media_library.h"

int main() {
  using namespace espcontrol::media;
  std::vector<LibraryItem> items;

  // Rendered pages are "name<TAB>uri" lines; Home Assistant trims the end.
  size_t lines = library_parse_page(
    "Morning Mix\tlibrary://playlist/1\n  Jazz  \tlibrary://playlist/2 \n"
    "no uri line\n\tlibrary://playlist/3\nBad\t\n", items, 100);
  assert(lines == 5);
  assert(items.size() == 3);
  assert(items[0].name == "Morning Mix" && items[0].uri == "library://playlist/1");
  assert(items[1].name == "Jazz" && items[1].uri == "library://playlist/2");
  // A missing name falls back to the URI so the row is never blank.
  assert(items[2].name == "library://playlist/3");
  assert(library_parse_page("", items, 100) == 0 && library_parse_page(nullptr, items, 100) == 0);

  // The memory cap stops appending but still counts the page.
  std::vector<LibraryItem> capped;
  assert(library_parse_page("a\tu1\nb\tu2\nc\tu3", capped, 2) == 3 && capped.size() == 2);
  assert(library_page_complete(3, 2, 2));
  assert(library_page_complete(LIBRARY_PAGE_SIZE - 1, 10, 1000));
  assert(!library_page_complete(LIBRARY_PAGE_SIZE, LIBRARY_PAGE_SIZE, 1000));

  LibraryList list;
  list.items.resize(LIBRARY_PAGE_SIZE);
  assert(!library_should_fetch_more(list, 10, 1000));
  assert(library_should_fetch_more(list, LIBRARY_PAGE_SIZE - LIBRARY_PREFETCH_ROWS, 1000));
  list.loading = true;
  assert(!library_should_fetch_more(list, LIBRARY_PAGE_SIZE, 1000));
  list.loading = false;
  list.complete = true;
  assert(!library_should_fetch_more(list, LIBRARY_PAGE_SIZE, 1000));
  list.complete = false;
  assert(!library_should_fetch_more(list, LIBRARY_PAGE_SIZE, LIBRARY_PAGE_SIZE));

  assert(library_first_visible(0, 84, 10) == 0);
  assert(library_first_visible(-20, 84, 10) == 0);
  assert(library_first_visible(170, 84, 10) == 2);
  assert(library_first_visible(100000, 84, 10) == 9);
  assert(library_first_visible(170, 84, 0) == 0);

  // Only plain entity ids may be placed inside the config entry template.
  assert(library_entity_id_safe("media_player.kitchen_2"));
  assert(!library_entity_id_safe("media_player.kitchen') }}{{ x('"));
  assert(!library_entity_id_safe("media_player"));
  assert(!library_entity_id_safe("media_player.a.b"));
  assert(!library_entity_id_safe(".kitchen"));
  assert(library_config_entry_template("media_player.kitchen") ==
         "{{ config_entry_id('media_player.kitchen') }}");
  assert(std::string(library_media_type(LibraryKind::RADIO)) == "radio");
  assert(std::string(library_icon_name(LibraryKind::ARTIST)) == "Account Music");
}
