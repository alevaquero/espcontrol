#include <cassert>
#include <string>
#include <vector>
#include "media_library.h"

int main() {
  using namespace espcontrol::media;
  LibraryItems items;

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
  LibraryItems capped;
  assert(library_parse_page("a\tu1\nb\tu2\nc\tu3", capped, 2) == 3 && capped.size() == 2);
  assert(library_page_complete(3, LIBRARY_PAGE_SIZE, 2, 2));
  assert(library_page_complete(LIBRARY_PAGE_SIZE - 1, LIBRARY_PAGE_SIZE, 10, 1000));
  assert(!library_page_complete(LIBRARY_PAGE_SIZE, LIBRARY_PAGE_SIZE, LIBRARY_PAGE_SIZE, 1000));
  // A full bulk page is not the end, even though it is longer than a normal page.
  assert(!library_page_complete(LIBRARY_BULK_PAGE_SIZE, LIBRARY_BULK_PAGE_SIZE, 250, 3000));
  assert(library_page_complete(LIBRARY_BULK_PAGE_SIZE - 1, LIBRARY_BULK_PAGE_SIZE, 250, 3000));

  LibraryList list;
  list.items.resize(LIBRARY_PAGE_SIZE);
  // The rest of the library keeps loading until it is complete or capped.
  assert(library_should_load_more(list, 1000));
  list.loading = true;
  assert(!library_should_load_more(list, 1000));
  list.loading = false;
  list.complete = true;
  assert(!library_should_load_more(list, 1000));
  list.complete = false;
  list.failed = true;
  assert(!library_should_load_more(list, 1000));
  list.failed = false;
  assert(!library_should_load_more(list, LIBRARY_PAGE_SIZE));

  assert(std::string(library_order_by(LibraryFilter::ALL)) == "sort_name");
  assert(std::string(library_order_by(LibraryFilter::FAVORITES)) == "sort_name");
  assert(std::string(library_order_by(LibraryFilter::RECENT)) == "last_played_desc");
  assert(!library_filter_alphabetical(LibraryFilter::RECENT));

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

  // Sort names follow Music Assistant: lower case, accents dropped, leading
  // punctuation removed, and a leading article moved to the end.
  assert(library_sort_name("The Beatles") == "beatles, the");
  assert(library_sort_name("  A Tribe Called Quest ") == "tribe called quest, a");
  assert(library_sort_name("DJ Shadow") == "shadow, dj");
  assert(library_sort_name("Theory of a Deadman") == "theory of a deadman");
  assert(library_sort_name("\"Weird Al\" Yankovic") == "weird al\" yankovic");
  assert(library_sort_name("\xC3\x89" "dith Piaf") == "edith piaf");
  assert(library_sort_name("Bj\xC3\xB6rk") == "bjork");
  assert(library_sort_name("\xC3\x86" "ther") == "\xC3\xA6" "ther");
  assert(library_sort_name("2Pac") == "2pac");
  assert(library_sort_name("") == "");
  // Parsed entries carry their sort name.
  LibraryItems parsed;
  library_parse_page("The Killers\tu1\n", parsed, 10);
  assert(parsed.size() == 1 && parsed[0].sort == "killers, the");

  // Letter keys: '#' covers anything that is not a letter.
  assert(library_key_rank('#') == 0 && library_key_rank('2') == 0 && library_key_rank(' ') == 0);
  assert(library_key_rank('a') == 1 && library_key_rank('A') == 1 && library_key_rank('z') == 26);
  assert(library_key_for_rank(0) == '#' && library_key_for_rank(14) == 'N');
  assert(library_prefix_compare("anastacia", "AN") == 0);
  assert(library_prefix_compare("abba", "AN") < 0);
  assert(library_prefix_compare("arcade fire", "AN") > 0);
  assert(library_prefix_compare("a", "AN") < 0);
  assert(library_prefix_compare("ac/dc", "AC#") == 0);

  auto make_items = [](std::initializer_list<const char *> names) {
    LibraryItems out;
    for (const char *name : names) out.push_back({name, "u", library_sort_name(name)});
    return out;
  };
  // In Music Assistant's order.
  const LibraryItems artists = make_items({"10cc", "ABBA", "AC/DC", "Adele", "Anastacia", "Andrea Bocelli",
                                           "Arcade Fire", "The Beatles", "Coldplay", "Hozier"});
  assert(library_prefix_find(artists, "") == 0);
  assert(library_prefix_find(artists, "#") == 0);
  assert(library_prefix_find(artists, "A") == 1);
  assert(library_prefix_find(artists, "AN") == 4);
  assert(library_prefix_find(artists, "AND") == 5);
  assert(library_prefix_find(artists, "B") == 7);
  // A missing prefix lands on the next entry that exists.
  assert(library_prefix_find(artists, "AO") == 6);
  assert(library_prefix_find(artists, "E") == 9);
  // Past every loaded entry: more pages may be needed.
  assert(library_prefix_find(artists, "Z") == -1);

  const auto bit = [](char key) { return 1u << library_key_rank(key); };
  // With the whole library loaded, only letters that lead somewhere stay on.
  assert(library_next_keys(artists, "", true) ==
         (bit('#') | bit('A') | bit('B') | bit('C') | bit('H')));
  assert(library_next_keys(artists, "A", true) == (bit('B') | bit('C') | bit('D') | bit('N') | bit('R')));
  assert(library_next_keys(artists, "AC", true) == bit('#'));
  assert(library_next_keys(artists, "AN", true) == (bit('A') | bit('D')));
  assert(library_next_keys(artists, "ZZ", true) == 0);
  // While loading, letters after the last loaded entry may still exist.
  const uint32_t loading = library_next_keys(artists, "", false);
  assert((loading & bit('H')) && (loading & bit('Z')) && !(loading & bit('D')));
  assert(library_next_keys(LibraryItems{}, "", false) == (1u << LIBRARY_INDEX_KEY_COUNT) - 1);
}
