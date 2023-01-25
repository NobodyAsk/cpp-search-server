#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>
 
using namespace std;
 
const int MAX_RESULT_DOCUMENT_COUNT = 5;
 
string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}
 
int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    // Метод для деления текста на лова
    vector<string> words;
    string word;
     // Делим слова в тексте по пробелам
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        } else {
            word += c;
        }
    }
    // Добавляем последнее слово, если оно есть
    if (!word.empty()) {
        words.push_back(word);
    }
 
    return words;
}

// Структура для хранения характеристик документа
struct Document {
    Document() = default;

    Document(int id, double relevance, int rating)
        : id(id)
        , relevance(relevance)
        , rating(rating) {
    }

    int id = 0;
    double relevance = 0.0;
    int rating = 0;
};

template <typename StringContainer>
set<string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    // Функция создания набора стоп-слов для конструктора класса SearchServer
    set<string> non_empty_strings;
    for (const string& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(str);
        }
    }
    return non_empty_strings;
}

// Перечислитель для статусов документов
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};
 
class SearchServer {
public:

    // Defines an invalid document id
    // You can refer to this constant as SearchServer::INVALID_DOCUMENT_ID
    inline static constexpr int INVALID_DOCUMENT_ID = -1;

    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {
        for (const auto& str : stop_words) {
            for (const auto c : str) {
                if (c >= 0 && c <= 31) {
                    throw invalid_argument("Stop words contain invalid symbols");
                }
            }
        }
    }

    explicit SearchServer(const string& stop_words_text)
        : SearchServer(
            SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }
 
    void AddDocument(
        int document_id,
        const string& document,
        DocumentStatus status,
        const vector<int>& ratings) {
        //Метод для добавления документов в объект класса
        //обработка ошибок ввода
        if (document_id < 0) {
            throw invalid_argument("Document ID shouldn't be negative number");
        }
        for (char c : document) {
            if (c >= 0 && c <= 31) {
                throw invalid_argument("Document contain invalid symbols");
            }
        }
        if (documents_.count(document_id) > 0) {
            throw invalid_argument("Document with that ID already exist");
        }
        const vector<string> words = SplitIntoWordsNoStop(document);
        //Добавляем TF для каждого слова внутри документа
        const double inv_word_count = 1.0 / words.size();
        for (auto& word : words) {
            word_to_documents_freq_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
        return;
    }
 
    vector<Document> FindTopDocuments(const string& raw_query) const{
        // Метод для нахождения n самых подходящих документов только по фразе запроса 
        return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
    }    
 
    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus& current_status) const{
        // Метод для нахождения n самых подходящих документов по фразе запроса и статусу документа 
        return FindTopDocuments(raw_query, [current_status](
            int document_id, 
            DocumentStatus status, 
            int rating) { return status == current_status; });
    }

    template <typename KeyMaper>
    vector<Document> FindTopDocuments(const string& raw_query, KeyMaper key_map) const {
        // Метод для нахождения n самых подходящих документов по функции-предикату
        //обработка ошибок ввода
        {
            char temp = 'o';
            bool is_last = false;
            for (char c : raw_query) {
                is_last = false;
                if (c >= 0 && c <= 31) {
                    throw invalid_argument("Searching queue contain invalid symbols");
                }
                if (c == '-') {
                    is_last = true;
                    if (temp == '-') {
                        throw invalid_argument("Searching queue have double minus [--] in it");
                    }
                }
                temp = c;
            }
            if (is_last) { throw invalid_argument("Searching queue shouldn't end with [-]"); }
        }
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, key_map);
        // Сортируем документы по релевантности и рейтингу
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        // Убираем лишние документы из списка
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    int GetDocumentCount() const {
        // Функция возвращает текущее количество документов
        return documents_.size();
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        // Метод для сравнения документа с параметрами поиска
        //обработка ошибок ввода
        {
            char temp = 'o';
            bool is_last = false;
            for (char c : raw_query) {
                is_last = false;
                if (c >= 0 && c <= 31) {
                    throw invalid_argument("Searching queue contain invalid symbols");
                }
                if (c == '-') {
                    is_last = true;
                    if (temp == '-') {
                        throw invalid_argument("Searching queue have double minus [--] in it");
                    }
                }
                temp = c;
            }
            if (is_last) { throw invalid_argument("Searching queue shouldn't end with [-]"); }
        }
        const Query query = ParseQuery(raw_query);
        vector<string> matched_words;
        // Находим совпадения в документе с фразой запроса
        for (const string& word : query.plus_words) {
            if (word_to_documents_freq_.count(word) == 0) {
                continue;
            }
            if (word_to_documents_freq_.at(word).count(document_id)) {
                matched_words.push_back(word);
            }
        }
        // Фильтруем документ по минус словам запроса
        for (const string& word : query.minus_words) {
            if (word_to_documents_freq_.count(word) == 0) {
                continue;
            }
            if (word_to_documents_freq_.at(word).count(document_id)) {
                matched_words.clear();
                break;
            }
        }
        return {matched_words, documents_.at(document_id).status};
    }

    int GetDocumentId(int index) const {
        // Метод для нахождения ID документа по индексу
        if (documents_.size() < index) {
            throw out_of_range("Index is out of range");
        }
        int count = 0;
        for (auto& value : documents_) {
            if (index == count) {
                return value.first;
            }
            ++count;
        }
        return INVALID_DOCUMENT_ID;
    }

private:
    /*Структуры класса*/
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };
 
    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    /*Параметры класса*/
    map<string, map<int, double>> word_to_documents_freq_;
    map<int, DocumentData> documents_;
    set<string> stop_words_;
 
    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }
 
    vector<string> SplitIntoWordsNoStop(const string& text) const {
        // Метод для отсеивания неинформационных слов
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        // Функция рассчитывает средний рейтинг документа при добавлении
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }
 
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Текст не должен быть пустой строкой
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return {text, is_minus, IsStopWord(text)};
    }
 
    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }
 
    double CalculatingIDF(const string& word) const{
        return log(GetDocumentCount() * 1.0 / word_to_documents_freq_.at(word).size());
    }
    
    template<typename KeyMaper>
    vector<Document> FindAllDocuments(const Query& query, KeyMaper key_map) const{
        // Метод находит все релевантные документы
        map<int, double> document_to_relevance;
        // Находим все документы, релевантные запросу
        for (const auto& word : query.plus_words) {
            if(word_to_documents_freq_.count(word)==0){
                continue;
            }
            // Высчитываем релевантность TF-IDF для документа
            const double inverse_document_freq = CalculatingIDF(word);
                for (const auto [document_id, term_freq] : word_to_documents_freq_.at(word)) {
                    // Проверяем соответствие документа целевой функции key_map
                    if (key_map(document_id, documents_.at(document_id).status, documents_.at(document_id).rating)) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                    }
                }
            }
        
        // Убираем все документы, где встричаются минус слова
        for (const auto& word : query.minus_words) {
            if(word_to_documents_freq_.count(word)!=0){
                for(auto [id, value]:word_to_documents_freq_.at(word)){
                    document_to_relevance.erase(id);
                }
            }
        }
        vector<Document> matched_documents;
        for (const auto& [document_id, relevance] : document_to_relevance){
            matched_documents.push_back({document_id, relevance, documents_.at(document_id).rating});
        }
        return matched_documents;
    }
};

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
    SearchServer search_server("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "BANNED:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, DocumentStatus::BANNED)) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}
