// OptimizeSchedule_Tests.cc.

#include "doctest20251212/doctest.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Operations/OptimizeSchedule.h"
#include "Structs.h"

namespace {

using rows_t = std::vector<std::vector<std::string>>;

struct test_lexicon_t {
    std::filesystem::path path = std::filesystem::temp_directory_path() / "dcma_optimize_schedule_tests.lexicon";

    test_lexicon_t(){
        std::ofstream stream(path);
        stream << "optimized schedule : optimized schedule\n";
        if(!stream) throw std::runtime_error("unable to create OptimizeSchedule test lexicon");
    }

    ~test_lexicon_t(){
        std::error_code error;
        std::filesystem::remove(path, error);
    }
};

const std::string &test_lexicon(){
    static const test_lexicon_t lexicon;
    static const std::string path = lexicon.path.string();
    return path;
}

std::shared_ptr<Sparse_Table> make_table(const rows_t &rows){
    auto out = std::make_shared<Sparse_Table>();
    for(std::size_t r = 0; r < rows.size(); ++r){
        for(std::size_t c = 0; c < rows[r].size(); ++c){
            if(!rows[r][c].empty()) out->table.inject(static_cast<int64_t>(r), static_cast<int64_t>(c), rows[r][c]);
        }
    }
    out->table.metadata["TableLabel"] = "source";
    out->table.metadata["Sentinel"] = "unchanged";
    return out;
}

OperationArgPkg make_args(const std::map<std::string, std::string> &overrides = {}){
    OperationArgPkg args("OptimizeSchedule");
    for(const auto &entry : overrides) REQUIRE(args.insert(entry.first, entry.second));
    for(const auto &arg : OpArgDocOptimizeSchedule().args){
        if(arg.expected) args.insert(arg.name, arg.default_val);
    }
    return args;
}

void run(Drover &d, const std::map<std::string, std::string> &overrides = {}){
    auto args = make_args(overrides);
    std::map<std::string, std::string> invocation_metadata;
    REQUIRE(OptimizeSchedule(d, args, invocation_metadata, test_lexicon()));
}

std::vector<std::vector<std::string>> report_rows(const Sparse_Table &table, const std::string &kind){
    std::vector<std::vector<std::string>> out;
    if(table.table.data.empty()) return out;
    const auto rb = table.table.min_max_row();
    const auto cb = table.table.min_max_col();
    for(int64_t r = rb.first; r <= rb.second; ++r){
        if(table.table.value(r, 0).value_or("") != "Schedule Optimizer Report") continue;
        if(table.table.value(r, 1).value_or("") != kind) continue;
        std::vector<std::string> fields;
        for(int64_t c = 0; c <= cb.second; ++c) fields.push_back(table.table.value(r, c).value_or(""));
        out.push_back(std::move(fields));
    }
    return out;
}

std::string summary_value(const Sparse_Table &table, const std::string &key){
    for(const auto &row : report_rows(table, "Summary")){
        if(row.size() > 3U && row[2] == key) return row[3];
    }
    throw std::runtime_error("missing OptimizeSchedule summary row: " + key);
}

std::vector<std::string> component_row(const Sparse_Table &table, const std::string &label){
    for(const auto &row : report_rows(table, "Component")){
        if(row.size() > 7U && row[3] == label) return row;
    }
    throw std::runtime_error("missing OptimizeSchedule component row: " + label);
}

double as_double(const std::string &value){
    std::size_t consumed = 0;
    const auto out = std::stod(value, &consumed);
    REQUIRE(consumed == value.size());
    return out;
}

void check_rejects_unchanged(const rows_t &rows,
                             const std::map<std::string, std::string> &overrides = {}){
    Drover d;
    auto source = make_table(rows);
    d.table_data.push_back(source);
    const auto before_data = source->table.data;
    const auto before_metadata = source->table.metadata;
    auto args = make_args(overrides);
    std::map<std::string, std::string> invocation_metadata;
    CHECK_THROWS_AS(OptimizeSchedule(d, args, invocation_metadata, test_lexicon()), std::invalid_argument);
    REQUIRE(d.table_data.size() == 1U);
    CHECK(d.table_data.front() == source);
    CHECK(source->table.data == before_data);
    CHECK(source->table.metadata == before_metadata);
}

} // namespace

