/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2022 Dorina Sfirnaciuc
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
 * Copyright (C) 2020 Evgeny Groshev
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
**/

#include "configfix.h"
#include <vector>
#include <string>
#include <random>
#include <map>
#include <sstream>
#include <tuple>

typedef std::vector<menu*> MenuIterator;
typedef std::map<std::string, std::string> SymbolMap;
typedef std::vector<std::vector<std::string>> ConflictCandidateList;

enum optionMode {
    promptOpt
};

struct ConflictGeneratorSetup{
    std::string working_path;
    std::string root_path;

    std::string kconfig_path;
    std::string testing_path;
    std::string config_sample_dir;

    std::string config_sample_folder;
    std::string config_prob;
    unsigned int conflict_size;

    std::string arch;
    std::string srcarch;

    std::string ARCH;
    std::string SRCARCH;

    std::string CC;
    std::string CC_Version_Text;
    std::string LD;
    std::string srctree;
    std::string RUSTC;
    std::string kernel_version;

    std::string config_sample_path;

    std::string RESULTS_FILENAME = "results.csv";
    std::string csv_result_path;
    std::string conflict_dir;

    void init_default();
    void init_from_env();
    void print_console();
};

class ConflictGenerator {
private:
    ConflictGeneratorSetup SETUP;
    SymbolMap base_config;
    MenuIterator menu_iterator_all;
    ConflictCandidateList conflict_candidate_list;
    struct sfl_list* solution_output{nullptr};

    // Persistent configuration statistics
     int sym_count = 0;
    // tristates in configuration space
     bool tristates = false;
    // no. symbols with the value YES or MOD
     int no_enabled_symbols = 0;
    // no. symbols that conflict with current config
     int no_conflict_candidates = 0;

public:
    ConflictGenerator() = default;
    ~ConflictGenerator() = default;

    bool init(const ConflictGeneratorSetup& setup);

    void print_config_stats();
    void print_sample_stats();

    void test_random_conflict();

    void generate_conflict_candidate(std::uniform_int_distribution<int> dist);
    std::tuple<std::string,int> save_conflict_candidate();
    std::tuple<double, int>  calculate_fixes();

    void verify_diagnosis_all(const std::stringstream &csv_row);
    bool verify_diagnosis(int i, const std::stringstream& csv_row, struct sfix_list *diag);
    void save_diagnosis(struct sfix_list *diag, char *file_prefix, bool valid_diag);
    bool verify_resolution();

    bool save_to_csv_file(const std::string& content);
};

// static functions
static std::vector<std::string> add_symbol(struct menu * m);
static MenuIterator menu_to_iterator(struct menu *menu, enum optionMode optMode);
static void menu_to_iterator(struct menu *menu, MenuIterator &menu_iterator, enum optionMode optMode);
static struct menu *get_conflict_candidate(int index, const MenuIterator &menu_iterator, const SymbolMap &base_config);

static SymbolMap config_backup(void);
SymbolMap config_reset(void);
static int config_compare(const SymbolMap &backup_table);

static std::string tristate_value_to_std_string(const tristate& x);
static tristate std_string_value_to_tristate(const std::string& x);

static bool sym_has_conflict(struct symbol *sym, const SymbolMap &base_config);
static bool sym_enabled_in_base_config(struct symbol *sym, const SymbolMap &base_config);
static int sym_has_blocked_values(struct symbol *sym, const SymbolMap &base_config);

static tristate random_blocked_value(struct symbol *sym);
static const char *sym_get_type_name(struct symbol *sym);
static const char *sym_fix_get_string_value(struct symbol_fix *sym_fix);
static bool verify_fix_target_values(struct sfix_list *diag);

static std::string get_conflict_dir(const std::string &config_dir);
static struct sfix_list* sfl_list_idx(struct sfl_list* list, int index);

static std::string get_working_path();
static std::string get_root_path();


