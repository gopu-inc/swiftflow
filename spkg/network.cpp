// network.cpp
#include <curl/curl.h>
#include <string>
#include "spkg.hpp"
void spkg_login(std::string user, std::string pass) {
    CURL *curl = curl_easy_init();
    if(curl) {
        std::string url = "https://zenv-hub.onrender.com/api/auth/login";
        std::string json_data = "{\"username\":\"" + user + "\", \"password\":\"" + pass + "\"}";

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        CURLcode res = curl_easy_perform(curl);
        if(res != CURLE_OK) std::cerr << "Login failed: " << curl_easy_strerror(res) << std::endl;

        curl_easy_cleanup(curl);
    }
}

