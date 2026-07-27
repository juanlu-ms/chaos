/**
 * @file CliParserUnitTest.cpp
 * @brief Unit tests for CLI parser command handling.
 */

#include <gtest/gtest.h>

#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "MockContainerEngine.hpp"
#include "history/MockRunHistory.hpp"
#include "history/RunRecord.hpp"
#include "interfaces/cli/CliParser.hpp"

using namespace testing;
using namespace chaos::orchestrator;

namespace {

int runCli(interfaces::cli::CliParser& cli, std::initializer_list<const char*> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const char* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }
    return cli.run(std::span<char*>(argv.data(), argv.size()));
}

interfaces::cli::ParsedGlobalFlags parseGlobalFlags(std::initializer_list<const char*> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (const char* arg : args) {
        argv.push_back(const_cast<char*>(arg));
    }
    return interfaces::cli::parseGlobalFlags(std::span<char*>(argv.data(), argv.size()));
}

std::vector<std::string_view> toViews(const std::vector<char*>& args) { return {args.begin(), args.end()}; }

}  // namespace

/**
 * @test Verifies missing command returns a non-zero exit code.
 */
TEST(CliParserUnitTest, ReturnsErrorOnMissingCommand) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos"}), 0);
}

/**
 * @test Verifies help command returns success.
 */
TEST(CliParserUnitTest, HandlesHelpCommand) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_EQ(runCli(cli, {"chaos", "help"}), 0);
    EXPECT_EQ(runCli(cli, {"chaos", "-h"}), 0);
    EXPECT_EQ(runCli(cli, {"chaos", "--help"}), 0);
}

/**
 * @test Verifies list command delegates to the engine.
 */
TEST(CliParserUnitTest, HandlesListCommand) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, listContainers())
        .WillOnce(Return(std::vector<containers::Container>{{"id", "name", "running"}}));

    interfaces::cli::CliParser cli(mockEngine);
    EXPECT_EQ(runCli(cli, {"chaos", "list"}), 0);
}

/**
 * @test Verifies list command returns error when engine throws.
 */
TEST(CliParserUnitTest, ListReturnsErrorWhenEngineThrows) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, listContainers())
        .WillOnce(Throw(containers::ContainerEngineTransportError("socket down")));

    interfaces::cli::CliParser cli(mockEngine);
    EXPECT_NE(runCli(cli, {"chaos", "list"}), 0);
}

/**
 * @test Verifies stop command requires a container id argument.
 */
TEST(CliParserUnitTest, StopRequiresContainerId) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "stop"}), 0);
}

/**
 * @test Verifies stop command calls stopContainer on the engine.
 */
TEST(CliParserUnitTest, StopInvokesEngine) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, stopContainer(std::string_view("abc"))).Times(1);

    interfaces::cli::CliParser cli(mockEngine);
    EXPECT_EQ(runCli(cli, {"chaos", "stop", "abc"}), 0);
}

/**
 * @test Verifies stop command returns error when engine throws.
 */
TEST(CliParserUnitTest, StopReturnsErrorWhenEngineThrows) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, stopContainer(std::string_view("abc")))
        .WillOnce(Throw(containers::ContainerEngineApiError("fail")));

    interfaces::cli::CliParser cli(mockEngine);
    EXPECT_NE(runCli(cli, {"chaos", "stop", "abc"}), 0);
}

/**
 * @test Verifies kill command requires a container id argument.
 */
TEST(CliParserUnitTest, KillRequiresContainerId) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "kill"}), 0);
}

/**
 * @test Verifies kill command calls killContainer on the engine.
 */
TEST(CliParserUnitTest, KillInvokesEngine) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, killContainer(std::string_view("abc"))).Times(1);

    interfaces::cli::CliParser cli(mockEngine);
    EXPECT_EQ(runCli(cli, {"chaos", "kill", "abc"}), 0);
}

/**
 * @test Verifies kill command returns error when engine throws.
 */
TEST(CliParserUnitTest, KillReturnsErrorWhenEngineThrows) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    EXPECT_CALL(*mockEngine, killContainer(std::string_view("abc")))
        .WillOnce(Throw(containers::ContainerEngineApiError("fail")));

    interfaces::cli::CliParser cli(mockEngine);
    EXPECT_NE(runCli(cli, {"chaos", "kill", "abc"}), 0);
}

/**
 * @test Verifies run command requires a manifest path.
 */
TEST(CliParserUnitTest, RunRequiresManifestPath) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "run"}), 0);
}

/**
 * @test Verifies unknown command returns a non-zero exit code.
 */
TEST(CliParserUnitTest, UnknownCommandReturnsError) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "unknown"}), 0);
}

/**
 * @test Verifies completion for a supported shell returns success.
 */
TEST(CliParserUnitTest, CompletionBashSucceeds) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_EQ(runCli(cli, {"chaos", "completion", "bash"}), 0);
}

/**
 * @test Verifies completion without a shell name returns a non-zero exit code.
 */
TEST(CliParserUnitTest, CompletionRequiresShellName) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "completion"}), 0);
}

/**
 * @test Verifies completion for an unsupported shell returns a non-zero exit code.
 */
TEST(CliParserUnitTest, CompletionRejectsUnknownShell) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "completion", "fish"}), 0);
}

/**
 * @test Verifies serve is not dispatched by the parser; main() intercepts it.
 */
TEST(CliParserUnitTest, ServeIsNotDispatchedByParser) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "serve"}), 0);
}

/**
 * @test Verifies -v is stripped and sets log level to debug.
 */
