/// @file CliParserUnitTest.cpp
/// @brief Unit tests for CLI parser command handling.

#include <gtest/gtest.h>

#include <memory>
#include <string_view>
#include <span>
#include <vector>

#include "MockContainerEngine.hpp"
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
 * @test Verifies serve command rejects invalid port strings.
 */
TEST(CliParserUnitTest, ServeRejectsInvalidPort) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "serve", "--port", "not-a-number"}), 0);
}

/**
 * @test Verifies unknown command returns a non-zero exit code.
 */
TEST(CliParserUnitTest, UnknownCommandReturnsError) {
    auto mockEngine = std::make_shared<tests::MockContainerEngine>();
    interfaces::cli::CliParser cli(mockEngine);

    EXPECT_NE(runCli(cli, {"chaos", "unknown"}), 0);
}
