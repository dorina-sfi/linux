/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2022 Dorina Sfirnaciuc
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
 * Copyright (C) 2020 Evgeny Groshev
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
**/

#include <iostream>
#include <fstream>
#include <thread>
#include "cftestconfig.h"
#include <string>
#include <filesystem>

#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string WORKING_PATH_RELATIVE = ".";
static std::string ROOT_PATH_RELATIVE = "..";

int main() {
    /// set the logger pattern
    spdlog::set_pattern("[%^%l%$] %v");

    /// Initialization
    ConflictFrameworkSetup conflict_framework_setup;
    conflict_framework_setup.init_default();
    conflict_framework_setup.init_console();

    spdlog::info("Initializing testing framework...");
    ConflictFramework conflict_framework;
    conflict_framework.init(conflict_framework_setup);

    /// Run
    conflict_framework.run_tests();

    return 0;
}

/**
 * Initializing default values
 */
void ConflictFrameworkSetup::init_default() {
    // paths
    working_path = get_working_path();
    root_path = get_root_path();

    testing_path = root_path + "/tests";
    config_sample_dir = testing_path + "/x86_64/config.10";

    // values needed by program
    config_sample_folder = "config.";
    config_prob = "10";

    mode = "2";
    arch = "x86_64";
    srcarch = "x86";
    num_threads = 1;
    num_conflicts = 1;
    num_config_prob = 9;
    min_conf_size = 1;
    max_conf_size = 1;
}

/**
 * Initializing default/console values
 */
void ConflictFrameworkSetup::init_console() {
    ///get mode
    mode = getenv("mode")?getenv("mode"):mode;

    ///get architecture
    arch = getenv("arch")?getenv("arch"):arch;

    /// get source architecture
    srcarch = getenv("srcarch")?getenv("srcarch"):srcarch;

    /// get number of threads
    num_threads = getenv("num_threads")?atoi(getenv("num_threads")):num_threads;

    /// get number of conflicts
    num_conflicts = getenv("num_conflicts")?atoi(getenv("num_conflicts")):num_conflicts;

    /// get number of configuration probability
    num_config_prob = getenv("num_config_prob")?atoi(getenv("num_config_prob")):num_config_prob;

    /// get min conflict size
    min_conf_size = getenv("min_conf_size")?atoi(getenv("min_conf_size")):min_conf_size;

    /// get max conflict size
    max_conf_size = getenv("max_conf_size")?atoi(getenv("max_conf_size")):max_conf_size;
}

/**
 * Set the values as environment variable
 */
bool ConflictFramework::init(const ConflictFrameworkSetup &setup) {
    SETUP = setup;
    fs::current_path(SETUP.working_path);

    // paths
    setenv("working_path", SETUP.working_path.c_str(), true);
    setenv("root_path", SETUP.root_path.c_str(), true);

    setenv("testing_path", SETUP.testing_path.c_str(), true);
    setenv("config_sample_dir", SETUP.config_sample_dir.c_str(), true);

    // parameters
    setenv("config_sample_folder", SETUP.config_sample_folder.c_str(), true);
    setenv("config_prob", SETUP.config_prob.c_str(), true);

    // makefile
    setenv("arch", SETUP.arch.c_str(), true);
    setenv("srcarch", SETUP.srcarch.c_str(), true);

    return true;
}

/**
 * Run the tests in 3 different ways
 */
void ConflictFramework::run_tests() {
    //get current working directory
    spdlog::info("Current working directory: {}", SETUP.working_path);

    //get test directory
    spdlog::info("Current test directory: {}", SETUP.testing_path);

    if (SETUP.mode == "1") {
        run_mode_1();
    } else if (SETUP.mode == "2") {
        run_mode_2();
    } else if (SETUP.mode == "3") {
        run_mode_3();
    }
}

/**
 * Mode 1: Generates conflicts for an already existing configuration
 */
void ConflictFramework::run_mode_1() {
    for (int conflict_size = SETUP.min_conf_size; conflict_size <= SETUP.max_conf_size; conflict_size++) {
    setenv("conflict_size", std::to_string(conflict_size).c_str(), true);
        for (int j = 0; j < SETUP.num_conflicts; j++) {
            test_config_resolution();
        }
    }
}

/**
 * Mode 2: Generates configuration and conflicts for a given architecture
 */
