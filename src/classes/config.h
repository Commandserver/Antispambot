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
#pragma once
#include <dpp/json_fwd.h>

namespace config {

    /**
     * @brief Initialise config file
     *
     * @param config_file Config file to read
     */
    void init(const std::string& config_file);

    /**
     * @brief Get all config values from a specific key
     *
     * @param key The key, if empty/omitted the root node is returned
     * @return json& configuration or empty container if not found
     */
    nlohmann::json& get(const std::string& key = "");

    /**
     * Returns true if the specified key exists
     * @param key
     * @return true if key exists
     */
    bool exists(const std::string& key);

};