/**
 * @file CliSpecUnitTest.cpp
 * @brief Unit tests for the CLI spec table and the help text rendered from it.
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "interfaces/cli/CliParser.hpp"
#include "interfaces/cli/CliSpec.hpp"
#include "interfaces/cli/UsageRenderer.hpp"

using namespace testing;
using namespace chaos::orchestrator::interfaces::cli;

namespace {

chaos::orchestrator::interfaces::cli::ParsedGlobalFlags parseFlags(std::initializer_list<const char*> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const char* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }
    return parseGlobalFlags(std::span<char*>(argv.data(), argv.size()));
}

}  // namespace

/**
 * @test Verifies every log level the table offers is actually accepted by the parser.
 *
 * This is the one place the spec table can genuinely drift from parseLevelString,
 * since completion would otherwise suggest values that fail to parse.
 */
TEST(CliSpecUnitTest, EveryLogLevelCandidateIsAcceptedByParseGlobalFlags) {
    for (const auto& level : kLogLevels) {
        const std::string value(level);
        const auto result = parseFlags({"chaos", "--log-level", value.c_str()});

        EXPECT_FALSE(result.error.has_value()) << "spec offers unparsable log level: " << level;
    }
}

/**
 * @test Verifies help aliases resolve to the help command.
 */
TEST(CliSpecUnitTest, HelpAliasesResolveToHelp) {
    ASSERT_NE(findCommand("-h"), nullptr);
    ASSERT_NE(findCommand("--help"), nullptr);

    EXPECT_EQ(findCommand("-h")->commandId, CommandId::Help);
    EXPECT_EQ(findCommand("--help")->commandId, CommandId::Help);
    EXPECT_EQ(findCommand("help")->commandId, CommandId::Help);
}

/**
 * @test Verifies words that are not commands are rejected.
 */
TEST(CliSpecUnitTest, UnknownWordsAreNotCommands) {
    EXPECT_EQ(findCommand("unknown"), nullptr);
    EXPECT_EQ(findCommand("--json"), nullptr);
    EXPECT_EQ(findCommand(""), nullptr);
}

/**
 * @test Verifies command ids are unique so the dispatch switch cannot alias two commands.
 */
TEST(CliSpecUnitTest, CommandIdsAreUnique) {
    std::set<CommandId> seen;

    for (const auto& command : kCommands) {
        EXPECT_TRUE(seen.insert(command.commandId).second) << "duplicate command id for " << command.name;
    }
}

/**
 * @test Verifies serve is owned by the entry point and every other command by the parser.
 */
TEST(CliSpecUnitTest, OnlyServeIsOwnedByTheEntryPoint) {
    for (const auto& command : kCommands) {
        const bool expected_entry_point = command.name == "serve";
        EXPECT_EQ(command.owner == CommandOwner::EntryPoint, expected_entry_point)
            << "unexpected owner for " << command.name;
    }
}

/**
 * @test Verifies choice-valued arguments always carry candidate values.
 */
TEST(CliSpecUnitTest, ChoiceArgumentsDeclareCandidates) {
    for (const auto& flag : kGlobalFlags) {
        if (flag.argKind == ArgKind::Choice) {
            EXPECT_FALSE(flag.values.empty()) << "choice flag without values: " << flag.longName;
        }
    }

    for (const auto& command : kCommands) {
        if (command.positionalKind == ArgKind::Choice) {
            EXPECT_FALSE(command.positionalWords.empty()) << "choice positional without words: " << command.name;
        }
        for (const auto& flag : command.flags) {
            if (flag.argKind == ArgKind::Choice) {
                EXPECT_FALSE(flag.values.empty()) << "choice flag without values: " << flag.longName;
            }
        }
    }
}

/**
 * @test Verifies the help text lists every visible command in the table.
 */
TEST(UsageRendererUnitTest, ListsEveryVisibleCommand) {
    const std::string usage = renderUsage();

    for (const auto& command : kCommands) {
        if (command.hidden) {
            continue;
        }
        EXPECT_THAT(usage, HasSubstr(std::string(command.name))) << "command missing from help: " << command.name;
    }
}

/**
 * @test Verifies hidden commands are omitted from the help text.
 */
TEST(UsageRendererUnitTest, OmitsHiddenCommands) {
    const std::string usage = renderUsage();

    for (const auto& command : kCommands) {
        if (!command.hidden) {
            continue;
        }
        EXPECT_THAT(usage, Not(HasSubstr(std::string(command.name)))) << "hidden command shown: " << command.name;
    }
}

/**
 * @test Verifies a hidden command still resolves and dispatches when typed in full.
 */
TEST(CliSpecUnitTest, HiddenCommandsStillResolve) {
    const CommandSpec* spec = findCommand("completion");

    ASSERT_NE(spec, nullptr);
    EXPECT_TRUE(spec->hidden);
    EXPECT_EQ(spec->commandId, CommandId::Completion);
}

/**
 * @test Verifies the help text documents every global option.
 */
TEST(UsageRendererUnitTest, DocumentsEveryGlobalFlag) {
    const std::string usage = renderUsage();

    for (const auto& flag : kGlobalFlags) {
        EXPECT_THAT(usage, HasSubstr(std::string(flag.longName)));
        EXPECT_THAT(usage, HasSubstr(std::string(flag.description)));
    }
}

/**
 * @test Verifies descriptions share a single alignment column.
 *
 * The scan starts at each row's first non-blank character rather than at a
 * fixed offset. Options without a short form are indented four extra spaces to
 * line up under those that have one, so anchoring to column 2 would silently
 * skip them -- and with them every per-command section, which is precisely
 * where a width shared across sections has to hold.
 */
TEST(UsageRendererUnitTest, DescriptionsShareOneColumn) {
    const std::string usage = renderUsage();

    std::set<std::size_t> columns;
    std::size_t rows = 0;
    std::size_t start = 0;
    while (start < usage.size()) {
        const std::size_t end = usage.find('\n', start);
        const std::string line = usage.substr(start, end - start);
        start = (end == std::string::npos) ? usage.size() : end + 1;

        // Section headings and the usage banner start at column 0; rows are indented.
        const std::size_t label = line.find_first_not_of(' ');
        if (label == std::string::npos || label == 0) {
            continue;
        }
        const std::size_t gap = line.find("  ", label);
        if (gap != std::string::npos) {
            ++rows;
            columns.insert(line.find_first_not_of(' ', gap));
        }
    }

    EXPECT_GE(rows, kGlobalFlags.size() + kCommands.size() - 1) << "alignment check skipped rows it should cover";
    EXPECT_EQ(columns.size(), 1U) << "help descriptions are not aligned to one column";
}
