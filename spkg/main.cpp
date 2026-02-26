#include <iostream>
#include <string>
#include <vector>
#include "spkg.hpp" // Assure-toi que c'est bien .hpp

// Prototypes des modules
void run_build(TomlConfig& config);
void spkg_login(std::string user, std::string pass);
void spkg_publish(TomlConfig& config);
void spkg_install(std::string package_name);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "spkg - SwiftFlow Package Manager v1.0 (gopu.inc)\n";
        std::cout << "Usage: spkg <command> [args]\n\n";
        std::cout << "Commands:\n";
        std::cout << "  login    <user> <pass>  Authenticate with zenv-hub\n";
        std::cout << "  build                   Build project using vsproject.toml\n";
        std::cout << "  publish                 Pack and upload to registry\n";
        std::cout << "  install  <name>         Download and install a package\n";
        return 0;
    }

    std::string cmd = argv[1];
    TomlConfig config;

    if (cmd == "login") {
        if (argc < 4) { std::cerr << "Usage: spkg login <user> <pass>\n"; return 1; }
        spkg_login(argv[2], argv[3]);
    } 
    else if (cmd == "build") {
        if (!config.load("vsproject.toml")) {
            std::cerr << "Error: vsproject.toml not found!\n";
            return 1;
        }
        run_build(config);
    }
    else if (cmd == "publish") {
        if (config.load("vsproject.toml")) spkg_publish(config);
    }
    else if (cmd == "install") {
        if (argc < 3) { std::cerr << "Usage: spkg install <package>\n"; return 1; }
        spkg_install(argv[2]);
    } 
else if (cmd == "init") {
    // Usage: spkg init --lib name=mylib version=1.0.0
    std::string name = "new_project";
    std::string version = "1.0.0";
    bool is_lib = false;

    for(int i=2; i<argc; i++) {
        std::string arg = argv[i];
        if(arg == "--lib") is_lib = true;
        if(arg.find("name=") == 0) name = arg.substr(5);
        if(arg.find("version=") == 0) version = arg.substr(8);
    }
    spkg_init(name, version, is_lib);
}

    else {
        std::cerr << "Unknown command: " << cmd << "\n";
    }

    return 0;
}

