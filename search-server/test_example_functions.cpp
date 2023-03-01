#include <string>
#include <vector>
#include <iostream>

#include "document.h"
#include "log_duration.h"
#include "search_server.h"
#include "test_example_functions.h"

using namespace std::string_literals;

void TestAssert(bool t, const std::string& t_str, const std::string& file_name, const std::string& function_name, int line, const std::string& hint) {
    if (t) {
        return;
    }
    std::cerr << std::boolalpha;
    std::cerr << file_name << "("s << line << "): "s << function_name << ": ";
    std::cerr << "ASSERT("s << t_str << ") failed: "s;
    std::cerr << t << " != "s << true << "."s;
    if (!hint.empty()) {
        std::cerr << " Hint: "s << hint;
    }
    std::cerr << std::endl;
    abort();
}

void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<std::string> stop_words = { "no"s, "in"s, "the"s };
    const std::vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    {
        SearchServer server("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents");
    }
    {
        SearchServer server(stop_words);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

void TestFindAddedDocument() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    //ѕровер€ем что документ находитс€ по поисковому запросу
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Releavant documents should be in search results"s);
    }
    // ѕровер€ем, что нерелевантный документ не находитс€ по поисковому запросу
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT_HINT(found_docs.empty(), "Not releavant documents shouldn't be in search results"s);
    }
    // ѕровер€ем, что наход€тс€ все документы по поисковому запросу
    {
        SearchServer server("none"s);
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat in the night city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "dog in the town"s, DocumentStatus::ACTUAL, ratings);
        const auto found_doc = server.FindTopDocuments("cat in the night"s);
        ASSERT(found_doc.size() == 3);
    }
}

void TestExcludeMinusDocumentFromResults() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    // ѕровер€ем, что документы с минус словами не будут включены в выдачу
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -in"s);
        ASSERT_HINT(found_docs.empty(), "Documents witn minus words shouldn't be in search results"s);
    }
    // ѕровер€ем, что документы без минус слов будут включены в выдачу
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in -dog"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

void TestMatchingDocument() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    // ѕровер€ем, что вернулись все совпадени€ по словам
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        std::tuple<std::vector<std::string>, DocumentStatus> matching_doc = server.MatchDocument("cat in the night the"s, doc_id);
        std::vector<std::string> first_of_tuple = std::get<0>(matching_doc);
        ASSERT_EQUAL_HINT(count(first_of_tuple.begin(), first_of_tuple.end(), "cat"s), 1, "Didn't find all searched words from document"s);
        ASSERT_EQUAL_HINT(count(first_of_tuple.begin(), first_of_tuple.end(), "in"s), 1, "Didn't find all searched words from document"s);
        ASSERT_EQUAL_HINT(count(first_of_tuple.begin(), first_of_tuple.end(), "the"s), 1, "Didn't find all searched words from document"s);
    }
}

void TestSortingDocumentsInRelevantOrder() {
    const std::vector<int> ratings = { 1, 2, 3 };
    // ѕровер€ем, что документы отсортированы по релевантности, от большей к меньшей
    {
        SearchServer server("none"s);
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat in the night city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "dog in the town"s, DocumentStatus::ACTUAL, ratings);
        const auto found_doc = server.FindTopDocuments("cat in the night"s);
        ASSERT_HINT(found_doc[0].relevance >= found_doc[1].relevance, "Incorrect sorting: shuold be from largest to smallest"s);
    }
}

void TestAverageRatingOfAddedDocument() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    // ѕровер€ем, что средний рейтинг совпадает с заранее посчитанным
    {
        const std::vector<int> ratings = { 1, 2, 3 };
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, 2, "Incorrect average rating"s);
    }
    // ѕровер€ем средний рейтинг при рейтинге 0
    {
        const std::vector<int> ratings = { 0 };
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, 0, "Incorrect average rating"s);
    }
    // ѕровер€ем отрицательный средний рейтинг
    {
        const std::vector<int> ratings = { -1, -2, -3 };
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, -2, "Incorrect average rating"s);
    }
    // ѕровер€ем средний рейтинг при оценках 0, больше и меньше 0
    {
        const std::vector<int> ratings = { 0, -2, -3, 10, 5 };
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, 2, "Incorrect average rating"s);
    }
}

void TestFilterWithPredicateOfUser() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    // ѕровер€ем, что будут включены только документы отсе€ные функцией-предикатом
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_HINT(!found_docs.empty(), "No documents find"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, 42, "Predicate function didn't find correct document"s);
    }
}

void TestFilterWithDocumentsStatus() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, content, DocumentStatus::BANNED, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT_HINT(!found_docs.empty(), "No documents find"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, 3, "Finded documents have incorrect status."s);
    }
}

void TestCorrectCalculationRelevanceOfDocuments() {
    const int doc_id = 42;
    const std::string content = "cat in the city"s;
    const std::vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "cat in the night city"s, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("night cat"s);
        const Document& doc0 = found_docs[0];
        const double tf_idf = 0.138629;
        ASSERT_HINT(std::abs(doc0.relevance - tf_idf) <= 0.01, "Relevance is not in confidence interval"s);
    }
}

void TestSearchServer() {
    LOG_DURATION("Tests for SearchServer"s);
    RUN_TEST(TestFindAddedDocument);
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusDocumentFromResults);
    RUN_TEST(TestMatchingDocument);
    RUN_TEST(TestSortingDocumentsInRelevantOrder);
    RUN_TEST(TestAverageRatingOfAddedDocument);
    RUN_TEST(TestFilterWithPredicateOfUser);
    RUN_TEST(TestFilterWithDocumentsStatus);
    RUN_TEST(TestCorrectCalculationRelevanceOfDocuments);
}