TEST_CASE("OptimizeSchedule documents its complete public argument contract"){
    const auto doc = OpArgDocOptimizeSchedule();
    CHECK(doc.name == "OptimizeSchedule");
    std::set<std::string> names;
    for(const auto &arg : doc.args) names.insert(arg.name);
    CHECK(names == std::set<std::string>{"TableSelection", "RandomSeed", "Iterations", "RuntimeSeconds",
                                         "OutputSchedules", "ParetoArchiveSize", "TemperatureStart",
                                         "TemperatureEnd", "RestartCount", "TableLabel"});
    CHECK(doc.args.size() == 10U);
}

TEST_CASE("OptimizeSchedule loads and resolves the provided schedule template"){
#ifdef DCMA_SOURCE_DIR
    const auto fixture = std::filesystem::path(DCMA_SOURCE_DIR).parent_path()
                       / "artifacts" / "test_files" / "20260821_DCMA_schedule_template.tsv";
#else
    const auto fixture = std::filesystem::current_path()
                       / "artifacts" / "test_files" / "20260821_DCMA_schedule_template.tsv";
#endif
    std::ifstream stream(fixture, std::ios::binary);
    REQUIRE(stream.good());

    auto source = std::make_shared<Sparse_Table>();
    source->table.read_csv(stream);
    std::size_t constraints = 0, headers = 0, dates = 0, holidays = 0;
    for(const auto &cell : source->table.data){
        if(cell.get_col() == 0 && cell.val == "Constraint") ++constraints;
        if(cell.get_col() == 0 && cell.val == "Date") ++headers;
        if(cell.get_col() == 0 && cell.val.find(", 2026") != std::string::npos) ++dates;
        if(cell.val == "Holiday") ++holidays;
    }
    CHECK(constraints == 11U);
    CHECK(headers == 5U);
    CHECK(dates == 25U);
    CHECK(holidays == 22U);

    Drover d;
    d.table_data.push_back(source);
    const auto source_data = source->table.data;
    run(d, { {"Iterations", "300"}, {"OutputSchedules", "3"}, {"ParetoArchiveSize", "32"},
             {"RestartCount", "2"}, {"RandomSeed", "20260821"} });
    REQUIRE(d.table_data.size() == 4U);
    CHECK(source->table.data == source_data);
    for(auto it = std::next(d.table_data.begin()); it != d.table_data.end(); ++it){
        CHECK(report_rows(**it, "Component").size() == 11U);
        CHECK(report_rows(**it, "StaffTally").size() == 11U);
        CHECK(report_rows(**it, "ExcludedDay").empty());
        CHECK(summary_value(**it, "active-days") == "23");
    }
}

TEST_CASE("OptimizeSchedule resolves a tiny schedule without modifying its source"){
    Drover d;
    auto source = make_table({
        {"Constraint", "minimum_onsite", "100", "any 1 of A or B"},
        {"Constraint", "fairness_overrides", "1", ""},
        {"Date", "A", "B", "C"},
        {"2026-08-24", "x", "Pref", "  Vac  "},
    });
    const auto source_data = source->table.data;
    const auto source_metadata = source->table.metadata;
    d.table_data.push_back(source);

    run(d, {{"Iterations", "80"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "8"},
            {"RandomSeed", "17"}, {"RestartCount", "1"}, {"TableLabel", "Tiny"}});

    REQUIRE(d.table_data.size() == 2U);
    CHECK(d.table_data.front() == source);
    CHECK(source->table.data == source_data);
    CHECK(source->table.metadata == source_metadata);
    const auto &out = **std::next(d.table_data.begin());
    CHECK(out.table.value(3, 1) == "Onsite");
    CHECK(out.table.value(3, 2) == "Remote");
    CHECK(out.table.value(3, 3) == "  Vac  ");
    CHECK(out.table.metadata.at("TableLabel") == "Tiny 1");
    CHECK(out.table.metadata.at("ScheduleOptimizerSeed") == "17");
    CHECK(out.table.metadata.at("ScheduleOptimizerResultIndex") == "1");
    CHECK(out.table.metadata.count("NormalizedTableLabel") == 1U);
    CHECK(out.table.metadata.at("Description").find("decision-support") != std::string::npos);
    CHECK(summary_value(out, "result") == "1/1");
    CHECK(summary_value(out, "mutable-cells") == "2");
    CHECK(summary_value(out, "preference-overrides") == "0");
    CHECK(summary_value(out, "weighted-objective") == "0");
    CHECK(report_rows(out, "Component").size() == 2U);
    CHECK(report_rows(out, "StaffTally").size() == 3U);
}