void ConflictFramework::run_mode_2() {
    setenv("ARCH", SETUP.arch.c_str(), true);
    setenv("SRCARCH", SETUP.srcarch.c_str(), true);

    // generate
    for (int i = 1; i <= SETUP.num_config_prob; i++) {
        generate_random_config();
        int prob = i * 10;
        SETUP.config_prob = to_string(prob);
        SETUP.config_sample_dir = SETUP.testing_path + "/" + SETUP.arch + "/" + SETUP.config_sample_folder + to_string(prob);

        fs::create_directories(SETUP.config_sample_dir);
        string oldfile = SETUP.working_path + "/.config";
        string newfile = SETUP.config_sample_dir + "/" + "." + SETUP.config_sample_folder + to_string(prob);

        if (rename(oldfile.c_str(), newfile.c_str()) != 0){
            perror("Error renaming file");
        }

        setenv("config_sample_dir", SETUP.config_sample_dir.c_str(), true);
        setenv("config_prob", SETUP.config_prob.c_str(), true);

        generate_random_config();
        for (int conflict_size = SETUP.min_conf_size; conflict_size <= SETUP.max_conf_size; conflict_size++) {
            setenv("conflict_size", std::to_string(conflict_size).c_str(), true);
            for (int j = 0; j < SETUP.num_conflicts; j++) {
                test_config_resolution();
            }
        }
    }
}

/**
 * Mode 3: Generates configuration and conflicts with a given list of architecture
 */
void ConflictFramework::run_mode_3() {
    std::string arch_list_test = SETUP.testing_path + "/arch_list_3_arch.json";
    std::ifstream fsConfig(arch_list_test);
    json config_arch = json::parse(fsConfig);

    for (auto &elem: config_arch.items()) {
        std::string key = elem.key();
        json val = elem.value();
        for (auto &element: val.items()) {
            SETUP.arch = val["ARCH"];
            SETUP.srcarch = val["SRCARCH"];
            std::string key = element.key();
            std::string value = element.value();
            setenv(key.c_str(), value.c_str(), true);
        }

        for (int i = 1; i <= SETUP.num_config_prob; i++) {
            generate_random_config();

            int prob = i * 10;
            SETUP.config_prob = to_string(prob);
            SETUP.config_sample_dir =
                    SETUP.testing_path + "/" + SETUP.arch + "/" + SETUP.config_sample_folder + to_string(prob);

            namespace fs = std::filesystem;
            fs::create_directories(SETUP.config_sample_dir);

            string oldfile = SETUP.working_path + "/.config";
            string newfile = SETUP.config_sample_dir + "/" + "." + SETUP.config_sample_folder + to_string(prob);

            if (rename(oldfile.c_str(), newfile.c_str()) != 0)
                perror("Error renaming file");

            setenv("arch", SETUP.arch.c_str(), true);
            setenv("srcarch", SETUP.srcarch.c_str(), true);
            setenv("config_sample_dir", SETUP.config_sample_dir.c_str(), true);
            setenv("config_prob", SETUP.config_prob.c_str(), true);

            generate_random_config();

            vector<thread> threads;
            for (int conflict_size = SETUP.min_conf_size; conflict_size <= SETUP.max_conf_size; conflict_size++) {
                setenv("conflict_size", std::to_string(conflict_size).c_str(), true);
                for (int j = 0; j < SETUP.num_conflicts; j++) {
                    threads.push_back(thread(&test_config_resolution));
                    std::this_thread::sleep_for(std::chrono::milliseconds(600));
                    if (j % SETUP.num_threads == 0 || j == SETUP.num_conflicts - 1) {
                        for (auto &th: threads) {
                            th.join();
                            std::this_thread::sleep_for(std::chrono::milliseconds(600));
                        }
                        threads.clear();
                    }
                }
            }
        }
    }
}

/**
 * Execute the command
 */
std::string exec_cmd(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

/**
 * Call the conf script with the make command and prints the output
 */
void generate_random_config(){

    std::string cmds = "make randconfig";
    std::string out = exec_cmd(cmds.c_str());
    std::cout << out << std::endl;
}

/**
 * Call the tgenconfig script with the make command and prints the output
 */
void test_config_resolution(){

    std::string cmds= "make cftestgenconfig";
    std::string out = exec_cmd(cmds.c_str());
    std::cout << out << std::endl;
}

/**
 * Get the working path
 */
std::string get_working_path()
{
    fs::path cwd = fs::current_path();
    fs::path working_path_relative(WORKING_PATH_RELATIVE);
    fs::path working_path = cwd / working_path_relative;
    working_path = fs::canonical(working_path);

    return working_path.string();
}

/**
 * Get the root path
 */
std::string get_root_path()
{
    fs::path cwd = fs::current_path();
    fs::path root_path_relative(ROOT_PATH_RELATIVE);
    fs::path root_path = cwd / root_path_relative;
    root_path = fs::canonical(root_path);

    return root_path.string();
}