TEST(ParseGlobalFlagsTest, StripsVerboseFlag) {
    const auto result = parseGlobalFlags({"chaos", "-v", "run", "manifest.json"});

    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.options.log_level, spdlog::level::debug);
    EXPECT_EQ(toViews(result.args), (std::vector<std::string_view>{"chaos", "run", "manifest.json"}));
}

/**
 * @test Verifies --quiet is stripped and sets log level to warn.
 */
TEST(ParseGlobalFlagsTest, StripsQuietFlag) {
    const auto result = parseGlobalFlags({"chaos", "--quiet", "list"});

    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.options.log_level, spdlog::level::warn);
    EXPECT_EQ(toViews(result.args), (std::vector<std::string_view>{"chaos", "list"}));
}

/**
 * @test Verifies --no-color is stripped and disables color output.
 */
TEST(ParseGlobalFlagsTest, StripsNoColorFlag) {
    const auto result = parseGlobalFlags({"chaos", "--no-color", "list"});

    EXPECT_FALSE(result.error.has_value());
    EXPECT_FALSE(result.options.colorize);
    EXPECT_EQ(toViews(result.args), (std::vector<std::string_view>{"chaos", "list"}));
}

/**
 * @test Verifies --log-level consumes its value and sets the log level.
 */
TEST(ParseGlobalFlagsTest, LogLevelConsumesValue) {
    const auto result = parseGlobalFlags({"chaos", "run", "--log-level", "trace", "manifest.json"});

    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.options.log_level, spdlog::level::trace);
    EXPECT_EQ(toViews(result.args), (std::vector<std::string_view>{"chaos", "run", "manifest.json"}));
}

/**
 * @test Verifies --log-level without a value produces an error.
 */
TEST(ParseGlobalFlagsTest, ErrorOnMissingLogLevelValue) {
    const auto result = parseGlobalFlags({"chaos", "--log-level"});

    EXPECT_TRUE(result.error.has_value());
    EXPECT_EQ(toViews(result.args), (std::vector<std::string_view>{"chaos"}));
}

/**
 * @test Verifies an invalid --log-level value produces an error.
 */
TEST(ParseGlobalFlagsTest, ErrorOnInvalidLogLevel) {
    const auto result = parseGlobalFlags({"chaos", "--log-level", "banana"});

    EXPECT_TRUE(result.error.has_value());
}

/**
 * @test Verifies the program name is preserved when no flags are present.
 */
TEST(ParseGlobalFlagsTest, PreservesProgramName) {
    const auto result = parseGlobalFlags({"chaos"});

    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(toViews(result.args), (std::vector<std::string_view>{"chaos"}));
}

/**
 * @test Verifies an empty argv produces an empty args vector.
 */
TEST(ParseGlobalFlagsTest, EmptyArgv) {
    const auto result = parseGlobalFlags({});

    EXPECT_FALSE(result.error.has_value());
    EXPECT_TRUE(result.args.empty());
}

/**
 * @test Verifies global flags are recognized regardless of position.
 */
TEST(ParseGlobalFlagsTest, FlagsCanAppearAnywhere) {
    const auto result = parseGlobalFlags({"chaos", "run", "manifest.json", "-v"});

    EXPECT_FALSE(result.error.has_value());
    EXPECT_EQ(result.options.log_level, spdlog::level::debug);
    EXPECT_EQ(toViews(result.args), (std::vector<std::string_view>{"chaos", "run", "manifest.json"}));
}

/**
 * @test Verifies history command with no runs prints empty message.
 */
TEST(CliParserUnitTest, HistoryListEmpty) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    auto mockHistory = std::make_shared<tests::MockRunHistory>();
    EXPECT_CALL(*mockHistory, list()).WillOnce(Return(std::vector<history::RunSummary>{}));

    interfaces::cli::CliParser cli(mockEngine, mockHistory);
    EXPECT_EQ(runCli(cli, {"chaos", "history"}), 0);
}

/**
 * @test Verifies history command prints run id for a specific run.
 */
TEST(CliParserUnitTest, HistoryGetRun) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    auto mockHistory = std::make_shared<tests::MockRunHistory>();

    history::RunRecord record;
    record.summary.id = "run-1000";
    record.summary.status = "completed";
    record.summary.run_result.passed = true;

    EXPECT_CALL(*mockHistory, get("run-1000")).WillOnce(Return(record));

    interfaces::cli::CliParser cli(mockEngine, mockHistory);
    EXPECT_EQ(runCli(cli, {"chaos", "history", "run-1000"}), 0);
}

/**
 * @test Verifies history get for missing run returns error.
 */
TEST(CliParserUnitTest, HistoryGetMissingRun) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    auto mockHistory = std::make_shared<tests::MockRunHistory>();
    EXPECT_CALL(*mockHistory, get("run-999")).WillOnce(Return(std::nullopt));

    interfaces::cli::CliParser cli(mockEngine, mockHistory);
    EXPECT_NE(runCli(cli, {"chaos", "history", "run-999"}), 0);
}

/**
 * @test Verifies history clear delegates to mock.
 */
TEST(CliParserUnitTest, HistoryClear) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    auto mockHistory = std::make_shared<tests::MockRunHistory>();
    EXPECT_CALL(*mockHistory, clear()).Times(1);

    interfaces::cli::CliParser cli(mockEngine, mockHistory);
    EXPECT_EQ(runCli(cli, {"chaos", "history", "clear"}), 0);
}

/**
 * @test Verifies history command without history available prints error.
 */
TEST(CliParserUnitTest, HistoryNotAvailable) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    // Don't pass history (defaults to nullptr)
    interfaces::cli::CliParser cli(mockEngine);
    EXPECT_NE(runCli(cli, {"chaos", "history"}), 0);
}
