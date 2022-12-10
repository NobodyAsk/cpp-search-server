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
 
struct Document {
    int id;
    double relevance;
};
 
class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }
 
    void AddDocument(int document_id, const string& document) {
        //Метод для добавления документов в объект класса
        const vector<string> words = SplitIntoWordsNoStop(document);
        //Добавляем TF для каждого слова внутри документа
        for (auto& word:words){
            word_to_documents_freq_[word].insert({document_id, count(words.begin(), words.end(), word)*1.0/words.size()});
        }
        ++document_count_;
    }
 
    vector<Document> FindTopDocuments(const string& raw_query) const {
        // Метод для нахождения n самых подходящих документов
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);
        // Сортируем документы по релевантности
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 return lhs.relevance > rhs.relevance;
             });
        // Убираем лишние документы из списка
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }
 
private:
    /*Структуры класса*/
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
    
    set<string> stop_words_;
    
    int document_count_ = 0;
 
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
 
    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Слова не должны быть пустыми
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
 
    vector<Document> FindAllDocuments(const Query& query) const{
        // Метод находит все релевантные документы
        vector<Document> matched_documents;
        map<int, double> document_to_relevance;
        // Находим все документы, релевантные запросу
        for (const auto& word : query.plus_words) {
            if(word_to_documents_freq_.count(word)!=0){
                // Высчитываем IDF слова в документах
                double relevanceIDF = log(document_count_*1.0/word_to_documents_freq_.at(word).size());
                // Высчитываем релевантность TF-IDF для документа
                for(const auto [id, value]:word_to_documents_freq_.at(word)){
                    if(document_to_relevance[id]){
                        document_to_relevance.at(id) += relevanceIDF*value;
                    } else {
                        document_to_relevance.at(id) = relevanceIDF*value;
                    }
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
        for (auto& [key, value] : document_to_relevance){
            matched_documents.push_back({key, value});
        }
        return matched_documents;
    }
};
                       
SearchServer CreateSearchServer() {
    // Функция для создания экземпляра класса
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());
    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }
    return search_server;
}
 
int main() {
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();
    for (const auto& [document_id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << document_id << ", "
             << "relevance = "s << relevance << " }"s << endl;
    }
}