TEST_CASE("OptimizeSchedule reports hand-computable minimum group and exclusivity scores"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "2", "any 2 of A or B or C"},
        {"Constraint", "group(team)", "3", "any 2 of A or B or C"},
        {"Constraint", "exclusivity(room)", "5", "any 1 of A xor B xor C"},
        {"Date", "A", "B", "C", "D"},
        {"2026-08-24", "Remote", "Remote", "Remote", "x"},
        {"2026-08-25", "Onsite", "Remote", "Remote", "x"},
        {"2026-08-26", "Onsite", "Onsite", "Onsite", "x"},
    }));
    run(d, {{"Iterations", "30"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "8"}, {"RestartCount", "1"}});
    const auto &out = **std::next(d.table_data.begin());
    CHECK(as_double(component_row(out, "minimum_onsite")[5]) == doctest::Approx(0.5));
    CHECK(as_double(component_row(out, "group(team)")[5]) == doctest::Approx(0.5));
    CHECK(as_double(component_row(out, "exclusivity(room)")[5]) == doctest::Approx(1.0 / 3.0));
    CHECK(report_rows(out, "DayViolation").size() == 5U);
}

TEST_CASE("OptimizeSchedule consecutive runs cross weekends and reset at excluded breaks"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "max_consecutive_remote", "1", "1", "statuses=Away"},
        {"Date", "A", "B", "C"},
        {"2026-08-28", "Away", "Onsite", "x"},
        {"2026-08-31", "Away", "Onsite", "x"},
        {"2026-09-01", "Holiday", "Holiday", "Holiday"},
        {"2026-09-02", "Away", "Onsite", "x"},
        {"2026-09-03", "Away", "Onsite", "x"},
    }));
    run(d, {{"Iterations", "20"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "8"},
            {"RestartCount", "1"}});
    const auto &out = **std::next(d.table_data.begin());
    CHECK(as_double(component_row(out, "max_consecutive_remote")[5]) == doctest::Approx(1.0 / 6.0));
    const auto violations = report_rows(out, "DayViolation");
    REQUIRE(violations.size() == 2U);
    CHECK(violations[0][2] == "2026-08-31");
    CHECK(violations[1][2] == "2026-09-03");
    CHECK(report_rows(out, "ExcludedDay").empty());
}

TEST_CASE("OptimizeSchedule weekly limits use repeated schedule headers"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "max_weekly_remote", "1", "A=1", "statuses=Away"},
        {"Date", "A", "B"},
        {"2020-12-31", "Away", "x"},
        {"2021-01-01", "Away", "x"},
        {"Date", "A", "B"},
        {"2021-01-04", "Away", "x"},
    }));
    run(d, {{"Iterations", "20"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "8"}, {"RestartCount", "1"}});
    const auto &out = **std::next(d.table_data.begin());
    const auto weekly = report_rows(out, "Weekly");
    REQUIRE(weekly.size() == 2U);
    CHECK(weekly[0][2] == "Week 1");
    CHECK(weekly[0][5] == "2");
    CHECK(weekly[0][7] == "1");
    CHECK(weekly[1][2] == "Week 2");
    CHECK(weekly[1][5] == "1");
    CHECK(as_double(component_row(out, "max_weekly_remote")[5]) == doctest::Approx(1.0 / 3.0));
}

