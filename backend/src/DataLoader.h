#pragma once
#include <string>
#include <vector>

class DataLoader {
 public:
  explicit DataLoader(const std::string& dataPath);

  std::vector<std::string> ListFiles() const;

  static std::string ReadFile(const std::string& path);

 private:
  std::string dataPath; 
};