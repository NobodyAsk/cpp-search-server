#pragma once
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <utility>
#include <stdexcept>
#include <execution>
#include <functional>

#include "string_processing.h"
#include "document.h"
#include "concurrent_map.h"

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr int MAX_MAPS_TO_DIVIDE = 50;
constexpr double TOLERANCE = 1e-6;

class SearchServer {
public:

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
        const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(
        const std::execution::sequenced_policy&,
        std::string_view raw_query,
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(
        const std::execution::parallel_policy&,
        std::string_view raw_query,
        DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(
        const std::execution::sequenced_policy&,
        std::string_view raw_query,
        DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(
        const std::execution::parallel_policy&,
        std::string_view raw_query,
        DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const;

    int GetDocumentCount() const;
    std::set<int>::iterator begin();
    std::set<int>::iterator end();

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(std::string_view raw_query,
        int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::sequenced_policy&,
        std::string_view raw_query,
        int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::parallel_policy&,
        std::string_view raw_query,
        int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

private:
    struct DocumentData {
        int rating = 0;
        DocumentStatus status = DocumentStatus::ACTUAL;
    };
    const std::set<std::string> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> id_to_document_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::set<std::string, std::less<>> documents_in_strings_;
    const std::map<std::string_view, double> empty_ref_;

    bool IsStopWord(std::string_view word) const;
    static bool IsValidWord(std::string_view word);
    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);
    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };
    Query ParseQuery(std::string_view text) const;
    Query ParseQueryPar(std::string_view text) const;
    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(Query& query,
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(
        const std::execution::sequenced_policy&,
        Query& query,
        DocumentPredicate document_predicate) const;
    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(
        const std::execution::parallel_policy&,
        Query& query,
        DocumentPredicate document_predicate) const;

};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid");
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(query, document_predicate);
    std::sort(matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < TOLERANCE) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        }
    );
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    return FindTopDocuments(raw_query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    auto query = ParseQueryPar(raw_query);

    std::sort(std::execution::par, query.minus_words.begin(), query.minus_words.end());
    std::sort(std::execution::par, query.plus_words.begin(), query.plus_words.end());
    auto last = std::unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.erase(last, query.minus_words.end());
    last = std::unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.erase(last, query.plus_words.end());

    auto matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);
    std::sort(std::execution::par, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < TOLERANCE) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        }
    );
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(Query& query,
    DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;

    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            if (documents_.count(document_id)) {
                const auto& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
    }
    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, Query& query,
    DocumentPredicate document_predicate) const {
    return FindAllDocuments(query, document_predicate);
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, Query& query,
    DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(MAX_MAPS_TO_DIVIDE);
    std::for_each(std::execution::par,
        query.plus_words.begin(),
        query.plus_words.end(),
        [this, &document_to_relevance, document_predicate](std::string_view word)
        {
            if (word_to_document_freqs_.count(word) == 0) {
                return;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);

            std::for_each(std::execution::par,
                word_to_document_freqs_.at(word).begin(),
                word_to_document_freqs_.at(word).end(),
                [this, &document_to_relevance, document_predicate, inverse_document_freq](const auto pair) {
                    if (documents_.count(pair.first)) {
                        const auto document_data = documents_.at(pair.first);
                        if (document_predicate(pair.first, document_data.status, document_data.rating)) {

                            document_to_relevance[pair.first].ref_to_value += pair.second * inverse_document_freq;
                        }
                    }
                }
            );
        }
    );
    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }
    std::vector<Document> matched_documents;
    for (const auto& [document_id, relevance] : document_to_relevance.BuildOrdinaryMap()) {
        matched_documents.push_back(
            { document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