TEST_CASE("OptimizeSchedule fairness uses each staff member's mutable eligibility"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "fairness_remote", "1", ""},
        {"Date", "A", "B"},
        {"2026-08-24", "x", "x"},
        {"2026-08-25", "x", "Fixed"},
    }));
    run(d, {{"Iterations", "40"}, {"OutputSchedules", "8"}, {"ParetoArchiveSize", "16"},
            {"RestartCount", "1"}, {"RandomSeed", "9"}});
    REQUIRE(d.table_data.size() == 9U);
    const Sparse_Table *chosen = nullptr;
    for(auto it = std::next(d.table_data.begin()); it != d.table_data.end(); ++it){
        const auto &t = (*it)->table;
        const auto a0 = t.value(2, 1).value_or("");
        const auto b0 = t.value(2, 2).value_or("");
        const auto a1 = t.value(3, 1).value_or("");
        if(a0 == "Remote" && b0 == "Remote" && a1 == "Onsite") chosen = it->get();
    }
    REQUIRE(chosen != nullptr);
    CHECK(as_double(component_row(*chosen, "fairness_remote")[5]) == doctest::Approx(0.25));
    CHECK(summary_value(*chosen, "fairness-source-row-0-staff-A").find("denominator=2") != std::string::npos);
    CHECK(summary_value(*chosen, "fairness-source-row-0-staff-B").find("denominator=1") != std::string::npos);
}

TEST_CASE("OptimizeSchedule reports all preference overrides and weighted contributions"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "100", "any 2 of A or B"},
        {"Constraint", "fairness_overrides", "3", ""},
        {"Date", "A", "B"},
        {"2026-08-24", "Pref", "Pref"},
    }));
    run(d, {{"Iterations", "80"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "8"}, {"RestartCount", "1"}});
    const auto &out = **std::next(d.table_data.begin());
    CHECK(out.table.value(3, 1) == "Onsite*");
    CHECK(out.table.value(3, 2) == "Onsite*");
    CHECK(report_rows(out, "Override").size() == 2U);
    CHECK(summary_value(out, "preference-overrides") == "2");
    CHECK(as_double(component_row(out, "fairness_overrides")[5]) == doctest::Approx(0.5));
    double contribution_sum = 0.0;
    for(const auto &row : report_rows(out, "Component")) contribution_sum += as_double(row[6]);
    CHECK(as_double(summary_value(out, "weighted-objective")) == doctest::Approx(contribution_sum));
    CHECK(as_double(out.table.metadata.at("ScheduleOptimizerObjective")) == doctest::Approx(contribution_sum));
}

TEST_CASE("OptimizeSchedule fixed seeds reproduce complete iteration-mode reports"){
    const rows_t rows = {
        {"Constraint", "minimum_onsite", "4", "any 1 of A or B"},
        {"Constraint", "fairness_remote", "1", ""},
        {"Date", "A", "B"},
        {"2026-08-24", "x", "Pref"},
        {"2026-08-25", "Pref", "x"},
    };
    Drover a, b;
    a.table_data.push_back(make_table(rows));
    b.table_data.push_back(make_table(rows));
    const std::map<std::string, std::string> args = {{"Iterations", "120"}, {"OutputSchedules", "3"},
        {"ParetoArchiveSize", "12"}, {"RestartCount", "2"}, {"RandomSeed", "123456"}};
    run(a, args);
    run(b, args);
    REQUIRE(a.table_data.size() == b.table_data.size());
    auto ai = std::next(a.table_data.begin());
    auto bi = std::next(b.table_data.begin());
    for(; ai != a.table_data.end(); ++ai, ++bi){
        CHECK((*ai)->table.metadata == (*bi)->table.metadata);
        CHECK(summary_value(**ai, "actual-iterations") == summary_value(**bi, "actual-iterations"));
        CHECK(summary_value(**ai, "actual-seconds") == "not-applicable (deterministic iteration mode)");
        CHECK((*ai)->table.data == (*bi)->table.data);
    }
}

