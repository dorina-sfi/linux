/**
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2022 Dorina Sfirnaciuc
 * Copyright (C) 2021 Patrick Franz <deltaone@debian.org>
 * Copyright (C) 2020 Evgeny Groshev
 * Copyright (C) 2019 Ibrahim Fayaz <phayax@gmail.com>
**/

#include "lkc.h"
#include "cftestgenconfig.h"

#include <time.h>
#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <string.h>
#include <fstream>
#include <vector>
#include <algorithm>
#include <random>
#include <filesystem>
#include <map>
#include <iomanip>

#include <nlohmann/json.hpp>
#include "spdlog/spdlog.h"

using namespace std;
using json = nlohmann::json;
namespace fs = std::filesystem;

// Mersenne Twister random number generator, initialised once
static std::random_device::result_type seed = std::random_device{}();
static std::mt19937 rng(seed);

static std::string WORKING_PATH_RELATIVE = ".";
static std::string ROOT_PATH_RELATIVE = "..";

int main() {
    /// set the logger pattern
    spdlog::set_pattern("[%^%l%$] %v");

    /// Initialization
    ConflictGeneratorSetup conflict_generator_setup;
    conflict_generator_setup.init_default();
    conflict_generator_setup.init_from_env();
    conflict_generator_setup.print_console();

    spdlog::info("Initializing conflict generator...");
    ConflictGenerator conflict_generator;
    bool success = conflict_generator.init(conflict_generator_setup);

    /// Run
    if (success) {
        // print status
        conflict_generator.print_config_stats();
        conflict_generator.print_sample_stats();

        // generate a random conflict and save the results to .csv file
        conflict_generator.test_random_conflict();
    } else {
        spdlog::error("Conflict generator could not be initialized!");
    }
    return 0;
}

/**
 * Read default values
 */
void ConflictGeneratorSetup::init_default() {
    // paths
    working_path = get_working_path();
    root_path = get_root_path();

    kconfig_path = working_path + "/Kconfig";
    testing_path = root_path + "/tests";
    config_sample_dir = testing_path + "/x86_64/config.10";

    // parameters
    config_sample_folder = "config.";
    config_prob = "10";
    conflict_size = 1;

    // architecture
    arch = "x86_64";
    srcarch = "x86";

    // makefile values
    CC = "gcc";
    CC_Version_Text = "gcc (Ubuntu 9.3.0-17ubuntu1~20.04) 9.3.0";
    LD = "ld";
    srctree = ".";
    RUSTC = "rustc";
    kernel_version = "5.16.0-rc2";

    //set the makefile values
    setenv("CC", CC.c_str(), true);
    setenv("CC_VERSION_TEXT", CC_Version_Text.c_str(), true);
    setenv("LD", LD.c_str(), true);
    setenv("srctree", srctree.c_str(), true);
    setenv("RUSTC", RUSTC.c_str(), true);
};

/**
 * Read the environment values from the system environment
 */
void ConflictGeneratorSetup::init_from_env() {
    // paths
    working_path = getenv("working_path")?getenv("working_path"):working_path;
    root_path = getenv("root_path")?getenv("root_path"):root_path;

    kconfig_path = working_path + "/Kconfig";
    testing_path = getenv("testing_path")?getenv("testing_path"):testing_path;
    config_sample_dir = getenv("config_sample_dir")?getenv("config_sample_dir"):config_sample_dir;

    // parameters
    config_sample_folder = getenv("config_sample_folder")?getenv("config_sample_folder"):config_sample_folder;
    config_prob = getenv("config_prob")?getenv("config_prob"):config_prob;
    conflict_size = getenv("conflict_size")?std::atoi(getenv("conflict_size")):conflict_size;

    // makefile values
    ARCH = getenv("arch")?getenv("arch"):arch;
    SRCARCH = getenv("srcarch")?getenv("srcarch"):srcarch;

    // set archictecture values
    setenv("ARCH", ARCH.c_str(), true);
    setenv("SRCARCH", SRCARCH.c_str(), true);

    // output paths
    config_sample_path = config_sample_dir + "/." + config_sample_folder + config_prob;
    csv_result_path = testing_path + "/" + RESULTS_FILENAME;
    conflict_dir = get_conflict_dir(config_sample_dir);
}

/**
 * Prints environment variables and parameters that affect the testing framework
 */
