#include <iostream>
#include <string>
#include <curl/curl.h>
#include "spkg.hpp"

// Fonction de rappel pour écrire le fichier téléchargé
size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    return fwrite(ptr, size, nmemb, stream);
}

void spkg_install(std::string package_name) {
    std::string scope = "public"; // Par défaut
    std::string version = "1.0.0"; // Idéalement, parser le nom (ex: pkg@version)
    
    // 1. URL de téléchargement (zenv-hub)
    std::string url = "https://zenv-hub.onrender.com/package/download/" + scope + "/" + package_name + "/" + version;
    std::string archive_name = package_name + ".tar.gz";

    std::cout << "[spkg] Downloading " << package_name << "..." << std::endl;

    CURL *curl = curl_easy_init();
    if(curl) {
        FILE *fp = fopen(archive_name.c_str(), "wb");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        
        CURLcode res = curl_easy_perform(curl);
        fclose(fp);
        curl_easy_cleanup(curl);

        if(res == CURLE_OK) {
            std::cout << "[spkg] Installing to /usr/local/lib/swift/..." << std::endl;
            
            // 2. Création du dossier de destination
            system("mkdir -p /usr/local/lib/swift");

            // 3. Décompression vers le dossier cible
            // -C change le répertoire de destination de tar
            std::string tar_cmd = "tar -xzf " + archive_name + " -C /usr/local/lib/swift/";
            int status = system(tar_cmd.c_str());

            if(status == 0) {
                std::cout << "[spkg] " << package_name << " installed successfully!" << std::endl;
                // Nettoyage de l'archive temporaire
                remove(archive_name.c_str());
            } else {
                std::cerr << "[spkg] Extraction failed." << std::endl;
            }
        } else {
            std::cerr << "[spkg] Download failed: " << curl_easy_strerror(res) << std::endl;
        }
    }
}

