#include "InvertedIndex.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <mutex>
#include <set>

void IndexShard::AddTerm(const std::string& term, int docId, int count) {
  std::unique_lock<std::shared_mutex> lock(mutex);
  terms[term].push_back({docId, count});
}

std::vector<Posting> IndexShard::GetPostings(const std::string& term) const {
  std::shared_lock<std::shared_mutex> lock(mutex);
  auto found = terms.find(term);
  if (found == terms.end()) return {};
  return found->second;  // копія, щоб читати її вже без блокування
}

void IndexShard::Clear() {
  std::unique_lock<std::shared_mutex> lock(mutex);
  terms.clear();
}

std::vector<std::string> InvertedIndex::Tokenize(const std::string& text) {
  std::vector<std::string> words;
  std::string word;

  for (unsigned char c : text) {
    if (std::isalnum(c)) {
      word += static_cast<char>(std::tolower(c));
    } else {
      if (word.size() > 2) words.push_back(word);
      word.clear();
    }
  }

  if (word.size() > 2) words.push_back(word);
  return words;
}

// Беремо перші 200 байтів тексту для фрагмента
static std::string MakePreview(const std::string& text) {
  if (text.size() <= 200) return text;

  std::size_t length = 200;
  while (length > 0 &&
         (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80) {
    --length;
  }
  return text.substr(0, length);
}

std::size_t InvertedIndex::ShardIndex(const std::string& term) {
  return std::hash<std::string>{}(term) % kShardCount;
}

void InvertedIndex::AddDocument(const Document& doc) {
  std::shared_lock<std::shared_mutex> indexLock(indexMutex);

  std::unordered_map<std::string, int> counts;
  for (const std::string& word : Tokenize(doc.text)) {
    ++counts[word];
  }

  for (const auto& entry : counts) {
    shards[ShardIndex(entry.first)].AddTerm(entry.first, doc.id, entry.second);
  }

  // Документ стає доступним для пошуку після додавання всіх слів.
  std::unique_lock<std::shared_mutex> docsLock(docsMutex);
  docs[doc.id] = {doc.filename, MakePreview(doc.text)};
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

std::vector<SearchResult> InvertedIndex::Search(
    const std::vector<std::string>& words) const {
  std::shared_lock<std::shared_mutex> indexLock(indexMutex);
  std::shared_lock<std::shared_mutex> docsLock(docsMutex);

  std::set<std::string> uniqueWords(words.begin(), words.end());
  std::unordered_map<int, Match> matches;

  for (const std::string& word : uniqueWords) {
    for (const Posting& posting : shards[ShardIndex(word)].GetPostings(word)) {
      if (docs.find(posting.docId) == docs.end()) continue;

      Match& match = matches[posting.docId];
      match.docId = posting.docId;
      ++match.matchedWords;
      match.totalCount += posting.count;
    }
  }

  std::vector<Match> sorted;
  sorted.reserve(matches.size());
  for (const auto& entry : matches) {
    sorted.push_back(entry.second);
  }
  std::sort(sorted.begin(), sorted.end(), IsBetter);

  if (sorted.size() > kMaxResults) {
    sorted.resize(kMaxResults);
  }

  std::vector<SearchResult> results;
  results.reserve(sorted.size());
  for (const Match& match : sorted) {
    const DocInfo& info = docs.at(match.docId);
    SearchResult result;
    result.docId = info.filename;
    result.score = static_cast<double>(match.matchedWords);
    result.snippet = info.snippet;

    results.push_back(result);
  }
  return results;
}

void InvertedIndex::Clear() {
  std::unique_lock<std::shared_mutex> indexLock(indexMutex);

  for (IndexShard& shard : shards) {
    shard.Clear();
  }

  std::unique_lock<std::shared_mutex> docsLock(docsMutex);
  docs.clear();
}

int InvertedIndex::DocumentCount() const {
  std::shared_lock<std::shared_mutex> lock(docsMutex);
  return static_cast<int>(docs.size());
}