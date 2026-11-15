#include "InvertedIndex.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <set>

std::vector<std::string> InvertedIndex::Tokenize(const std::string& text) {
  std::vector<std::string> words;
  std::string word;

  for (unsigned char c : text) {
    if (std::isalnum(c)) {
      word += static_cast<char>(std::tolower(c));
    } else {
      if (word.size() > 2) {
        words.push_back(word);
      }
      word.clear();
    }
  }
  if (word.size() > 2) {
    words.push_back(word); 
  }
  return words;
}

// Беремо перші 200 байтів тексту для фрагмента
static std::string MakePreview(const std::string& text) {
  if (text.size() <= 200) return text;
  std::size_t length = 200;

  while (length > 0 && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80) {
    length--;
  }
  return text.substr(0, length);
}

void InvertedIndex::AddDocument(int id, const std::string& text) {
  // Рахуємо, скільки разів кожне слово є в цьому документі
  std::map<std::string, int> counts;
  for (const std::string& word : Tokenize(text)) {
    counts[word]++;
  }

  {
    std::unique_lock<std::shared_mutex> lock(indexLock);
    for (const auto& pair : counts) {
      index[pair.first].push_back({id, pair.second});
    }
    snippets[id] = MakePreview(text);
  } 

  documentCount++; 
}

struct Match { 
  int docId = 0;
  int matchedWords = 0;  // скільки різних слів запиту є в документі
  int totalCount = 0;    // скільки разів усі ці слова зустрілись в документі
};


static bool IsBetter(const Match& a, const Match& b) {
  if (a.matchedWords != b.matchedWords) {
    return a.matchedWords > b.matchedWords;
  }
  if (a.totalCount != b.totalCount) {
    return a.totalCount > b.totalCount;
  }
  return a.docId < b.docId;
}

std::vector<SearchResult> InvertedIndex::Search(const std::vector<std::string>& words) const {

  std::set<std::string> uniqueWords(words.begin(), words.end());
  std::vector<SearchResult> results;

  std::shared_lock<std::shared_mutex> lock(indexLock);

  std::map<int, Match> matches;

  for (const std::string& word : uniqueWords) {
    auto found = index.find(word);
    if (found == index.end()) continue;

    for (const auto& entry : found->second) {
      int docId = entry.first;
      int count = entry.second;

      Match& match = matches[docId]; 
      match.docId = docId;
      match.matchedWords += 1;
      match.totalCount += count;
    }
  }

  std::vector<Match> sorted;

  for (const auto& pair : matches) {
    sorted.push_back(pair.second);
  }

  std::sort(sorted.begin(), sorted.end(), IsBetter);

  if ((int)sorted.size() > kMaxResults) {
    sorted.resize(kMaxResults);
  }

  for (const Match& match : sorted) {
    SearchResult result;

    result.docId = match.docId;
    result.score = match.matchedWords;
    auto snippet = snippets.find(match.docId);

    if (snippet != snippets.end()) {
      result.snippet = snippet->second;
    }

    results.push_back(result);
  }
  return results;
}

void InvertedIndex::Clear() {
  std::unique_lock<std::shared_mutex> lock(indexLock);
  index.clear();
  snippets.clear();
  documentCount = 0;
}

int InvertedIndex::DocumentCount() const {
  return documentCount.load();
}