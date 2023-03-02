#include <set>
#include <map>
#include <iostream>
#include "remove_duplicates.h"
#include "search_server.h"

void RemoveDuplicates(SearchServer& search_server) {
    std::set<int> id_to_erase;
    std::set<std::set<std::string>> words_in_documents;
    for (const int document_id : search_server) {
        std::set<std::string> temp;
        for (auto [key, value] : search_server.GetWordFrequencies(document_id)) {
            temp.insert(key);
        }
        if (words_in_documents.count(temp)) {
            id_to_erase.insert(document_id);
        }
        else {
            words_in_documents.insert(temp);
        }
    }
    for (int i : id_to_erase) {
        search_server.RemoveDocument(i);
    }
}