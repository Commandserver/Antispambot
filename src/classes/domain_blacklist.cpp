#include "domain_blacklist.h"

namespace domain_blacklist {
    void init(const std::string& config_file) {
        std::ifstream configfile(config_file);
        std::string configdocument;
        configfile >> configdocument;
        auto lines = dpp::utility::tokenize(configdocument);
        for (const auto& line : lines) {
            domains.insert(line);
        }
    }
}