void ConflictGeneratorSetup::print_console() {
    spdlog::info("---------------------------------------------------------------------------------------------");
    spdlog::info("Configfix testing enabled:");
    spdlog::info("----------------------------");
    spdlog::info("Root menu prompt:            {}", "Linux/" + ARCH + " Kernel Configuration");
    spdlog::info("Root directory:              {}", root_path);
    spdlog::info("Working directory:           {}", working_path);
    spdlog::info("Kconfig file:                {}", std::string(kconfig_path));
    spdlog::info("CC:                          {}", CC);
    spdlog::info("CC_VERSION_TEXT:             {}", CC_Version_Text);
    spdlog::info("KERNELVERSION:               {}", kernel_version);
    spdlog::info("ARCH:                        {}", ARCH);
    spdlog::info("SRCARCH:                     {}", SRCARCH);
    spdlog::info("srctree:                     {}", srctree);
    spdlog::info("Test path:                   {}", testing_path);
    spdlog::info("Results file:                {}", csv_result_path);
    spdlog::info("Configuration directory:     {}", config_sample_dir);
    spdlog::info("Conflict directory:          {}", conflict_dir);
    spdlog::info("Configuration sample:        {}", std::string(conf_get_configname()));
    spdlog::info("Test probability:            {}", config_prob);
    spdlog::info("Conflict size:               {}", conflict_size);
}

/**
 * Initialization with the Kconfig file and with the symbol list from .config
 */
bool ConflictGenerator::init(const ConflictGeneratorSetup &setup) {
    SETUP = setup;

    // set current path to the working path ../.. up
    fs::current_path(SETUP.working_path);

    conf_set_message_callback(NULL);
    // get Kconfig file
    conf_parse(SETUP.kconfig_path.c_str());

    // read config sample
    conf_read(SETUP.config_sample_path.c_str());
    base_config = config_backup();

    menu_iterator_all = menu_to_iterator(&rootmenu, optionMode::promptOpt);
    if (menu_iterator_all.empty()) {
        return false;
    } else {
        return true;
    }
}

/**
 * Print ConfigItem and symbol statistics
 */
void ConflictGenerator::print_config_stats() {
    spdlog::info("---------------------------------------------------------------------------------------------");
    spdlog::info("Configuration statistics:");
    spdlog::info("----------------------------");

    struct symbol *sym;

    // collect statistics
    int menuless = 0;
    int invisible = 0;
    int unknown = 0;
    int symbolless = 0;
    int nonchangeable = 0;
    int promptless = 0;
    int conf_item_candidates = 0;

    struct menu *menu;

    for (size_t idx = 0; idx < menu_iterator_all.size(); idx++) {
        menu = menu_iterator_all[idx];

        if (!menu) {
            menuless++;
            continue;
        }
        if (!menu_has_prompt(menu))
            promptless++;
        if (!menu_is_visible(menu))
            invisible++;
        sym = menu->sym;
        if (!sym) {
            symbolless++;
            continue;
        }
        if (sym_get_type(sym) == S_UNKNOWN)
            unknown++;
        if (!sym_is_changeable(sym))
            nonchangeable++;
        if (sym_has_conflict(sym, base_config))
            conf_item_candidates++;
    }
    spdlog::info(
            "{} ConfigItems: {}  menu-less, {}  prompt-less, {}  invisible, {} symbol-less, {} unknown type, {} non-changeable",
            menu_iterator_all.size(), menuless, promptless, invisible, symbolless, unknown, nonchangeable);

    // alternative counts by iterating symbols
    invisible = 0, unknown = 0, nonchangeable = 0, promptless = 0;
    int i;
    int sym_candidates = 0;
    int promptless_unchangeable = 0;
    int dep_mod = 0;
    int blocked_1 = 0;
    int blocked_2 = 0;
    int blocked_3 = 0;
    int count = 0;

    for_all_symbols(i, sym) {
            count++;
            if (!sym_has_prompt(sym))
                promptless++;
            if (sym->visible == no)
                invisible++;
            if (!sym_is_changeable(sym))
                nonchangeable++;
            if (sym_get_type(sym) == S_UNKNOWN)
                unknown++;
            if (sym_has_conflict(sym, base_config))
                sym_candidates++;
            if (!sym_is_changeable(sym) && !sym_has_prompt(sym))
                promptless_unchangeable++;
            if (expr_contains_symbol(sym->dir_dep.expr, &symbol_mod))
                dep_mod++;
            if (sym_has_blocked_values(sym, base_config) == 1)
                blocked_1++;
            if (sym_has_blocked_values(sym, base_config) == 2)
                blocked_2++;
            if (sym_has_blocked_values(sym, base_config) == 3)
                blocked_3++;
        }

    spdlog::info(
            "{} symbols: {}  prompt-less, {}  invisible, {} unknown type, {} non-changeable, {} prompt-less & unchangeable",
            count, promptless, invisible, unknown, nonchangeable, promptless_unchangeable);

    spdlog::info("Conflict candidates: {}  config items ({} symbols)",
                 conf_item_candidates, sym_candidates);

    // set static variables
    sym_count = count;
    no_conflict_candidates = sym_candidates;
    spdlog::info("Depend on 'mod': {} ", dep_mod);
    spdlog::info("Blocked values: 1 - {}, 2 - {}, 3 - {}, total - {} ", blocked_1, blocked_2, blocked_3,
                 (blocked_1 + blocked_2 + blocked_3));
}

