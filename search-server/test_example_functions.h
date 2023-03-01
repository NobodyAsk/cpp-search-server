#pragma once
#include <string>
#include <iostream>

template<typename T, typename U>
void TestEqualAssert(T t, U u, const std::string& t_str, const std::string& u_str, const std::string& file_name, const std::string& function_name, int line, const std::string& hint) {
    if (t == u) {
        return;
    }
    std::cerr << file_name << "(" << line << "): " << function_name << ": ";
    std::cerr << "ASSERT_EQUAL(" << t_str << ", " << u_str << ") failed: ";
    std::cerr << t << " != " << u << ".";
    if (!hint.empty()) {
        std::cerr << " Hint: " << hint;
    }
    std::cerr << std::endl;
    abort();
}

void TestAssert(bool t, const std::string& t_str, const std::string& file_name, const std::string& function_name, int line, const std::string& hint);

#define ASSERT(a) TestAssert((a), #a, __FILE__, __FUNCTION__, __LINE__, "")
#define ASSERT_HINT(a, hint) TestAssert((a), #a, __FILE__, __FUNCTION__, __LINE__, hint)
#define ASSERT_EQUAL(a, b) TestEqualAssert((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, "")
#define ASSERT_EQUAL_HINT(a, b, hint) TestEqualAssert((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, hint)

template<typename T>
void RunTestFunction(T& func, const std::string& func_str) {
    std::cerr << "Trying run " << func_str;
    func();
    std::cerr << func_str << " OK" << std::endl;
}

#define RUN_TEST(function) RunTestFunction((function), #function)

void TestExcludeStopWordsFromAddedDocumentContent();
void TestFindAddedDocument();
void TestExcludeMinusDocumentFromResults();
void TestMatchingDocument();
void TestSortingDocumentsInRelevantOrder();
void TestAverageRatingOfAddedDocument();
void TestFilterWithPredicateOfUser();
void TestFilterWithDocumentsStatus();
void TestCorrectCalculationRelevanceOfDocuments();

void TestSearchServer();
