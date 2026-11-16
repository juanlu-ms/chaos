/**
 * @file TestPreconditionsUnitTest.cpp
 * @brief Unit tests for the Docker/root checks that let smoke and e2e tests skip themselves.
 *
 * Every test passes its own socket path and uid, so none of them touches the real Docker socket.
 */

#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <string>

#include "support/TestPreconditions.hpp"

using namespace chaos::orchestrator::tests::support;

namespace {

constexpr uid_t ROOT_UID = 0;
constexpr uid_t REGULAR_UID = 1000;

/** @brief Owns a private temporary directory and, on request, a Unix socket inside it. */
class TestPreconditionsUnitTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::string pattern = (std::filesystem::temp_directory_path() / "chaos_precond_XXXXXX").string();
        ASSERT_NE(::mkdtemp(pattern.data()), nullptr) << std::strerror(errno);
        dir_ = pattern;
        socket_path_ = (dir_ / "docker.sock").string();
    }

    void TearDown() override {
        closeSocket();
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }

    /** @brief Binds a Unix socket at socket_path_; it accepts connections only if @p listening. */
    void bindSocket(const bool listening) {
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        ASSERT_LT(socket_path_.size(), sizeof(address.sun_path));
        std::memcpy(address.sun_path, socket_path_.c_str(), socket_path_.size() + 1);

        fd_ = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        ASSERT_GE(fd_, 0) << std::strerror(errno);
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API requires it.
        ASSERT_EQ(::bind(fd_, reinterpret_cast<const sockaddr*>(&address), sizeof(address)), 0) << std::strerror(errno);
        if (listening) {
            ASSERT_EQ(::listen(fd_, 1), 0) << std::strerror(errno);
        }
    }

    void closeSocket() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    std::filesystem::path dir_;
    std::string socket_path_;
    int fd_ = -1;
};

}  // namespace

/** @test A listening socket counts as an available Docker daemon. */
TEST_F(TestPreconditionsUnitTest, DockerIsAvailableWhenSocketAcceptsConnections) {
    ASSERT_NO_FATAL_FAILURE(bindSocket(true));
    EXPECT_EQ(dockerUnavailableReason(socket_path_), std::nullopt);
}

/** @test A machine without Docker has no socket file; the reason names Docker and the path. */
TEST_F(TestPreconditionsUnitTest, DockerIsUnavailableWhenSocketIsMissing) {
    const auto reason = dockerUnavailableReason(socket_path_);
    ASSERT_TRUE(reason.has_value());
    EXPECT_NE(reason->find("Docker is not available"), std::string::npos) << *reason;
    EXPECT_NE(reason->find(socket_path_), std::string::npos) << *reason;
    EXPECT_NE(reason->find(std::generic_category().message(ENOENT)), std::string::npos) << *reason;
}

/** @test A socket file nobody listens on (daemon stopped) is not an available Docker. */
TEST_F(TestPreconditionsUnitTest, DockerIsUnavailableWhenNothingListensOnTheSocket) {
    ASSERT_NO_FATAL_FAILURE(bindSocket(false));
    const auto reason = dockerUnavailableReason(socket_path_);
    ASSERT_TRUE(reason.has_value());
    EXPECT_NE(reason->find(std::generic_category().message(ECONNREFUSED)), std::string::npos) << *reason;
}

/** @test A path that does not fit in sockaddr_un is reported, not truncated into another path. */
TEST_F(TestPreconditionsUnitTest, DockerIsUnavailableWhenSocketPathIsTooLong) {
    const std::string too_long = (dir_ / std::string(200, 'x')).string();
    const auto reason = dockerUnavailableReason(too_long);
    ASSERT_TRUE(reason.has_value());
    EXPECT_NE(reason->find("too long"), std::string::npos) << *reason;
}

/** @test The length limit is exact: a path that leaves no room for the terminating NUL is rejected. */
TEST(TestPreconditionsSocketPathUnitTest, SocketPathLimitLeavesRoomForTheTerminator) {
    const std::size_t capacity = sizeof(sockaddr_un{}.sun_path);

    const auto at_capacity = dockerUnavailableReason(std::string(capacity, 'x'));
    ASSERT_TRUE(at_capacity.has_value());
    EXPECT_NE(at_capacity->find("too long"), std::string::npos) << *at_capacity;

    const auto longest_valid = dockerUnavailableReason(std::string(capacity - 1, 'x'));
    ASSERT_TRUE(longest_valid.has_value());
    EXPECT_EQ(longest_valid->find("too long"), std::string::npos) << *longest_valid;
}