/**
 * Print the statistics about the boolean and tristate values
 */
void ConflictGenerator::print_sample_stats() {

    int i;
    int count = 0;
    int invalid = 0;
    int other = 0;
    int bool_y = 0;
    int bool_n = 0;
    int tri_y = 0;
    int tri_m = 0;
    int tri_n = 0;

    struct symbol *sym;
    for_all_symbols(i, sym) {
            count++;
            const char *val = sym_get_string_value(sym);
            switch (sym_get_type(sym)) {
                case S_BOOLEAN:
                    if (strcmp(val, "y") == 0)
                        bool_y++;
                    else if (strcmp(val, "n") == 0)
                        bool_n++;
                    else
                        invalid++;
                    break;
                case S_TRISTATE:
                    if (strcmp(val, "y") == 0)
                        tri_y++;
                    else if (strcmp(val, "m") == 0)
                        tri_m++;
                    else if (strcmp(val, "n") == 0)
                        tri_n++;
                    else
                        invalid++;
                    break;
                default:
                    other++;
            }
        }
    spdlog::info("Sym count    Boolean        Tristates");
    spdlog::info("--------- ------ ------ ----- ----- -----");
    spdlog::info("               Y      N     Y     M     N");
    spdlog::info("   {}     {}  {}     {}     {}     {}",
                 count, bool_y, bool_n, tri_y, tri_m, tri_n);
    // set static variables
    if (tri_y + tri_m + tri_n > 0)
        tristates = true;
    no_enabled_symbols = bool_y + tri_y + tri_m;
}

/**
 * This function generate and resolve a number
 * of random conflicts, verify fixes, and save results to file.
 * It use uniform distribution for the conflict candidates.
 */
void ConflictGenerator::test_random_conflict(void) {
    std::uniform_int_distribution<int> dist(1, no_conflict_candidates);
        // If the current conflict_dir exists (conflict.txt was saved
        // there), this call will re-calculate that value
        SETUP.conflict_dir = get_conflict_dir(SETUP.config_sample_dir);

        // initialise result string in the cvs file
        // column 1 - Architecture
        // column 2 - Configuration sample
        // column 3 - KCONFIG_PROBABILITY used to generate the sample
        // column 4 - Symbol count
        // column 5 - Tristates
        // column 6 - No. symbols = YES | MOD
        // column 7 - No. conflict candidates
        // column 8 - Conflict filename
        // column 9 - Conflict
        // column 10 - Resolution time
        // columns 11 - No. diagnoses (non-zero)
        // columns 12 -  placeholder

        std::stringstream csv_row;
        csv_row << SETUP.ARCH << ",";
        csv_row << conf_get_configname() << ",";
        csv_row << SETUP.config_prob << ",";
        csv_row << sym_count << ",";
        csv_row << (tristates ? "YES" : "NO") << ",";
        csv_row << no_enabled_symbols << ",";
        csv_row << no_conflict_candidates << ",";

        // generate conflict & find fixes
        generate_conflict_candidate(dist);
        auto[filename, conflict_number] = save_conflict_candidate();
        auto[time, solutions_size] = calculate_fixes();

        csv_row << filename << ",";
        csv_row << conflict_number << ",";

        csv_row << std::setprecision(6) << time << ",";
        csv_row << solution_output->size << ",";

        csv_row << ",";
        // output result and continue if no solution found and if yes verify diagnosis
        if (solution_output == nullptr || solution_output->size == 0) {
            // set remaining result columns to "-"
            // column 13 - Diag. index
            // column 14 - Diag. size
            // column 15 - Resolved
            // column 16 - Applied
            csv_row << "-,-,-,-,";
            save_to_csv_file(csv_row.str());
        } else {
            verify_diagnosis_all(csv_row);
        }

    spdlog::info("Test run has finished");
}

/**
 * Generate conflicts by iteration through the menu
 */
void ConflictGenerator::generate_conflict_candidate(std::uniform_int_distribution<int> dist)
{
    int index = 0;
    while (conflict_candidate_list.size() < SETUP.conflict_size) {
        spdlog::info("Conflict ({} symbols)", conflict_candidate_list.size());
        index = dist(::rng);
        spdlog::info("Random index = {} ", index);
        struct menu *menu = get_conflict_candidate(index, menu_iterator_all, base_config);

        std::vector<std::string> tmpVar = add_symbol(menu);
        // set target value (one of the currently blocked)
        //tristate current = sym_get_tristate_value(menu->sym);
        tristate target = random_blocked_value(menu->sym);
        tmpVar.at(1) = tristate_value_to_std_string(target);
        conflict_candidate_list.push_back(tmpVar);
    }
    if (conflict_candidate_list.size() == 0) {
        spdlog::error("No conflict could be generated");
        return;
    } else {
        // print conflict
        spdlog::info("Conflict ({} symbols)", conflict_candidate_list.size());
        spdlog::info("------------------------------");
        for (unsigned int i = 0; i < conflict_candidate_list.size(); i++) {
            auto _symbol = conflict_candidate_list[i][0].c_str();
            struct symbol *sym = sym_find(_symbol);
            //print the conflict solutions
            spdlog::info("{}: {} => {}",
                         sym->name,
                         sym_get_string_value(sym),
                         tristate_get_char(std_string_value_to_tristate(conflict_candidate_list[i][1])));
            spdlog::info("------------------------------");
        }
    }
}

