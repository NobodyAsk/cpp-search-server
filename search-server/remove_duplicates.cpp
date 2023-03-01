#include <set>
#include <map>
#include <iostream>
#include "remove_duplicates.h"
#include "search_server.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> id_to_erase;
    for (auto l_it = search_server.begin(); l_it != search_server.end() - 1; ++l_it) {
        int l_id = *l_it;
        if (id_to_erase.count(l_id)) {
            continue;
        }
        auto l_temp = search_server.GetWordFrequencies(l_id);
        for (auto r_it = l_it + 1; r_it != search_server.end(); ++r_it) {
            int r_id = *r_it;
            auto r_temp = search_server.GetWordFrequencies(r_id);
            if (l_temp.size() != r_temp.size()) {
                continue;
            }
            bool is_duplicat = false;
            for (auto [key, value] : l_temp) {
                if (r_temp.count(key) != 0) {
                    is_duplicat = true;
                }
                else {
                    is_duplicat = false;
                    break;
                }
            }
            if (is_duplicat) {
                id_to_erase.insert(r_id);
            }
        }
    }
    for (int i : id_to_erase) {
        search_server.RemoveDocument(i);
    }
}