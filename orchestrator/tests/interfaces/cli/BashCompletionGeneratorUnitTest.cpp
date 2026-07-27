/**
 * @file BashCompletionGeneratorUnitTest.cpp
 * @brief Unit tests for invariants of the generated bash completion script.
 *
 * What the script *does* is covered by completion_behavior_test.sh, which loads
 * it into a real shell and asserts on COMPREPLY, and its syntax by piping it
 * through `bash -n`; both run as CTest tests. Assertions on the generated text
 * belong here only when the behavioural test cannot observe them -- otherwise
 * they duplicate it less truthfully, as the word-splitting defect showed by
 * surviving a full green suite.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <string>

#include "interfaces/cli/BashCompletionGenerator.hpp"

using namespace testing;
using namespace chaos::orchestrator::interfaces::cli;

namespace {

std::size_t countOccurrences(const std::string& haystack, const std::string& needle) {
    std::size_t count = 0;
    for (std::size_t pos = haystack.find(needle); pos != std::string::npos; pos = haystack.find(needle, pos + 1)) {
        ++count;
    }
    return count;
}

}  // namespace

/**
 * @test Verifies container-id arguments are completed from the container engine.
 *
 * Not observable in the behavioural test: it would need a reachable Docker
 * daemon, and a machine without one cannot tell an empty candidate list from a
 * command that was never wired up.
 */
TEST(BashCompletionGeneratorUnitTest, CompletesContainerIdsForStopAndKill) {
    const std::string script = generateBashCompletion();

    EXPECT_THAT(script, HasSubstr(R"BASH(command docker ps --format '{{.ID}}')BASH"));
    EXPECT_THAT(script, HasSubstr(R"BASH(compgen -W "$(_chaos_container_ids)" -- "$cur")BASH"));
}

/**
 * @test Verifies every path completion marks its results as filenames.
 *
 * Not observable in the behavioural test: compopt changes how readline renders
 * a candidate -- trailing '/' on directories, escaped spaces -- and leaves no
 * trace in COMPREPLY. Asserted as a ratio rather than a fixed count so adding a
 * path-valued argument does not fail a correct script.
 */
TEST(BashCompletionGeneratorUnitTest, MarksEveryPathCompletionAsFilenames) {
    const std::string script = generateBashCompletion();
    const std::size_t pathCompletions = countOccurrences(script, "mapfile -t COMPREPLY < <(compgen -f");

    ASSERT_GT(pathCompletions, 0U);
    EXPECT_EQ(pathCompletions, countOccurrences(script, "compopt -o filenames"));
}

/**
 * @test Verifies no empty word list is ever emitted, which would suggest nothing.
 *
 * Not observable in the behavioural test: an empty `-W ""` and a genuinely
 * unmatched prefix both yield an empty COMPREPLY.
 */
TEST(BashCompletionGeneratorUnitTest, EmitsNoEmptyWordLists) {
    EXPECT_THAT(generateBashCompletion(), Not(HasSubstr(R"(compgen -W "")")));
}
