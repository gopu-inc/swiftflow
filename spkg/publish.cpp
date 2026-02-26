#include <string>    
#include "spkg.hpp"  

void spkg_publish(TomlConfig& config) {
    std::string name = config.data["project"]["name"];
    std::string version = config.data["project"]["version"];
    std::string archive = name + ".tar.gz";

    std::cout << "[spkg] Packaging " << name << " v" << version << "...\n";
    
    // Commande système pour créer l'archive
    std::string tar_cmd = "tar -czf " + archive + " --exclude='*.tar.gz' .";
    system(tar_cmd.c_str());

    // Ici on appelle Curl pour envoyer vers zenv-hub.onrender.com/api/package/upload
    std::cout << "[spkg] Uploading to zenv-hub...\n";
    // (Utilise le code curl précédent pour envoyer le fichier binaire)
}

