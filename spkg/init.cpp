#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include "spkg.hpp"

void create_dir(std::string path) {
    mkdir(path.c_str(), 0777);
}

void spkg_init(std::string name, std::string version, bool is_lib) {
    std::cout << "[spkg] Initializing project: " << name << "...\n";

    // 1. Création de l'arborescence
    create_dir(name);
    create_dir(name + "/src");
    create_dir(name + "/build");
    create_dir(name + "/test");
    create_dir(name + "/script");
    if(is_lib) create_dir(name + "/" + name);

    // 2. Génération du vsproject.toml
    std::ofstream toml(name + "/vsproject.toml");
    toml << "[project]\nname = \"" << name << "\"\nversion = \"" << version << "\"\n\n";
    toml << "[build]\nenv_TARGET = \"release\"\ncommand = \"swift src/main.sf\"\n";
    toml.close();

    // 3. Génération du main.sf
    std::ofstream main_sf(name + "/src/main.sf");
    main_sf << "// Project: " << name << "\nprint(\"Hello from SwiftFlow!\");\n";
    main_sf.close();

    // 4. SwiftFlow.txt (Manifeste interne)
    std::ofstream sf_txt(name + "/SwiftFlow.txt");
    sf_txt << "type=" << (is_lib ? "library" : "executable") << "\n";
    sf_txt.close();

    std::cout << "[spkg] Project " << name << " created successfully!\n";
}