/**
 * Save conflicts in a conflict.txt file
 */
std::tuple<std::string, int> ConflictGenerator::save_conflict_candidate() {
    // create directory
    namespace fs = std::filesystem;
    fs::create_directories(SETUP.conflict_dir);
    // create conflict.txt file
    std::string conflict_file = SETUP.conflict_dir + "conflict.txt";
    std::ofstream outFile(conflict_file);
    if (!outFile) {
        spdlog::error("Could not create conflict file");
        return {"ERROR", 0};
    }
    // compare conflict table with conflict_size
    if (conflict_candidate_list.size() != SETUP.conflict_size) {
        spdlog::warn("Conflict table row count and conflict_size parameter mismatch");
    }
    // iterate conflicts table, write symbols to file
    for (unsigned int i = 0; i < conflict_candidate_list.size(); i++) {
        auto _symbol = conflict_candidate_list[i][0].c_str();
        struct symbol *sym = sym_find(_symbol);
        if (!sym) {
            spdlog::error("Conflict symbol %s not found {}", std::string(_symbol));
            return {"ERROR", 0};
        } else if (!sym->name) {
            spdlog::error("Conflict symbol %s not found {}", std::string(_symbol));
            return {"ERROR", 0};
        }
        outFile << sym->name << ": ";
        outFile << sym_get_string_value(sym) << " => ";
        outFile << tristate_get_char(std_string_value_to_tristate(conflict_candidate_list[i][1])) << endl;

        // output direct dependencies
        outFile << "      Direct dependencies: ";
        gstr dir_deps = str_new();
        expr_gstr_print(sym->dir_dep.expr, &dir_deps);
        outFile << str_get(&dir_deps) << endl;
        str_free(&dir_deps);

        // output reverese dependencies
        if (sym->rev_dep.expr) {
            gstr rev_deps = str_new();
            outFile << "      Reverse dependencies: ";
            expr_gstr_print(sym->rev_dep.expr, &rev_deps);
            outFile << str_get(&rev_deps) << endl;
            str_free(&rev_deps);
        }
        outFile << "\n";
    }
    outFile.close();
    spdlog::info("conflict saved to: {}", conflict_file.c_str());
    spdlog::info("------------------------------");
    std::string filename_tmp = conflict_file.c_str();
    return {filename_tmp, conflict_candidate_list.size()};
}

/**
 * The function runs the RangeFix Algorithm to solve the conflict candidates
*/
std::tuple<double, int> ConflictGenerator::calculate_fixes() {

    /// Create wanted symbols from conflict_candidate_list
    struct sdv_list *wanted_symbols = sdv_list_init();
    //loop through the rows in conflicts table adding each row into the array:
    struct symbol_dvalue *p = nullptr;
    p = static_cast<struct symbol_dvalue *>(calloc(conflict_candidate_list.size(), sizeof(struct symbol_dvalue)));
    if (!p)
        return {0, 0};
    for (unsigned int i = 0; i < conflict_candidate_list.size(); i++) {
        struct symbol_dvalue *tmp = (p + i);
        auto _symbol = conflict_candidate_list[i][0].c_str();
        struct symbol *sym = sym_find(_symbol);
        tmp->sym = sym;
        tmp->type = static_cast<symboldv_type>(sym->type == symbol_type::S_BOOLEAN ? 0 : 1);
        tmp->tri = std_string_value_to_tristate(conflict_candidate_list[i][1]);
        sdv_list_add(wanted_symbols, tmp);
    }

    /// Run the RangeFix algorithm with the created wanted_symbols
    clock_t start, end;
    double time = 0.0;
    start = clock();

    /// starting solving the conflict
    solution_output = run_satconf(wanted_symbols);

    end = clock();
    time = ((double) (end - start)) / CLOCKS_PER_SEC;
    spdlog::info("Conflict resolution time = {}", time);

    free(p);
    sdv_list_free(wanted_symbols);
    spdlog::info("solution length = {}", unsigned(solution_output->size));
    return {time, solution_output->size};
}

