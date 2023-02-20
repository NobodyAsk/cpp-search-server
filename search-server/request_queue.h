#pragma once
#include <vector>
#include <string>
#include <deque>
#include "request_queue.h"
#include "search_server.h"
#include "document.h"

class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate);
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;

private:
    struct QueryResult {
        bool is_empty;
        std::vector<std::string> query_;
        std::vector<Document> relevant_documents_;
    };
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    const SearchServer& server_;
    int empty_results_ = 0;
};

template <typename DocumentPredicate>
std::vector<Document> RequestQueue::AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
    std::vector<Document> documents = server_.FindTopDocuments(raw_query, document_predicate);
    requests_.push_back({ documents.empty(), SplitIntoWords(raw_query), documents });

    if (documents.empty()) {
        ++empty_results_;
    }

    if (requests_.size() > min_in_day_) {
        if (requests_.front().is_empty) {
            --empty_results_;
            if (empty_results_ < 0) {
                empty_results_ = 0;
            }
        }
        requests_.pop_front();
    }
    return documents;
}