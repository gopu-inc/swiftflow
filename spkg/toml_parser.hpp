#ifndef TOML_PARSER_HPP
#define TOML_PARSER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <algorithm>

class TomlConfig {
public:
    // Structure : [Section] -> Clef -> Valeur
    std::map<std::string, std::map<std::string, std::string>> data;

    bool load(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) return false;

        std::string line, current_section = "default";
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;

            // Détection de section [section]
            if (line.front() == '[' && line.back() == ']') {
                current_section = line.substr(1, line.size() - 2);
            } 
            // Détection de clef = valeur
            else {
                size_t sep = line.find('=');
                if (sep != std::string::npos) {
                    std::string key = trim(line.substr(0, sep));
                    std::string val = trim(line.substr(sep + 1));
                    
                    // Nettoyage des guillemets (simple ou double)
                    if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
                        val = val.substr(1, val.size() - 2);
                    }
                    data[current_section][key] = val;
                }
            }
        }
        return true;
    }

    std::string get(const std::string& section, const std::string& key) {
        if (data.count(section) && data[section].count(key)) {
            return data[section][key];
        }
        return "";
    }

private:
    std::string trim(const std::string& s) {
        if (s.empty()) return "";
        size_t first = s.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = s.find_last_not_of(" \t\r\n");
        return s.substr(first, (last - first + 1));
    }
};

#endif

