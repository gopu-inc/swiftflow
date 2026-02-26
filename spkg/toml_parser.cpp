// toml_parser.hpp
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include "spkg.hpp"

class TomlConfig {
public:
    std::map<std::string, std::map<std::string, std::string>> data;

    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line, current_section;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            if (line[0] == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
            } else {
                size_t sep = line.find('=');
                if (sep != std::string::npos) {
                    std::string key = trim(line.substr(0, sep));
                    std::string val = trim(line.substr(sep + 1));
                    // Nettoyage des guillemets
                    if (val.front() == '"') val = val.substr(1, val.size() - 2);
                    data[current_section][key] = val;
                }
            }
        }
        return true;
    }

private:
    std::string trim(const std::string& s) {
        size_t first = s.find_first_not_of(" \t");
        if (first == std::string::npos) return "";
        size_t last = s.find_last_not_of(" \t");
        return s.substr(first, (last - first + 1));
    }
};