/**
 * Verify all present diagnoses.
 * For every diagnosis, construct and output a result string
 * assuming that values common to all diagnoses (columns 1-12)
 */
void ConflictGenerator::verify_diagnosis_all(const std::stringstream &csv_row)
{
    conflict_candidate_list.clear();
    for (unsigned int i = 0; i < solution_output->size; i++) {
        verify_diagnosis(i + 1, csv_row, sfl_list_idx(solution_output, i));
        SymbolMap initial_config = config_reset();
        if (config_compare(initial_config) != 0)
            spdlog::error("Could not reset configuration after verifying diagnosis");
        else
            spdlog::info("Restoring initial configuration... OK");
    }
}

/**
 * - save & reload config - should match
 * - applied (all symbols have target values)
 * - compare changed symbols and dependencies
 * - restoring initial configuration
 */
bool ConflictGenerator::verify_diagnosis(int i, const std::stringstream &csv_row, struct sfix_list *diag) {
    int size = diag->size;

    std::stringstream csv_row_diag;
    csv_row_diag << csv_row.str();
    csv_row_diag << i << ",";
    csv_row_diag << size << ",";

    // print diagnosis info
    spdlog::info("-------------------------------");
    spdlog::info("Diagnosis {}", i);
    print_diagnosis_symbol(diag);

    int permutation_count = 0;
    // check 1 - conflict resolved
    bool RESOLVED = false;
    // check 2 - fix fully applied
    bool APPLIED = false;
    // config reset error
    bool ERR_RESET = false;
    // save & reload config - should match
    bool CONFIGS_MATCH = false;

    /// Creates the permutation structure
    struct sfix_list *permutation = sfix_list_init();
    while (permutation_count < 2) {
        /// Copy the fixes found in the diagnosis
        permutation = sfix_list_copy(diag);
        permutation_count++;

        if (apply_fix(permutation)) {
            conf_write(".config.applied");
            /// check 1 - conflict resolved
            if (verify_resolution()) {
                RESOLVED = true;
            }
            /// check 2 - fix applied
            if (verify_fix_target_values(permutation)) {
                APPLIED = true;
            }
            // exit loop if the conflict is resolved
            if (RESOLVED)
                break;
        } else {

            SymbolMap initial_config = config_reset();
            if (config_compare(initial_config) != 0) {
                spdlog::error("Could not reset configuration after testing permutation:");
                print_diagnosis_symbol(permutation);
                ERR_RESET = true;
                break;
            }
            spdlog::info("TEST FAILED");
        }
    }
    spdlog::info("-------------------------------");
    spdlog::info("Conflict resolution status: {} ({} permutations tested)",
                 RESOLVED ? "SUCCESS" : "FAILURE", permutation_count);

    // creates the filename prefix e.g. diag09
    char diag_prefix[strlen("diagXX") + 1];
    sprintf(diag_prefix, "diag%.2d", i);

    // save diagnosis in a txt file
    save_diagnosis(diag, diag_prefix, RESOLVED);

    // save the results in results.csv file
    std::stringstream config_filename;
    config_filename << SETUP.conflict_dir;
    config_filename << ".config.";
    config_filename << diag_prefix;

    // make a backup of the configuration
    conf_write(config_filename.str().c_str());
    SymbolMap after_write = config_backup();

    // reload, compare
    conf_read(config_filename.str().c_str());

    if (config_compare(after_write) == 0)
        CONFIGS_MATCH = true;
    else
        spdlog::warn("Reloaded configuration and backup mismatch");

    csv_row_diag << (RESOLVED ? "YES" : "NO") << ",";
    csv_row_diag << (APPLIED ? "YES" : "NO") << ",";
    save_to_csv_file(csv_row_diag.str());
    return RESOLVED;
}

/**
 * Save diagnosis using filename that combines given prefix and
 * diagnosis status e.g. diag02.VALID.txt or diag08.INVALID.txt.
 * The file is saved into the 'conflict_dir'.
 */
void ConflictGenerator::save_diagnosis(struct sfix_list *diag, char *file_prefix, bool valid_diag) {
    // construct name of a file
    char filename[
            SETUP.conflict_dir.size()
            + strlen(file_prefix)
            + strlen(valid_diag ? ".VALID" : ".INVALID")
            + strlen(".txt") + 1];
    sprintf(filename, "%s%s%s.txt",
            SETUP.conflict_dir.c_str(), file_prefix,
            valid_diag ? ".VALID" : ".INVALID");
    ofstream file;
    file.open(filename);
    if (!file.is_open()) {
        spdlog::error("ERROR: could not save diagnosis");
        return;
    }
    struct symbol_fix *fix;
    struct sfix_node *node;
    sfix_list_for_each(node, diag) {
        fix = node->elem;
        if (fix->type == SF_BOOLEAN)
            file << fix->sym->name << " => " << tristate_get_char(fix->tri) << "\n";
        else if (fix->type == SF_NONBOOLEAN)
            file << fix->sym->name << " => " << str_get(&fix->nb_val) << "\n";
        else
            perror("NB not yet implemented.");
    }
    file.close();
    spdlog::info("diagnosis saved to {}", std::string(filename));
}

