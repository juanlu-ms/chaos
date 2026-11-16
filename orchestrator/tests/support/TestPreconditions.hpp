/**
 * @file TestPreconditions.hpp
 * @brief Run-time checks for what the smoke and e2e tests need from the machine (Docker, root).
 *
 * The checks run when a test starts, not when CMake configures: the machine that configures
 * is not always the one that runs the tests, and both root and the Docker daemon come and go.
 */

#pragma once

#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace chaos::orchestrator::tests::support {

/** @brief Socket the tests' container engine talks to (the default of createContainerEngine()). */
inline constexpr const char* DOCKER_SOCKET_PATH = "/var/run/docker.sock";

/** @brief Environment variable that turns a missing precondition into a failure instead of a skip. */
inline constexpr const char* REQUIRE_PRIVILEGED_TESTS_ENV = "CHAOS_REQUIRE_PRIVILEGED_TESTS";

/** @brief What a test needs from the machine it runs on. */
enum class Requirement {
    Docker,         ///< A Docker daemon this user can talk to.
    DockerAndRoot,  ///< Docker, plus root for nsenter, tc, iptables and raw sockets.
};

/**
 * @brief Checks that something accepts connections on the Docker socket.
 * @param socket_path Path to the Docker Engine Unix socket.
 * @return std::nullopt if a connection succeeds, otherwise why it does not.
 */
[[nodiscard]] inline std::optional<std::string> dockerUnavailableReason(
    const std::string& socket_path = DOCKER_SOCKET_PATH) {
    const auto unavailable = [&socket_path](const std::string_view why) {
        return "Docker is not available: cannot connect to " + socket_path + " (" + std::string(why) + ")";
    };

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    if (socket_path.size() >= sizeof(address.sun_path)) {
        return unavailable("path is too long for a Unix socket");
    }
    std::memcpy(address.sun_path, socket_path.c_str(), socket_path.size() + 1);

    // A real connect() instead of access(): it also catches a socket file left behind by a
    // daemon that is no longer running, and a user who is not allowed to use the socket.
    const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        return unavailable(std::generic_category().message(errno));
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): the sockets API requires it.
    const int result = ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    const int connect_errno = errno;
    ::close(fd);
    if (result != 0) {
        return unavailable(std::generic_category().message(connect_errno));
    }
    return std::nullopt;
}

/**
 * @brief Checks that the process runs as root.
 * @param euid Effective user id to check.
 * @return std::nullopt for root, otherwise the reason.
 */
[[nodiscard]] inline std::optional<std::string> notRootReason(const uid_t euid = ::geteuid()) {
    if (euid == 0) {
        return std::nullopt;
    }
    return "root is required (running with effective uid " + std::to_string(euid) + ")";
}

/**
 * @brief Lists every precondition of @p requirement that this machine does not meet.
 *
 * All of them are reported, not only the first, so one run tells the reader everything to fix.
 *
 * @param requirement What the test needs.
 * @param euid Effective user id to check.
 * @param socket_path Path to the Docker Engine Unix socket.
 * @return std::nullopt if the test can run, otherwise the missing preconditions.
 */
[[nodiscard]] inline std::optional<std::string> missingPreconditions(
    const Requirement requirement, const uid_t euid = ::geteuid(),
    const std::string& socket_path = DOCKER_SOCKET_PATH) {
    std::string missing;
    const auto add = [&missing](const std::optional<std::string>& reason) {
        if (!reason) {
            return;
        }
        if (!missing.empty()) {
            missing += "; ";
        }
        missing += *reason;
    };

    add(dockerUnavailableReason(socket_path));
    if (requirement == Requirement::DockerAndRoot) {
        add(notRootReason(euid));
    }

    if (missing.empty()) {
        return std::nullopt;
    }
    return missing;
}

/**
 * @brief Tells whether a missing precondition must fail the test instead of skipping it.
 *
 * CI sets the variable: there Docker and root are expected, so a broken runner must turn the
 * run red instead of skipping every privileged test and reporting success.
 *
 * @param value Value of CHAOS_REQUIRE_PRIVILEGED_TESTS, or nullptr if unset.
 * @return True unless the variable is unset, empty or "0".
 */
[[nodiscard]] inline bool preconditionsAreMandatory(const char* value = std::getenv(REQUIRE_PRIVILEGED_TESTS_ENV)) {
    return value != nullptr && *value != '\0' && std::string_view(value) != "0";
}

}  // namespace chaos::orchestrator::tests::support

/**
 * @brief Skip-or-fail decision of CHAOS_REQUIRE_TEST_ENVIRONMENT, with both inputs passed in.
 *
 * Split out so the unit tests can drive it without depending on the real Docker socket, uid or
 * environment. Tests use CHAOS_REQUIRE_TEST_ENVIRONMENT instead.
 */
#define CHAOS_REQUIRE_TEST_ENVIRONMENT_IMPL(missing_expr, mandatory_expr)                               \
    if (const ::std::optional<::std::string> chaos_missing_preconditions = (missing_expr)) {            \
        if (mandatory_expr) {                                                                           \
            FAIL() << ::chaos::orchestrator::tests::support::REQUIRE_PRIVILEGED_TESTS_ENV               \
                   << " is set, so this is a failure and not a skip: " << *chaos_missing_preconditions; \
        }                                                                                               \
        GTEST_SKIP() << *chaos_missing_preconditions;                                                   \
    }                                                                                                   \
    static_assert(true, "require a trailing semicolon")

/**
 * @brief Skips the current test (or fails it, in strict mode) unless the machine meets @p requirement.
 *
 * A macro because GTEST_SKIP() and FAIL() return from the function they are written in. Use it
 * in a function returning void: a test body or a fixture's SetUp(). A fixture that overrides
 * SetUp() must return early when IsSkipped() or HasFatalFailure() is true after calling its base.
 */
#define CHAOS_REQUIRE_TEST_ENVIRONMENT(requirement)                                                               \
    CHAOS_REQUIRE_TEST_ENVIRONMENT_IMPL(::chaos::orchestrator::tests::support::missingPreconditions(requirement), \
                                        ::chaos::orchestrator::tests::support::preconditionsAreMandatory())
