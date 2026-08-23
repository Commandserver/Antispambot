#pragma once
#include <dpp/dpp.h>
#include <set>

namespace domain_blacklist {
    static std::set<std::string> domains;

    /**
     * @brief Initialise config file
     *
     * @param config_file Config file to read
     */
    void init(const std::string& config_file);
}
