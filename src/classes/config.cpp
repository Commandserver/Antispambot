/************************************************************************************
*
 * dpp-mysql - An asynchronous MySQL database wrapper for D++ bots
 *
 * Copyright 2020-2024 Craig Edwards <signing@dpp.dev>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#include <dpp/dpp.h>
#include <dpp/json.h>
#include <fstream>

namespace config {

    static nlohmann::json configdocument;

    void init(const std::string& config_file) {
        std::ifstream configfile(config_file);
        configfile >> configdocument;
    }

    bool exists(const std::string& key) {
        return configdocument.contains(key);
    }

    nlohmann::json& get(const std::string& key) {
        if (key.empty()) {
            return configdocument;
        }
        return configdocument.at(key);
    }
};