/** @test A path that is not a socket at all is not an available Docker either. */
TEST_F(TestPreconditionsUnitTest, DockerIsUnavailableWhenPathIsNotASocket) {
    const auto reason = dockerUnavailableReason(dir_.string());
    ASSERT_TRUE(reason.has_value());
    EXPECT_NE(reason->find("Docker is not available"), std::string::npos) << *reason;
}

/** @test Only effective uid 0 is root; the reason for anyone else names the uid. */
TEST(TestPreconditionsRootUnitTest, OnlyUidZeroIsRoot) {
    EXPECT_EQ(notRootReason(ROOT_UID), std::nullopt);

    const auto reason = notRootReason(REGULAR_UID);
    ASSERT_TRUE(reason.has_value());
    EXPECT_NE(reason->find("root is required"), std::string::npos) << *reason;
    EXPECT_NE(reason->find("1000"), std::string::npos) << *reason;
}

/** @test With Docker and root present nothing is missing, so the tests run. */
TEST_F(TestPreconditionsUnitTest, NothingIsMissingWithDockerAndRoot) {
    ASSERT_NO_FATAL_FAILURE(bindSocket(true));
    EXPECT_EQ(missingPreconditions(Requirement::DockerAndRoot, ROOT_UID, socket_path_), std::nullopt);
    EXPECT_EQ(missingPreconditions(Requirement::Docker, ROOT_UID, socket_path_), std::nullopt);
}

/** @test A Docker-only test does not ask for root. */
TEST_F(TestPreconditionsUnitTest, DockerOnlyRequirementIgnoresRoot) {
    ASSERT_NO_FATAL_FAILURE(bindSocket(true));
    EXPECT_EQ(missingPreconditions(Requirement::Docker, REGULAR_UID, socket_path_), std::nullopt);
}

/** @test Root is reported as missing when Docker is there but the user is not root. */
TEST_F(TestPreconditionsUnitTest, RootIsMissingForDockerAndRootRequirement) {
    ASSERT_NO_FATAL_FAILURE(bindSocket(true));
    const auto missing = missingPreconditions(Requirement::DockerAndRoot, REGULAR_UID, socket_path_);
    ASSERT_TRUE(missing.has_value());
    EXPECT_NE(missing->find("root is required"), std::string::npos) << *missing;
    EXPECT_EQ(missing->find("Docker"), std::string::npos) << *missing;
}

/** @test Docker is reported as missing even for root. */
TEST_F(TestPreconditionsUnitTest, DockerIsMissingEvenForRoot) {
    const auto missing = missingPreconditions(Requirement::DockerAndRoot, ROOT_UID, socket_path_);
    ASSERT_TRUE(missing.has_value());
    EXPECT_NE(missing->find("Docker is not available"), std::string::npos) << *missing;
    EXPECT_EQ(missing->find("root is required"), std::string::npos) << *missing;
}

/** @test Both missing preconditions are reported in one message. */
TEST_F(TestPreconditionsUnitTest, ReportsEveryMissingPrecondition) {
    const auto missing = missingPreconditions(Requirement::DockerAndRoot, REGULAR_UID, socket_path_);
    ASSERT_TRUE(missing.has_value());
    EXPECT_NE(missing->find("Docker is not available"), std::string::npos) << *missing;
    EXPECT_NE(missing->find("root is required"), std::string::npos) << *missing;
}

/** @test Strict mode is off unless the variable holds something other than "" or "0". */
TEST(TestPreconditionsStrictModeUnitTest, MandatoryOnlyWhenVariableIsSetToATruthyValue) {
    EXPECT_FALSE(preconditionsAreMandatory(nullptr));
    EXPECT_FALSE(preconditionsAreMandatory(""));
    EXPECT_FALSE(preconditionsAreMandatory("0"));
    EXPECT_TRUE(preconditionsAreMandatory("1"));
    EXPECT_TRUE(preconditionsAreMandatory("true"));
}