TEST_CASE("OptimizeSchedule runtime mode respects its end-to-end limit on a small schedule"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "1", "any 1 of all"},
        {"Date", "A", "B"},
        {"2026-08-24", "x", "Pref"},
    }));
    const auto started = std::chrono::steady_clock::now();
    run(d, {{"Iterations", "0"}, {"RuntimeSeconds", "0.05"}, {"OutputSchedules", "1"},
            {"ParetoArchiveSize", "4"}, {"RestartCount", "1"}});
    const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    REQUIRE(d.table_data.size() == 2U);
    const auto &out = **std::next(d.table_data.begin());
    CHECK(summary_value(out, "search-mode") == "runtime");
    CHECK(as_double(summary_value(out, "actual-seconds")) <= 0.10);
    CHECK(elapsed <= 0.30);
}

TEST_CASE("OptimizeSchedule emits all unique schedules in an undersized one-variable space"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Date", "A"},
        {"2026-08-24", "x"},
    }));
    run(d, {{"Iterations", "10"}, {"OutputSchedules", "5"}, {"ParetoArchiveSize", "8"}, {"RestartCount", "1"}});
    REQUIRE(d.table_data.size() == 3U);
    std::set<std::string> assignments;
    for(auto it = std::next(d.table_data.begin()); it != d.table_data.end(); ++it){
        assignments.insert((*it)->table.value(1, 1).value_or(""));
        CHECK(summary_value(**it, "result").find("/2") != std::string::npos);
    }
    CHECK(assignments == std::set<std::string>{"Onsite", "Remote"});
}

TEST_CASE("OptimizeSchedule fills requested alternatives despite fallback collisions"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Date", "A", "B", "C", "D", "E"},
        {"2026-08-24", "x", "x", "x", "x", "x"},
    }));
    run(d, {{"Iterations", "1"}, {"OutputSchedules", "20"}, {"ParetoArchiveSize", "20"},
            {"RestartCount", "1"}, {"RandomSeed", "41"}});
    REQUIRE(d.table_data.size() == 21U);
    std::set<std::vector<std::string>> assignments;
    for(auto it = std::next(d.table_data.begin()); it != d.table_data.end(); ++it){
        std::vector<std::string> assignment;
        for(int64_t col = 1; col <= 5; ++col) assignment.push_back((*it)->table.value(1, col).value_or(""));
        assignments.insert(std::move(assignment));
        CHECK(summary_value(**it, "result").find("/20") != std::string::npos);
    }
    CHECK(assignments.size() == 20U);
}

TEST_CASE("OptimizeSchedule handles sparse coordinates and constraints after schedule blocks"){
    Drover d;
    auto source = std::make_shared<Sparse_Table>();
    source->table.inject(10, 5, "Date");
    source->table.inject(10, 6, "A");
    source->table.inject(10, 8, "B");
    source->table.inject(12, 5, "Thu, Feb 29, 2024");
    source->table.inject(12, 6, "Onsite*");
    source->table.inject(12, 8, "x");
    source->table.inject(14, 5, "Constraint");
    source->table.inject(14, 6, "minimum_onsite");
    source->table.inject(14, 7, "1");
    source->table.inject(14, 8, "any 1 of all");
    d.table_data.push_back(source);
    const auto original = source->table.data;

    run(d, {{"Iterations", "10"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "4"},
            {"RestartCount", "1"}});

    REQUIRE(d.table_data.size() == 2U);
    CHECK(source->table.data == original);
    const auto &out = **std::next(d.table_data.begin());
    CHECK(out.table.value(10, 5) == "Date");
    CHECK(out.table.value(10, 7) == std::nullopt);
    CHECK(out.table.value(12, 5) == "Thu, Feb 29, 2024");
    CHECK(out.table.value(12, 6) == "Onsite*");
    CHECK(out.table.value(12, 8).value_or("") != "x");
    CHECK(report_rows(out, "Component").size() == 1U);
}

TEST_CASE("OptimizeSchedule zero-weight constraints are advisory and emit no violations"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "0", "any 2 of A or B"},
        {"Date", "A", "B", "C"},
        {"2026-08-24", "Remote", "Remote", "x"},
    }));
    run(d, {{"Iterations", "10"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "4"}, {"RestartCount", "1"}});
    const auto &out = **std::next(d.table_data.begin());
    const auto component = component_row(out, "minimum_onsite");
    CHECK(as_double(component[5]) == doctest::Approx(1.0));
    CHECK(component[7] == "disabled/advisory");
    CHECK(summary_value(out, "direct-violations") == "0");
    CHECK(report_rows(out, "DayViolation").empty());
}

