cmake_minimum_required(VERSION 3.28)

project(SchoolManager VERSION 0.1.0 LANGUAGES CXX)

# The workspace image has conda libraries that can shadow Ubuntu 24.04 system
# libraries at runtime. The backend targets system Drogon/SQLite packages.
list(APPEND CMAKE_IGNORE_PREFIX_PATH "/opt/conda")

add_subdirectory(backend)
