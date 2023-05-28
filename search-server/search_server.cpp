#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <utility>
#include <stdexcept>
#include <numeric>
#include <execution>
#include <string_view>
#include "string_processing.h"
#include "document.h"
#include "search_server.h"
#include "concurrent_map.h"

using namespace std;

SearchServer::SearchServer(const std::string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
{
}

SearchServer::SearchServer(std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
{
}

void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
    const std::vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw std::invalid_argument("Invalid document_id");
    }
    auto current_doc = documents_in_strings_.insert(static_cast<std::string>(document));
    const std::vector<std::string_view> words = SplitIntoWordsNoStop(*current_doc.first);
    const double inv_word_count = 1.0 / words.size();
    for (std::string_view word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        id_to_document_freqs_[document_id][word] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ SearchServer::ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(
        raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
        });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return SearchServer::FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

std::set<int>::iterator SearchServer::begin() {
    return document_ids_.begin();
}

std::set<int>::iterator SearchServer::end() {
    return document_ids_.end();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    try {
        const std::map<std::string_view, double>& key = id_to_document_freqs_.at(document_id);
        return key;
    }
    catch (const std::out_of_range& ex) {
        const std::map<std::string_view, double>& key = empty_ref_;
        return key;
    }
}

void SearchServer::RemoveDocument(int document_id) {
    for (auto [str, freq] : id_to_document_freqs_[document_id]) {
        word_to_document_freqs_.at(str).erase(document_id);
    }
    id_to_document_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(find(document_ids_.begin(), document_ids_.end(), document_id));
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
    std::vector<std::string_view> str_to_remove(id_to_document_freqs_.at(document_id).size());
    transform(
        std::execution::par,
        id_to_document_freqs_.at(document_id).begin(),
        id_to_document_freqs_.at(document_id).end(),
        str_to_remove.begin(),
        [](auto& temp_pair) {
            const auto temp = temp_pair.first;
            return temp;
        }
    );
    std::for_each(
        std::execution::par,
        str_to_remove.begin(),
        str_to_remove.end(),
        [this, document_id](std::string_view str) {word_to_document_freqs_[str].erase(document_id); }
    );
    id_to_document_freqs_.erase(document_id);
    documents_.erase(document_id);
    document_ids_.erase(document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
    std::string_view raw_query,
    int document_id) const {
    auto query = ParseQuery(raw_query);
    std::vector<std::string_view> matched_words;

    for (std::string_view min_word : query.minus_words) {
        if (std::any_of(
            id_to_document_freqs_.at(document_id).begin(),
            id_to_document_freqs_.at(document_id).end(),
            [min_word](const auto word) {return word.first == min_word; })
            ) {
            matched_words.clear();
            return { matched_words, documents_.at(document_id).status };
        }
    }
    for (std::string_view plus_word : query.plus_words) {
        if (std::any_of(
            id_to_document_freqs_.at(document_id).begin(),
            id_to_document_freqs_.at(document_id).end(),
            [plus_word](auto word) { return word.first == plus_word; })
            ) {
            auto temp = id_to_document_freqs_.at(document_id).find(plus_word);
            matched_words.push_back((*temp).first);
        }
    }
    return { matched_words, documents_.at(document_id).status };
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
    const std::execution::sequenced_policy&,
    std::string_view raw_query,
    int document_id) const {
    return MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(
    const std::execution::parallel_policy&,
    std::string_view raw_query,
    int document_id) const {
    const auto query = ParseQueryPar(raw_query);
    std::vector<std::string_view> matched_words;
    matched_words.reserve(query.plus_words.size());
    for (std::string_view min_word : query.minus_words) {
        if (std::any_of(std::execution::par,
            id_to_document_freqs_.at(document_id).begin(),
            id_to_document_freqs_.at(document_id).end(),
            [&min_word](const auto& word) {return word.first == min_word; })
            ) {
            return { matched_words, documents_.at(document_id).status };
        }
    }
    for (std::string_view plus_word : query.plus_words) {
        if (std::any_of(
            id_to_document_freqs_.at(document_id).begin(),
            id_to_document_freqs_.at(document_id).end(),
            [plus_word](auto word) { return word.first == plus_word; })
            ) {
            auto temp = id_to_document_freqs_.at(document_id).find(plus_word);
            matched_words.push_back((*temp).first);
        }
    }
    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
    auto last = std::unique(matched_words.begin(), matched_words.end());
    matched_words.erase(last, matched_words.end());
    return { matched_words, documents_.at(document_id).status };
}


/*-------------private---------------*/

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(static_cast<std::string>(word)) > 0;
}

bool SearchServer::IsValidWord(std::string_view word) {
    // A valid word must not contain special characters
    return std::none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        }
    );
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (std::string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw std::invalid_argument("Word " + static_cast<std::string>(word) + " is invalid");
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = std::accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    if (text.empty()) {
        throw std::invalid_argument("Query word is empty");
    }
    bool is_minus = false;
    if (text.at(0) == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw std::invalid_argument("Query word " + static_cast<std::string>(text) + " is invalid");
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    Query result;
    for (std::string_view word : SplitIntoWords(text)) {
        auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            query_word.is_minus ? 
                result.minus_words.push_back(query_word.data) : 
                result.plus_words.push_back(query_word.data);
        }
    }
    std::sort(result.minus_words.begin(), result.minus_words.end());
    std::sort(result.plus_words.begin(), result.plus_words.end());
    auto last = std::unique(result.minus_words.begin(), result.minus_words.end());
    result.minus_words.erase(last, result.minus_words.end());
    last = std::unique(result.plus_words.begin(), result.plus_words.end());
    result.plus_words.erase(last, result.plus_words.end());
    return result;
}

SearchServer::Query SearchServer::ParseQueryPar(std::string_view text) const {
    Query result;
    for (std::string_view word : SplitIntoWords(text)) {
        auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            query_word.is_minus ? 
                result.minus_words.push_back(query_word.data) : 
                result.plus_words.push_back(query_word.data);
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return std::log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