TEST_CASE("OptimizeSchedule reports impossible coverage instead of throwing"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "1", "any 2 of A or B"},
        {"Date", "A", "B", "C"},
        {"2026-08-24", "Remote", "Remote", "x"},
    }));
    run(d, {{"Iterations", "10"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "4"}, {"RestartCount", "1"}});
    const auto &out = **std::next(d.table_data.begin());
    const auto feasibility = report_rows(out, "Feasibility");
    REQUIRE(feasibility.size() == 1U);
    CHECK(feasibility[0][2] == "2026-08-24");
    CHECK(feasibility[0][5] == "0");
    CHECK(feasibility[0][6] == "2");
}

TEST_CASE("OptimizeSchedule terminates with conflicting onsite and remote coverage"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "1", "any 2 of A or B", "statuses=Onsite"},
        {"Constraint", "group(remote)", "1", "any 2 of A or B", "statuses=Remote"},
        {"Date", "A", "B"},
        {"2026-08-24", "x", "x"},
    }));
    run(d, {{"Iterations", "100"}, {"OutputSchedules", "3"}, {"ParetoArchiveSize", "8"}, {"RestartCount", "2"}});
    REQUIRE(d.table_data.size() == 4U);
}

TEST_CASE("OptimizeSchedule rejects malformed inputs without changing the Drover"){
    SUBCASE("repeated header layout"){
        check_rejects_unchanged({{"Date", "A"}, {"2026-08-24", "x"}, {"Date", "B"}, {"2026-08-25", "x"}});
    }
    SUBCASE("staff label"){
        check_rejects_unchanged({{"Date", "A", "a"}, {"2026-08-24", "x", "x"}});
    }
    SUBCASE("policy"){
        check_rejects_unchanged({{"Constraint", "minimum_onsite", "1", "any 1 of all", "unknown=Onsite"},
                                 {"Date", "A"}, {"2026-08-24", "x"}});
    }
    SUBCASE("constraint number"){
        check_rejects_unchanged({{"Constraint", "minimum_onsite", "NaN", "any 1 of all"},
                                 {"Date", "A"}, {"2026-08-24", "x"}});
    }
    SUBCASE("duplicate weekly staff"){
        check_rejects_unchanged({{"Constraint", "max_weekly_remote", "1", "A=1, a=2"},
                                 {"Date", "A"}, {"2026-08-24", "x"}});
    }
    SUBCASE("argument number"){
        check_rejects_unchanged({{"Date", "A"}, {"2026-08-24", "x"}}, {{"Iterations", "not-a-number"}});
    }
    SUBCASE("pre-existing report"){
        check_rejects_unchanged({{"Date", "A"}, {"2026-08-24", "x"}, {"Schedule Optimizer Report"}});
    }
    SUBCASE("missing staff cell"){
        check_rejects_unchanged({{"Date", "A", "B"}, {"2026-08-24", "x"}});
    }
    SUBCASE("no mutable cells"){
        check_rejects_unchanged({{"Date", "A"}, {"2026-08-24", "Onsite"}});
    }
}

TEST_CASE("OptimizeSchedule rejects zero or multiple selected tables without mutation"){
    auto args = make_args();
    std::map<std::string, std::string> invocation_metadata;
    Drover empty;
    CHECK_THROWS_AS(OptimizeSchedule(empty, args, invocation_metadata, test_lexicon()), std::invalid_argument);
    CHECK(empty.table_data.empty());

    Drover multiple;
    const rows_t rows = {{"Date", "A"}, {"2026-08-24", "x"}};
    multiple.table_data.push_back(make_table(rows));
    multiple.table_data.push_back(make_table(rows));
    const auto first = multiple.table_data.front()->table.data;
    const auto last = multiple.table_data.back()->table.data;
    args = make_args({{"TableSelection", "all"}});
    CHECK_THROWS_AS(OptimizeSchedule(multiple, args, invocation_metadata, test_lexicon()), std::invalid_argument);
    REQUIRE(multiple.table_data.size() == 2U);
    CHECK(multiple.table_data.front()->table.data == first);
    CHECK(multiple.table_data.back()->table.data == last);
}

