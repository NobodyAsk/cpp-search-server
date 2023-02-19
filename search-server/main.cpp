#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <sstream>
#include <deque>
#include <iostream>

#include "string_processing.h"
#include "document.h"
#include "read_input_functions.h"
#include "search_server.h"
#include "paginator.h"
#include "request_queue.h"
 

using namespace std;

/*---------------------начало тестов------------------------*/

template<typename T, typename U>
void TestEqualAssert(T t, U u, const string& t_str, const string& u_str, const string& file_name, const string& function_name, int line, const string& hint) {
    if (t == u) {
        return;
    }
    cerr << file_name << "("s << line << "): "s << function_name << ": "s;
    cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
    cerr << t << " != "s << u << "."s;
    if (!hint.empty()) {
        cout << " Hint: "s << hint;
    }
    cerr << endl;
    abort();
}

void TestAssert(bool t, const string& t_str, const string& file_name, const string& function_name, int line, const string& hint) {
    if (t) {
        return;
    }
    cerr << boolalpha;
    cerr << file_name << "("s << line << "): "s << function_name << ": "s;
    cerr << "ASSERT("s << t_str << ") failed: "s;
    cerr << t << " != "s << true << "."s;
    if (!hint.empty()) {
        cout << " Hint: "s << hint;
    }
    cerr << endl;
    abort();
}

#define ASSERT(a) TestAssert((a), #a, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_HINT(a, hint) TestAssert((a), #a, __FILE__, __FUNCTION__, __LINE__, hint)
#define ASSERT_EQUAL(a, b) TestEqualAssert((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) TestEqualAssert((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, hint)

template<typename T>
void RunTestFunction(T& func, const string& func_str) {
    cerr << "Trying run " << func_str;
    func();
    cerr << func_str << " OK" << endl;
}
#define RUN_TEST(function) RunTestFunction((function), #function)

// -------- Начало модульных тестов поисковой системы ----------
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
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
            "Stop words must be excluded from documents"s);
    }
}

void TestFindAddedDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    //Проверяем что документ находится по поисковому запросу
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, doc_id, "Releavant documents should be in search results"s);
    }
    // Проверяем, что нерелевантный документ не находится по поисковому запросу
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("dog"s);
        ASSERT_HINT(found_docs.empty(), "Not releavant documents shouldn't be in search results"s);
    }
    // Проверяем, что находятся все документы по поисковому запросу
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
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Проверяем, что документы с минус словами не будут включены в выдачу
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat -in"s);
        ASSERT_HINT(found_docs.empty(), "Documents witn minus words shouldn't be in search results");
    }
    // Проверяем, что документы без минус слов будут включены в выдачу
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
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Проверяем, что вернулись все совпадения по словам
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        tuple<vector<string>, DocumentStatus> matching_doc = server.MatchDocument("cat in the night the"s, doc_id);
        vector<string> first_of_tuple = get<0>(matching_doc);
        ASSERT_EQUAL_HINT(count(first_of_tuple.begin(), first_of_tuple.end(), "cat"), 1, "Didn't find all searched words from document");
        ASSERT_EQUAL_HINT(count(first_of_tuple.begin(), first_of_tuple.end(), "in"s), 1, "Didn't find all searched words from document");
        ASSERT_EQUAL_HINT(count(first_of_tuple.begin(), first_of_tuple.end(), "the"s), 1, "Didn't find all searched words from document");
    }
}

void TestSortingDocumentsInRelevantOrder() {
    const vector<int> ratings = { 1, 2, 3 };
    // Проверяем, что документы отсортированы по релевантности, от большей к меньшей
    {
        SearchServer server("none"s);
        server.AddDocument(1, "cat in the city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(2, "cat in the night city"s, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "dog in the town"s, DocumentStatus::ACTUAL, ratings);
        const auto found_doc = server.FindTopDocuments("cat in the night"s);
        ASSERT_HINT(found_doc[0].relevance >= found_doc[1].relevance, "Incorrect sorting: shuold be from largest to smallest");
    }
}

void TestAverageRatingOfAddedDocument() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    // Проверяем, что средний рейтинг совпадает с заранее посчитанным
    {
        const vector<int> ratings = { 1, 2, 3 };
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, 2, "Incorrect average rating");
    }
    // Проверяем средний рейтинг при рейтинге 0
    {
        const vector<int> ratings = { 0 };
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, 0, "Incorrect average rating");
    }
    // Проверяем отрицательный средний рейтинг
    {
        const vector<int> ratings = { -1, -2, -3 };
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, -2, "Incorrect average rating");
    }
    // Проверяем средний рейтинг при оценках 0, больше и меньше 0
    {
        const vector<int> ratings = { 0, -2, -3, 10, 5 };
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.rating, 2, "Incorrect average rating");
    }
}

void TestFilterWithPredicateOfUser() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // Проверяем, что будут включены только документы отсеяные функцией-предикатом
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; });
        ASSERT_HINT(!found_docs.empty(), "No documents find");
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, 42, "Predicate function didn't find correct document");
    }
}

void TestFilterWithDocumentsStatus() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, content, DocumentStatus::BANNED, ratings);
        const auto found_docs = server.FindTopDocuments("cat"s, DocumentStatus::BANNED);
        ASSERT_HINT(!found_docs.empty(), "No documents find");
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL_HINT(doc0.id, 3, "Finded documents have incorrect status.");
    }
}

void TestCorrectCalculationRelevanceOfDocuments() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server("none"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(3, "cat in the night city"s, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("night cat"s);
        const Document& doc0 = found_docs[0];
        const double tf_idf = 0.138629;
        ASSERT_HINT(abs(doc0.relevance - tf_idf) <= 0.01, "Relevance is not in confidence interval");
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
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

/*-------------------- конец тестов ------------------------*/

/*--------------------- для примера ------------------------*/
void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}

int main() {
    TestSearchServer();
    SearchServer search_server("and in at"s);
    RequestQueue request_queue(search_server);
    search_server.AddDocument(1, "curly cat curly tail", DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "curly dog and fancy collar", DocumentStatus::ACTUAL, { 1, 2, 3 });
    search_server.AddDocument(3, "big cat fancy collar ", DocumentStatus::ACTUAL, { 1, 2, 8 });
    search_server.AddDocument(4, "big dog sparrow Eugene", DocumentStatus::ACTUAL, { 1, 3, 2 });
    search_server.AddDocument(5, "big dog sparrow Vasiliy", DocumentStatus::ACTUAL, { 1, 1, 1 });
    // 1439 запросов с нулевым результатом
    for (int i = 0; i < 1439; ++i) {
        request_queue.AddFindRequest("big dog cat");
    }
    std::cout << "Total empty requests: " << request_queue.GetNoResultRequests() << std::endl;
    // все еще 1439 запросов с нулевым результатом
    request_queue.AddFindRequest("curly dog");
    // новые сутки, первый запрос удален, 1438 запросов с нулевым результатом
    request_queue.AddFindRequest("big collar");
    std::cout << "Total empty requests: " << request_queue.GetNoResultRequests() << std::endl;
    // первый запрос удален, 1437 запросов с нулевым результатом
    request_queue.AddFindRequest("sparrow");
    std::cout << "Total empty requests: " << request_queue.GetNoResultRequests() << std::endl;
    return 0;
}