/**
* Check that conflict in the given table is resolved,
* i.e. all its symbols have their target values
* in the current configuration.
 */
bool ConflictGenerator::verify_resolution() {
    for (unsigned int i = 0; i < conflict_candidate_list.size(); i++) {
        auto _symbol = conflict_candidate_list[i][1].c_str();
        struct symbol *sym = sym_find(_symbol);
        tristate value = std_string_value_to_tristate(conflict_candidate_list[i][1]);
        // consider only booleans as conflict symbols
        if (value != sym_get_tristate_value(sym)) {
            spdlog::info("Conflict symbol {}: target {} != actual {}",
                         sym_get_name(sym),
                         conflict_candidate_list[i][1],
                         sym_get_string_value(sym));
            return false;
        }
    }
    return true;
}

/**
 * Save the test results in the "results.csv" file
 */
bool ConflictGenerator::save_to_csv_file(const std::string &content) {
    std::fstream csv_result_file(SETUP.csv_result_path, std::fstream::in | std::fstream::out | std::fstream::app);
    if (csv_result_file.is_open()) {
        csv_result_file << content << std::endl;
        csv_result_file.close();
        return true;
    } else {
        spdlog::error("Could not write to {}", SETUP.csv_result_path);
        return false;
    }
}

/**
* Return a row, first column=symbol name, second and third column is the value of the symbol
*/
std::vector<std::string> add_symbol(struct menu *m) {
    std::vector<std::string> tmpVar;
    if (m != nullptr) {
        if (m->sym != nullptr) {
            struct symbol *sym = m->sym;
            tristate currentval = sym_get_tristate_value(sym);
            tmpVar.clear();
            tmpVar.push_back(sym->name);
            tmpVar.push_back(tristate_value_to_std_string(currentval));
            tmpVar.push_back(tristate_value_to_std_string(currentval));
        }
    }
    return tmpVar;
}

/**
* This function iterates through the main menu, pre-order traversal
*/
MenuIterator menu_to_iterator(struct menu *menu, enum optionMode optMode = promptOpt) {
    MenuIterator menu_iterator;
    for (struct menu *child = menu->list; child; child = child->next) {
        if (optMode == promptOpt && menu_has_prompt(child)) {
            menu_iterator.push_back(child);
            menu_to_iterator(child, menu_iterator, optMode);
        }
    }
    return menu_iterator;
}

/**
* This function iterates through the submenu
*/
void menu_to_iterator(struct menu *menu, MenuIterator &menu_iterator, enum optionMode optMode = promptOpt) {
    for (struct menu *child = menu->list; child; child = child->next) {
        if (optMode == promptOpt && menu_has_prompt(child)) {
            menu_iterator.push_back(child);
            menu_to_iterator(child, menu_iterator, optMode);
        }
    }
}

/**
 * Returns the conflicting ConfigItem with the given 1-based index
 * (which can be randomly generated) from the given ConfigList,
 * assuming its consistent iteration order.
 *
 * This function iterates the ConfigList, and increases a counter
 * for every ConfigItem that holds a conflicting symbol (see
 * sym_has_conflict()).
 * When counter equals index, that ConfigItem is returned.
 */
static struct menu *get_conflict_candidate(int index, const MenuIterator &menu_iterator, const SymbolMap &base_config) {
    struct menu *menu;
    struct symbol *sym;
    int cnt = 0;
    for (size_t idx = 0; idx < menu_iterator.size(); idx++) {
        menu = menu_iterator[idx];
        // skip items without menus or symbols
        if (!menu) {
            continue;
        }
        sym = menu->sym;
        if (!sym) {
            continue;
        }
        // consider only conflicting items
        if (sym_has_conflict(sym, base_config)) {
            cnt++;
        }
        if (cnt == index) {
            return menu;
        }
    }
    return NULL;
}

/**
 * Save the current configuration (symbol values) into a map container,
 * where keys are symbol names, and values are symbol values.
 */
static SymbolMap config_backup() {
    SymbolMap backup_table;

    spdlog::info("Backing up configuration...");
    int i;
    int count = 0;
    int unknowns = 0;

    struct symbol *sym;

    std::string key;
    std::string val;
    std::string val_old;

    for_all_symbols(i, sym) {
            count++;
            if (sym_get_type(sym) == S_UNKNOWN) {
                unknowns++;
                continue;
            }
            if (sym_get_string_value(sym) == NULL) {
                continue;
            }

            key = std::string(sym_get_name(sym));
            val = std::string(sym_get_string_value(sym));

            if (backup_table.count(key) > 0) {
                val_old = backup_table[key];
                spdlog::info("Duplicate key: {} {} {}", sym_get_type_name(sym), sym_get_name(sym), sym->name);
                if (val != val_old) {
                    spdlog::info("Value has changed: {} {}", val_old, val);
                }
            }
            backup_table[key] = val;
        }
    spdlog::info("Done: iterated {} symbols, {} symbols in backup table, {} UNKNOWNs ignored",
                 count, backup_table.size(), unknowns);
    return backup_table;
}