TEST_CASE("OptimizeSchedule passes every row containing Holiday through unchanged"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "10", "any 1 of A or B"},
        {"Date", "A", "B", "C"},
        {"opaque holiday label", "Holiday", "x", ""},
        {"not a date", "x", "x", "Remote"},
    }));
    run(d, {{"Iterations", "10"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "4"},
            {"RestartCount", "1"}});
    const auto &out = **std::next(d.table_data.begin());
    CHECK(summary_value(out, "active-days") == "1");
    CHECK(summary_value(out, "mutable-cells") == "2");
    CHECK(report_rows(out, "ExcludedDay").empty());
    CHECK(out.table.value(2, 0) == "opaque holiday label");
    CHECK(out.table.value(2, 1) == "Holiday");
    CHECK(out.table.value(2, 2) == "x");
    CHECK(out.table.value(2, 3) == std::nullopt);
    CHECK(out.table.value(3, 1).value_or("") != "x");
    CHECK(out.table.value(3, 2).value_or("") != "x");
}

TEST_CASE("OptimizeSchedule passes through an entirely holiday schedule without searching"){
    Drover d;
    auto source = make_table({
        {"Constraint", "minimum_onsite", "10", "any 1 of A or B"},
        {"Date", "A", "B"},
        {"closed", "Holiday", ""},
    });
    const auto original = source->table.data;
    d.table_data.push_back(source);
    run(d);

    REQUIRE(d.table_data.size() == 2U);
    CHECK(source->table.data == original);
    const auto &out = **std::next(d.table_data.begin());
    CHECK(out.table.value(2, 0) == "closed");
    CHECK(out.table.value(2, 1) == "Holiday");
    CHECK(out.table.value(2, 2) == std::nullopt);
    CHECK(summary_value(out, "active-days") == "0");
    CHECK(summary_value(out, "mutable-cells") == "0");
    CHECK(summary_value(out, "actual-iterations") == "0");
    CHECK(report_rows(out, "DayViolation").empty());
    CHECK(report_rows(out, "Feasibility").empty());
}

TEST_CASE("OptimizeSchedule uses opaque row labels in reports without date parsing"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "1", "any 2 of A or B"},
        {"Date", "A", "B"},
        {"2026-02-30", "Remote", "x"},
        {"duplicate opaque label", "Remote", "x"},
        {"duplicate opaque label", "Remote", "x"},
    }));
    run(d, {{"Iterations", "10"}, {"OutputSchedules", "1"}, {"ParetoArchiveSize", "4"},
            {"RestartCount", "1"}});
    const auto &out = **std::next(d.table_data.begin());
    const auto feasibility = report_rows(out, "Feasibility");
    REQUIRE(feasibility.size() == 3U);
    CHECK(feasibility[0][2] == "2026-02-30");
    CHECK(feasibility[1][2] == "duplicate opaque label");
    CHECK(feasibility[2][2] == "duplicate opaque label");
}

TEST_CASE("OptimizeSchedule emits objective-ranked viable alternatives"){
    Drover d;
    d.table_data.push_back(make_table({
        {"Constraint", "minimum_onsite", "100", "any 2 of A or B or C"},
        {"Constraint", "fairness_remote", "1", ""},
        {"Date", "A", "B", "C"},
        {"day", "x", "x", "x"},
    }));
    run(d, {{"Iterations", "100"}, {"OutputSchedules", "3"}, {"ParetoArchiveSize", "16"},
            {"RestartCount", "2"}, {"RandomSeed", "7"}});
    REQUIRE(d.table_data.size() == 4U);
    for(auto it = std::next(d.table_data.begin()); it != d.table_data.end(); ++it){
        std::size_t onsite = 0;
        for(int64_t col = 1; col <= 3; ++col) onsite += ((*it)->table.value(3, col) == "Onsite");
        CHECK(onsite >= 2U);
        CHECK(as_double(summary_value(**it, "weighted-objective")) < 1.0);
    }
}
