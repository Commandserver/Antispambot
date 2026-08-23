# minimum CMake version required
cmake_minimum_required(VERSION 3.15)
# Project name, version and description
project(Antispambot VERSION 1.1 DESCRIPTION "A discord bot to prevent spam")

set(DPP_CORO ON)

# Add DPP as dependency
add_subdirectory(libs/DPP)
add_subdirectory(libs/spdlog)

# Create an executable
add_executable(${PROJECT_NAME}
        src/main.cpp
        # your others files...
        src/utils.hpp
        src/classes/ConfigSet.cpp
        src/classes/ConfigSet.h
        src/classes/ButtonHandler.hpp
        src/classes/CachedGuildMember.cpp
        src/commands/info.hpp
        src/commands/config.hpp
        src/commands/massban.hpp
        src/commands/masskick.hpp
        src/classes/database.cpp
        src/classes/database.h
        src/classes/config.cpp
        src/classes/config.h
        src/events/onMessageCreate.hpp
        src/classes/domain_blacklist.cpp
        src/classes/domain_blacklist.h
)

# Linking libraries
target_link_libraries(${PROJECT_NAME}
        dpp
        spdlog # if you need a logger. Don't forget to clone sources
        # in the `libs/` directory
        mysqlclient
        )

# Specify includes
target_include_directories(${PROJECT_NAME} PRIVATE
        libs/DPP/include
        libs/spdlog/include # Like before, if you need spdlog
        )

# Set C++ version
set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
        )