/**
 * Reset configuration to the initial one, which was read upon program start.
 * Initial configuration filename is specified by the KCONFIG_CONFIG variable,
 * and defaults to '.config' if the variable is not set.
 */
SymbolMap config_reset() {
    conf_read(conf_get_configname());
    SymbolMap symbolMap;
    return symbolMap;
}

/**
 * Compare the current configuration with given backup.
 * Return 0 if the configuration and the backup match,
 * otherwise return number of mismatching symbols.
 */
static int config_compare(const SymbolMap &backup_table) {
    struct symbol *sym;

    int i;
    int count = 0;
    int match = 0;
    int mismatch = 0;
    int unknowns = 0;

    std::string key;
    std::string backup_val;
    std::string current_val;

    if (!backup_table.empty()) {
        for_all_symbols(i, sym) {
                count++;
                if (sym_get_type(sym) == S_UNKNOWN) {
                    unknowns++;
                    continue;
                }
                if (sym_get_string_value(sym) == NULL) {
                    continue;
                }

                key = std::string(sym_get_name(sym));
                current_val = std::string(sym_get_string_value(sym));

                if (backup_table.count(key) > 0) {
                    backup_val = backup_table.at(key);
                    if (backup_val != current_val) {
                        spdlog::info("Symbols that are mismatching key= {} initial value= {} current value= {}",
                                     key, backup_val, current_val);
                        mismatch++;
                    } else
                        match++;
                } else {
                    spdlog::info("Symbol missing in the original config for the key= {}", key);
                    mismatch++;
                }
            }
    }
    return mismatch;
}

/**
 * returns the tristate value for the symbol in a string
 */
std::string tristate_value_to_std_string(const tristate& x) {
    switch (x) {
        case no:
            return std::string("NO");
            break;
        case yes:
            return std::string("YES");
            break;
        case mod:
            return std::string("MODULE");
            break;
        default:
            break;
    }
    return "";
}

/**
 * returns the string value for the symbol in a tristate
 */
tristate std_string_value_to_tristate(const std::string& x) {
    if (x == "YES") {
        return tristate::yes;
    } else if (x == "NO") {
        return tristate::no;
    } else if (x == "MODULE") {
        return tristate::mod;
    }
}

/**
 * Return 'true' if the symbol conflicts with the current
 * configuration, 'false' otherwise.
 */
static bool sym_has_conflict(struct symbol *sym, const SymbolMap &base_config) {
    // symbol is conflicting if it
    return (
                   // has prompt (visible to user)
                   sym_has_prompt(sym) &&
                   // is bool or tristate
                   sym_is_boolean(sym) &&
                   // is not 'choice' (choice values should be used instead)
                   !sym_is_choice(sym)) && sym_has_blocked_values(sym, base_config);
}

/**
 * Returns 'true' if a bool/tristate symbol has value 'yes' or 'mod'
 * in the base configuration, 'false' otherwise.
 */
static bool sym_enabled_in_base_config(struct symbol *sym, const SymbolMap &base_config) {
    std::string key = std::string(sym_get_name(sym));

    if (base_config.count(key) == 0) {
        spdlog::error("Symbol missing in base config");
        return false;
    }
    std::string base_val = base_config.at(key);

    if (base_val == "y" || base_val == "m")
        return true;
    return false;
}

/**
 * For a visible boolean or tristate symbol, returns the number of
 * its possible values that cannot be set (not within range).
 * Otherwise, returns 0 (including other symbol types).
 */
static int sym_has_blocked_values(struct symbol *sym, const SymbolMap &base_config) {
    if (!sym_is_boolean(sym))
        return 0;
    // ignore symbols disabled in the base config
    if (!sym_enabled_in_base_config(sym, base_config))
        return 0;
    int result = 0;
    if (!sym_tristate_within_range(sym, no))
        result++;
    if (sym_get_type(sym) == S_TRISTATE &&
        !sym_tristate_within_range(sym, mod))
        result++;
    // some tristates depend on 'mod', can never be set to 'yes
    if (!expr_contains_symbol(sym->dir_dep.expr, &symbol_mod) &&
        !sym_tristate_within_range(sym, yes))
        result++;
    return result;
}

/**
 * Selects a tristate value currently blocked for given symbol.
 * For a tristate symbol, if two values are blocked, makes a random selection.
 */
