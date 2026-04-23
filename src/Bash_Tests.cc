//Bash_Tests.cc - A part of DICOMautomaton 2026. Written by OpenAI.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "doctest20251212/doctest.h"

#include "Bash.h"

namespace {

struct ScopedTempDir {
    std::filesystem::path path;

    ScopedTempDir(){
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() / ("dcma_bash_tests_" + std::to_string(stamp));
        std::filesystem::create_directories(path);
    }

    ~ScopedTempDir(){
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

} // namespace

TEST_CASE("Bash maintains cwd and environment across commands"){
    ScopedTempDir temp;
    std::filesystem::create_directories(temp.path / "work");

    Bash bash(temp.path, {{"HOME", temp.path.string()}});
    const auto result = bash.process({
        "export PROJECT=dcma",
        "cd work",
        "pwd",
        "echo $PROJECT",
        "echo $PWD",
    });

    REQUIRE(result.return_code == 0);
    REQUIRE(result.output.size() == 3);
    CHECK(result.output[0] == (temp.path / "work").string());
    CHECK(result.output[1] == "dcma");
    CHECK(result.output[2] == (temp.path / "work").string());
    REQUIRE(result.environment.count("PROJECT") == 1U);
    CHECK(result.environment.at("PROJECT") == "dcma");
}

TEST_CASE("Bash supports arrays, arithmetic, if, and while"){
    ScopedTempDir temp;
    Bash bash(temp.path, {{"HOME", temp.path.string()}});

    const auto result = bash.process(
        "items=(alpha beta); "
        "if test ${#items[@]} -eq 2; then echo ${items[1]}; else echo missing; fi; "
        "i=0; while test $i -lt 3; do echo $i; i=$((i + 1)); done"
    );

    REQUIRE(result.return_code == 0);
    REQUIRE(result.output.size() == 4);
    CHECK(result.output[0] == "beta");
    CHECK(result.output[1] == "0");
    CHECK(result.output[2] == "1");
    CHECK(result.output[3] == "2");
}

TEST_CASE("Bash builtins operate on the filesystem and support redirection"){
    ScopedTempDir temp;
    Bash bash(temp.path, {{"HOME", temp.path.string()}});

    const auto result = bash.process({
        "mkdir -p data",
        "printf 'alpha\\nbeta\\n' > data/sample.txt",
        "cat data/sample.txt | grep beta",
        "find . -name '*.txt'",
    });

    REQUIRE(result.return_code == 0);
    REQUIRE(result.output.size() >= 2);
    CHECK(result.output[0] == "beta");
    CHECK(std::find(result.output.begin(), result.output.end(), (temp.path / "data" / "sample.txt").string()) != result.output.end());

    std::ifstream is(temp.path / "data" / "sample.txt");
    REQUIRE(is.good());
}

TEST_CASE("Bash supports command substitution and glob expansion"){
    ScopedTempDir temp;
    std::ofstream(temp.path / "alpha.txt") << "x\n";
    std::ofstream(temp.path / "beta.txt") << "y\n";

    Bash bash(temp.path, {{"HOME", temp.path.string()}});
    const auto result = bash.process({
        "name=$(basename ./alpha.txt)",
        "echo $name",
        "ls *.txt",
    });

    REQUIRE(result.return_code == 0);
    REQUIRE(result.output.size() == 3);
    CHECK(result.output[0] == "alpha.txt");
    CHECK(result.output[1] == "alpha.txt");
    CHECK(result.output[2] == "beta.txt");
}
