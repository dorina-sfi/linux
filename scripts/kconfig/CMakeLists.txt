cmake_minimum_required(VERSION 3.16)
project(cftestconfig C CXX)
set(PROJECT_RANDOM_CONFLICT_GENERATOR "cftestgenconfig")
set(PROJECT_RANDOM_CONFLICT_FRAMEWORK "cftestconfig")

# options
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXE_LINKER_FLAGS "-rdynamic")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../..")

# create a list of .h .c files under current folder
file(GLOB source_files CONFIGURE_DEPENDS
        "*.h"
        "*.hpp"
        "*.c"
        "*.cc"
        "*.cpp"
        )

# exclude files that have wrong libraries
list(FILTER source_files EXCLUDE REGEX ".*/gconf\\.c")

# exclude all main()
list(FILTER source_files EXCLUDE REGEX ".*/conf\\.c")
list(FILTER source_files EXCLUDE REGEX ".*/nconf\\.c")
list(FILTER source_files EXCLUDE REGEX ".*/mconf\\.c")
list(FILTER source_files EXCLUDE REGEX ".*/cfconfig\\.c")
list(FILTER source_files EXCLUDE REGEX ".*/cfcliconfig\\.c")
list(FILTER source_files EXCLUDE REGEX ".*/cfoutconfig\\.c")
list(FILTER source_files EXCLUDE REGEX ".*qconf\\.cc")
list(FILTER source_files EXCLUDE REGEX ".*qconf\\.h")
list(FILTER source_files EXCLUDE REGEX ".*/main\\.c")

list(FILTER source_files EXCLUDE REGEX ".*cftestgenconfig\\.cc")
list(FILTER source_files EXCLUDE REGEX ".*cftestgenconfig\\.h")

# exclude files for other reasons
list(FILTER source_files EXCLUDE REGEX ".*/nconf\\.gui\\.c")
list(FILTER source_files EXCLUDE REGEX ".*/qconf-moc\\.cc")
list(FILTER source_files EXCLUDE REGEX ".*/cftestconfig\\.cc")

# include cftestgenconfig
set(cftestgenconfig_source_files ${source_files})
list(APPEND cftestgenconfig_source_files cftestgenconfig.cc)
list(APPEND cftestgenconfig_source_files cftestgenconfig.h)

# include cftestconfig
set(cftestconfig_source_files ${source_files})
list(APPEND cftestconfig_source_files cftestconfig.cc)

# link packages
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Curses REQUIRED)
find_library(MENU_LIBRARY menu panel)
find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)

include_directories(${CURSES_INCLUDE_DIR})

pkg_check_modules(deps REQUIRED IMPORTED_TARGET glib-2.0)
pkg_check_modules(GTK REQUIRED gtk+-2.0 gmodule-2.0 libglade-2.0)

#####################################
### Executable cftestgenconfig
add_executable(${PROJECT_RANDOM_CONFLICT_GENERATOR} ${cftestgenconfig_source_files} )

# linking libraries
target_link_libraries(${PROJECT_RANDOM_CONFLICT_GENERATOR} ${CURSES_LIBRARIES})
target_link_libraries(${PROJECT_RANDOM_CONFLICT_GENERATOR} PkgConfig::deps)
target_link_libraries(${PROJECT_RANDOM_CONFLICT_GENERATOR} PRIVATE ${GTKMM_LIBRARIES})
target_compile_definitions(${PROJECT_RANDOM_CONFLICT_GENERATOR} PRIVATE ${GTKMM_CFLAGS_OTHER})
target_include_directories(${PROJECT_RANDOM_CONFLICT_GENERATOR} PRIVATE ${GTKMM_INCLUDE_DIRS})
target_link_directories(${PROJECT_RANDOM_CONFLICT_GENERATOR} PRIVATE ${GTKMM_LIBRARY_DIRS})
target_link_libraries (${PROJECT_RANDOM_CONFLICT_GENERATOR} Qt5::Widgets)
target_link_libraries(${PROJECT_RANDOM_CONFLICT_GENERATOR} nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_RANDOM_CONFLICT_GENERATOR} spdlog::spdlog_header_only)

#####################################
### Executable cftestconfig
add_executable(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} ${cftestconfig_source_files} )

# linking libraries
target_link_libraries(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} ${CURSES_LIBRARIES})
target_link_libraries(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} PkgConfig::deps)
target_link_libraries(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} PRIVATE ${GTKMM_LIBRARIES})
target_compile_definitions(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} PRIVATE ${GTKMM_CFLAGS_OTHER})
target_include_directories(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} PRIVATE ${GTKMM_INCLUDE_DIRS})
target_link_directories(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} PRIVATE ${GTKMM_LIBRARY_DIRS})
target_link_libraries (${PROJECT_RANDOM_CONFLICT_FRAMEWORK} Qt5::Widgets)
target_link_libraries(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} nlohmann_json::nlohmann_json)
target_link_libraries(${PROJECT_RANDOM_CONFLICT_FRAMEWORK} spdlog::spdlog_header_only)