static tristate random_blocked_value(struct symbol *sym) {
    tristate *values = NULL;
    int no_values = 0;
    // dynamically allocate at most 2 values (excluding current)
    if (sym_get_tristate_value(sym) != no &&
        !sym_tristate_within_range(sym, no)) {
        no_values++;
        values = (tristate *) realloc(values,
                                      no_values * sizeof(tristate));
        values[no_values - 1] = no;
    }
    if (sym_get_type(sym) == S_TRISTATE &&
        sym_get_tristate_value(sym) != mod &&
        !sym_tristate_within_range(sym, mod)) {
        no_values++;
        values = (tristate *) realloc(values,
                                      no_values * sizeof(tristate));
        values[no_values - 1] = mod;
    }
    if (sym_get_tristate_value(sym) != yes &&
        !expr_contains_symbol(sym->dir_dep.expr, &symbol_mod) &&
        !sym_tristate_within_range(sym, yes)) {
        no_values++;
        values = (tristate *) realloc(values,
                                      no_values * sizeof(tristate));
        values[no_values - 1] = yes;
    }
    switch (no_values) {
        case 1:
            return values[0];
        case 2:
            return values[rand() % no_values];
        default:
            spdlog::error("Too many random values for {}", sym_get_name(sym));
    }
    return no;
}

/**
 * Return the string representation of the given symbol's type
 */
static const char *sym_get_type_name(struct symbol *sym) {
    /**
     * This is different from sym_type_name(sym->type),
     * because sym_get_type() covers some special cases
     * related to choice values and MODULES.
     */
    return sym_type_name(sym_get_type(sym));
}

/**
 * Return value of the given symbol fix as string.
 */
static const char *sym_fix_get_string_value(struct symbol_fix *fix) {
    if (fix->type == SF_BOOLEAN)
        switch (fix->tri) {
            case no:
                return "n";
            case mod:
                return "m";
            case yes:
                return "y";
        }
    else if (fix->type == SF_NONBOOLEAN)
        return str_get(&fix->nb_val);

    spdlog::info("Cannot get value: disallowed symbol fix");
    return NULL;
}

/**
 * Check if all symbols in the diagnosis have their target values.
 */
static bool verify_fix_target_values(struct sfix_list *diag) {
    struct symbol *sym;
    struct symbol_fix *fix;
    struct sfix_node *node;

    sfix_list_for_each(node, diag) {
        fix = node->elem;
        sym = fix->sym;
        switch (sym_get_type(sym)) {
            case S_BOOLEAN:
            case S_TRISTATE:
                if (fix->tri != sym_get_tristate_value(fix->sym)) {
                    spdlog::info("Fix symbol {}: target {} != actual {}",
                                 sym_get_name(sym),
                                 sym_fix_get_string_value(fix),
                                 sym_get_string_value(sym));
                    return false;
                }
                break;
            default:
                if (strcmp(str_get(&fix->nb_val), sym_get_string_value(fix->sym)) != 0) {
                    spdlog::info("{}: target {} != actual {}",
                                 sym_get_name(sym),
                                 sym_fix_get_string_value(fix),
                                 sym_get_string_value(sym));
                    return false;
                }
        }
    }
    return true;
}

/**
 * Returns a path for saving a next conflict
 * for the current configuration sample.
 */
std::string get_conflict_dir(const std::string &config_dir) {
    // open the configuration sample directory
    struct dirent *de;
    DIR *dr = opendir(config_dir.c_str());
    if (dr == NULL) {
        spdlog::error("Could not open directory: {}", config_dir);
        return NULL;
    }
    int next_conflict_num = 1;

    while ((de = readdir(dr)) != NULL)
        // look for subdirectories
        if (de->d_type == DT_DIR
            // whose name start with 'conflict.'
            && strncmp("conflict.", de->d_name, strlen("conflict.")) == 0) {
            char *num_string = strtok(de->d_name, "conflict.");
            if (num_string) {
                int current_conflict_num = atoi(num_string);
                if (current_conflict_num >= next_conflict_num)
                    next_conflict_num = current_conflict_num + 1;
            }
        }
    closedir(dr);
    char *conflict_dir = (char *) malloc(
            sizeof(char) * (strlen(config_dir.c_str()) + strlen("/conflict.XXX/") + 1));
    sprintf(conflict_dir, "%s/conflict.%.3d/", config_dir.c_str(), next_conflict_num);
    return std::string(conflict_dir);
}

/**
 * Return the sfix_list from a specific index
 */
struct sfix_list* sfl_list_idx(struct sfl_list* list, int index)
{
    struct sfl_node* current = list->head;

    int count = 0;
    while (current != NULL) {
        if (count == index)
            return (current->elem);
        count++;
        current = current->next;
    }
    return nullptr;
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
