#include "DataLoader.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

DataLoader::DataLoader(const std::string& dataPath) : dataPath(dataPath) {}

std::vector<std::string> DataLoader::ListFiles() const {
  std::vector<std::string> files;

  if (!fs::exists(dataPath)) {
    return files;
  }

  for (const auto& entry : fs::recursive_directory_iterator(dataPath)) {
    if (!entry.is_regular_file()) continue;                        
    if (entry.path().filename().string()[0] == '.') continue;   
    if (entry.path().extension() != ".txt") continue;              
    files.push_back(entry.path().string());
  }

  // Сортуємо, щоб кожен документ при кожному запуску мав той самий номер
  std::sort(files.begin(), files.end());
  return files;
}

std::string DataLoader::ReadFile(const std::string& path) {
  std::ifstream file(path);
  std::ostringstream content;
  content << file.rdbuf(); 
  return content.str();
}