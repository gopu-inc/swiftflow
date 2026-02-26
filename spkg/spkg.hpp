#ifndef SPKG_HPP
#define SPKG_HPP

#include <string>
#include <vector>
#include <map>
#include "toml_parser.hpp"

// Prototypes globaux
void run_build(TomlConfig& config);
void spkg_login(std::string user, std::string pass);
void spkg_publish(TomlConfig& config);
void spkg_install(std::string package_name);
void spkg_init(std::string name, std::string version, bool is_lib);

#endif