namespace {

/** @brief Sets or clears CHAOS_REQUIRE_PRIVILEGED_TESTS and puts the old value back on destruction. */
class ScopedStrictModeVariable {
public:
    explicit ScopedStrictModeVariable(const char* value) {
        if (const char* previous = std::getenv(REQUIRE_PRIVILEGED_TESTS_ENV)) {
            previous_ = previous;
        }
        if (value != nullptr) {
            ::setenv(REQUIRE_PRIVILEGED_TESTS_ENV, value, 1);
        } else {
            ::unsetenv(REQUIRE_PRIVILEGED_TESTS_ENV);
        }
    }
    ~ScopedStrictModeVariable() {
        if (previous_) {
            ::setenv(REQUIRE_PRIVILEGED_TESTS_ENV, previous_->c_str(), 1);
        } else {
            ::unsetenv(REQUIRE_PRIVILEGED_TESTS_ENV);
        }
    }
    ScopedStrictModeVariable(const ScopedStrictModeVariable&) = delete;
    ScopedStrictModeVariable& operator=(const ScopedStrictModeVariable&) = delete;

private:
    std::optional<std::string> previous_;
};

/** @brief Runs the skip-or-fail decision the way a fixture's SetUp() would, and counts fall-throughs. */
void requireEnvironment(const std::optional<std::string>& missing, const bool mandatory, int& reached_end) {
    CHAOS_REQUIRE_TEST_ENVIRONMENT_IMPL(missing, mandatory);
    ++reached_end;
}

}  // namespace

/** @test Strict mode really reads CHAOS_REQUIRE_PRIVILEGED_TESTS, the name the CI preset sets. */
TEST(TestPreconditionsStrictModeUnitTest, ReadsTheEnvironmentVariableByDefault) {
    EXPECT_STREQ(REQUIRE_PRIVILEGED_TESTS_ENV, "CHAOS_REQUIRE_PRIVILEGED_TESTS");
    {
        const ScopedStrictModeVariable variable("1");
        EXPECT_TRUE(preconditionsAreMandatory());
    }
    {
        const ScopedStrictModeVariable variable(nullptr);
        EXPECT_FALSE(preconditionsAreMandatory());
    }
}

/** @test With nothing missing the guard does nothing, so the test runs exactly as before. */
TEST(TestPreconditionsGuardUnitTest, RunsTheTestWhenNothingIsMissing) {
    int reached_end = 0;
    ::testing::TestPartResultArray results;
    {
        const ::testing::ScopedFakeTestPartResultReporter reporter(
            ::testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &results);
        requireEnvironment(std::nullopt, false, reached_end);
        requireEnvironment(std::nullopt, true, reached_end);
    }
    EXPECT_EQ(results.size(), 0);
    EXPECT_EQ(reached_end, 2);
}

/** @test A missing precondition is a skip carrying the reason, and leaves the calling function. */
TEST(TestPreconditionsGuardUnitTest, SkipsWithTheReasonWhenNotMandatory) {
    int reached_end = 0;
    ::testing::TestPartResultArray results;
    {
        // The fake reporter captures the skip, so this test itself is not reported as skipped.
        const ::testing::ScopedFakeTestPartResultReporter reporter(
            ::testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &results);
        requireEnvironment(std::string("no docker here"), false, reached_end);
    }
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results.GetTestPartResult(0).skipped());
    EXPECT_NE(std::string(results.GetTestPartResult(0).message()).find("no docker here"), std::string::npos);
    EXPECT_EQ(reached_end, 0);
}

/** @test In strict mode the same situation is a fatal failure and never a skip. */
TEST(TestPreconditionsGuardUnitTest, FailsWithTheReasonWhenMandatory) {
    int reached_end = 0;
    ::testing::TestPartResultArray results;
    {
        const ::testing::ScopedFakeTestPartResultReporter reporter(
            ::testing::ScopedFakeTestPartResultReporter::INTERCEPT_ONLY_CURRENT_THREAD, &results);
        requireEnvironment(std::string("no docker here"), true, reached_end);
    }
    ASSERT_EQ(results.size(), 1);
    EXPECT_TRUE(results.GetTestPartResult(0).fatally_failed());
    const std::string message = results.GetTestPartResult(0).message();
    EXPECT_NE(message.find("no docker here"), std::string::npos) << message;
    EXPECT_NE(message.find("CHAOS_REQUIRE_PRIVILEGED_TESTS"), std::string::npos) << message;
    EXPECT_EQ(reached_end, 0);
}
