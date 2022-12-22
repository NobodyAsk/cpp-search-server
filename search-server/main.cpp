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
    int id;
    double relevance;
    int rating;
};

// Перечислитель для статусов документов
enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};
 
class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }
 
    void AddDocument(
        int document_id,
        const string& document, 
        DocumentStatus status, 
        const vector<int>&ratings) {
        //Метод для добавления документов в объект класса
        const vector<string> words = SplitIntoWordsNoStop(document);
        //Добавляем TF для каждого слова внутри документа
        const double inv_word_count = 1.0 / words.size();
        for (auto& word:words){
            word_to_documents_freq_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
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

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        // Функция для сравнения документа с параметрами поиска
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

/*--------------------- для примера ------------------------*/
void PrintDocument(const Document& document) {
    cout << "{ "s
         << "document_id = "s << document.id << ", "s
         << "relevance = "s << document.relevance << ", "s
         << "rating = "s << document.rating
         << " }"s << endl;
}

int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s,        DocumentStatus::ACTUAL, {8, -3});
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    search_server.AddDocument(3, "ухоженный скворец евгений"s,         DocumentStatus::BANNED, {9});
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