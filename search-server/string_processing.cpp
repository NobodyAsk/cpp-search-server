#include <string>
#include <string_view>
#include <vector>
#include "string_processing.h"


std::vector<std::string> SplitIntoWords(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

std::vector<std::string_view> SplitIntoWords(std::string_view text)
{
    std::string_view delims = " ";
    std::vector<std::string_view> output;
    size_t first = 0;
    while (first < text.size())
    {
        const auto second = text.find_first_of(delims, first);
        if (first != second) output.emplace_back(text.substr(first, second - first));
        if (second == std::string_view::npos) break;
        first = second + 1;
    }
    return output;
}


std::set<std::string> MakeUniqueNonEmptyStrings(std::vector<std::string_view> strings) {
    std::set<std::string> non_empty_strings;
    for (std::string_view str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(static_cast<std::string>(str));
        }
    }
    return non_empty_strings;
}
