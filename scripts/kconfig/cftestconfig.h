/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2022 Dorina Sfirnaciuc
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
 * Copyright (C) 2020 Evgeny Groshev
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
**/
#include <string>

struct ConflictFrameworkSetup{
    std::string working_path;
    std::string root_path;

    std::string testing_path;
    std::string config_sample_dir;

    std::string config_sample_folder;
    std::string config_prob;

    std::string mode;
    std::string arch;
    std::string srcarch;
    std::int64_t num_threads;
    std::int64_t num_conflicts;
    std::int64_t num_config_prob;
    std::int64_t min_conf_size;
    std::int64_t max_conf_size;

    void init_default();
    void init_console();
};

class ConflictFramework {
private:
    ConflictFrameworkSetup SETUP;

public:
    ConflictFramework() = default;
    ~ConflictFramework() = default;

    bool init(const ConflictFrameworkSetup& setup);

    void run_mode_1();
    void run_mode_2();
    void run_mode_3();

    void run_tests();
};

//static functions
static std::string exec_cmd(const char* cmd);
static void generate_random_config();
static void test_config_resolution();

static std::string get_working_path();
static std::string get_root_path();
