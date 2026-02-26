// builder.cpp
#include <cstdlib>
#include <iostream>
#include <string>    
#include "spkg.hpp"  

void run_build(TomlConfig& config) {
    auto build_section = config.data["build"];
    
    // 1. Injection des Variables d'Environnement
    // Imaginons dans le TOML: env_TARGET = "release"
    for (auto const& [key, val] : build_section) {
        if (key.rfind("env_", 0) == 0) { // Si la clé commence par env_
            std::string env_name = key.substr(4);
            setenv(env_name.c_str(), val.c_str(), 1);
            std::cout << "[spkg] Set ENV: " << env_name << "=" << val << std::endl;
        }
    }

    // 2. Exécution de la commande
    std::string cmd = build_section["command"];
    if (cmd.empty()) {
        std::cerr << "[spkg] Error: No build command found in vsproject.toml" << std::endl;
        return;
    }

    std::cout << "[spkg] Executing: " << cmd << std::endl;
    int status = std::system(cmd.c_str());
    
    if (status == 0) std::cout << "[spkg] Build Successful!" << std::endl;
    else std::cerr << "[spkg] Build Failed (Code: " << status << ")" << std::endl;
}

