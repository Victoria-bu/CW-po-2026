#include "DataLoader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace fs = std::filesystem;

DataLoader::DataLoader(const std::string& dataPath) : dataPath(dataPath) {}

std::vector<std::string> DataLoader::ListFiles() const {
  std::vector<std::string> files;

  if (!fs::exists(dataPath)) {
    return files;
  }

  for (const auto& entry : fs::recursive_directory_iterator(dataPath)) {
    if (!entry.is_regular_file()) continue;

    std::string filename = entry.path().filename().string();

    if (!filename.empty() && filename[0] == '.') continue;
    if (entry.path().extension() != ".txt") continue;

    files.push_back(fs::relative(entry.path(), dataPath).generic_string());
  }

  // Сортуємо, щоб кожен документ при кожному запуску мав той самий номер
  std::sort(files.begin(), files.end());
  return files;
}

std::string DataLoader::ReadFile(const std::string& relativePath) const {
  std::ifstream file(dataPath + "/" + relativePath);

  if (!file.is_open()) {
    throw std::runtime_error("Cannot open file: " + relativePath);
  }

  std::ostringstream content;
  content << file.rdbuf();

  if (file.bad()) {
    throw std::runtime_error("Cannot read file: " + relativePath);
  }

  return content.str();
}