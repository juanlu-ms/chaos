#include "interfaces/cli/UsageRenderer.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "interfaces/cli/CliSpec.hpp"

namespace chaos::orchestrator::interfaces::cli {

namespace {

struct Row {
    std::string label;
    std::string description;
};

struct Section {
    std::string title;
    std::vector<Row> rows;
};

[[nodiscard]] Row flagRow(const FlagSpec& flag) {
    std::string label = flag.shortName.empty() ? fmt::format("    {}", flag.longName)
                                               : fmt::format("{}, {}", flag.shortName, flag.longName);
    if (!flag.metavar.empty()) {
        fmt::format_to(std::back_inserter(label), " {}", flag.metavar);
    }
    std::string description(flag.description);
    if (!flag.values.empty()) {
        fmt::format_to(std::back_inserter(description), " ({})", fmt::join(flag.values, "|"));
    }
    return Row{.label = std::move(label), .description = std::move(description)};
}

[[nodiscard]] std::string titleCase(std::string_view word) {
    std::string out(word);
    if (!out.empty()) {
        out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    }
    return out;
}

}  // namespace

std::string renderUsage() {
    std::vector<Section> sections;

    Section globals{.title = "Global Options:", .rows = {}};
    for (const auto& flag : kGlobalFlags) {
        globals.rows.push_back(flagRow(flag));
    }
    sections.push_back(std::move(globals));

    Section commands{.title = "Commands:", .rows = {}};
    for (const auto& command : kCommands) {
        if (command.hidden) {
            continue;
        }
        std::string label =
            command.usage.empty() ? std::string(command.name) : fmt::format("{} {}", command.name, command.usage);
        commands.rows.push_back(Row{.label = std::move(label), .description = std::string(command.description)});
    }
    sections.push_back(std::move(commands));

    for (const auto& command : kCommands) {
        if (command.hidden || command.flags.empty()) {
            continue;
        }
        Section section{.title = fmt::format("{} Options:", titleCase(command.name)), .rows = {}};
        for (const auto& flag : command.flags) {
            section.rows.push_back(flagRow(flag));
        }
        sections.push_back(std::move(section));
    }

    std::size_t width = 0;
    for (const auto& section : sections) {
        for (const auto& row : section.rows) {
            width = std::max(width, row.label.size());
        }
    }

    std::string out;
    fmt::format_to(std::back_inserter(out), "Usage: {} [global-options] <command> [command-options]\n\n{}\n",
                   kProgramName, kTagline);
    for (const auto& section : sections) {
        fmt::format_to(std::back_inserter(out), "\n{}\n", section.title);
        for (const auto& row : section.rows) {
            fmt::format_to(std::back_inserter(out), "  {:<{}}  {}\n", row.label, width, row.description);
        }
    }
    return out;
}

}  // namespace chaos::orchestrator::interfaces::cli
