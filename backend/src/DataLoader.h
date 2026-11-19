#pragma once

#include <string>
#include <vector>

struct Document {
  int id = 0;
  std::string filename;
  std::string text;
};

class DataLoader {
 public:
  explicit DataLoader(const std::string& dataPath);

  std::vector<std::string> ListFiles() const;
  std::string ReadFile(const std::string& relativePath) const;

 private:
  std::string dataPath; 
};