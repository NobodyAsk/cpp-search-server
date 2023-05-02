#include <vector>
#include <execution>
#include <list>
#include "document.h"
#include "search_server.h"
#include "process_queries.h"


std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> top_documents(queries.size());
    transform(
        std::execution::par,
        queries.begin(),
        queries.end(),
        top_documents.begin(),
        [&search_server](const std::string& query) {
            return search_server.FindTopDocuments(query);
        });
    return top_documents;
}

std::vector<Document>ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<Document> documents;
    std::vector<std::vector<Document>> temp = ProcessQueries(search_server, queries);
    for (auto vec : temp) {
        for (auto doc : vec) {
            documents.push_back(doc);
        }
    }
    return documents;
}