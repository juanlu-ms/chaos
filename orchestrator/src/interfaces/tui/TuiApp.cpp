/**
 * @file TuiApp.cpp
 * @brief Terminal User Interface implementation for the chaos orchestrator.
 */

#include "interfaces/tui/TuiApp.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "containers/Container.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::interfaces::tui {

TuiApp::TuiApp(std::shared_ptr<containers::IContainerEngine> engine)
    : m_engine(std::move(engine)) {}

int TuiApp::run() {
    using namespace ftxui;

    // ── State ────────────────────────────────────────────────────────────────
    std::vector<containers::Container> containers;
    std::vector<std::string> container_labels;
    int selected = 0;
    std::string manifest_path;
    std::vector<std::string> output_lines;

    auto refresh_containers = [&] {
        try {
            containers = m_engine->listContainers();
            container_labels.clear();
            for (const auto& c : containers) {
                const std::string display_id =
                    c.id.size() > 12 ? c.id.substr(0, 12) : c.id;
                container_labels.push_back(
                    fmt::format("{:<14} {:<28} {}", display_id, c.name, c.state));
            }
        } catch (const std::exception& ex) {
            output_lines.push_back(
                fmt::format("ERROR listing containers: {}", ex.what()));
        }
        // Keep selection in range
        if (!container_labels.empty() &&
            selected >= static_cast<int>(container_labels.size())) {
            selected = static_cast<int>(container_labels.size()) - 1;
        }
    };

    refresh_containers();

    // ── Components ───────────────────────────────────────────────────────────
    auto container_menu = Menu(&container_labels, &selected);

    auto btn_refresh = Button("  Refresh  ", [&] {
        output_lines.clear();
        refresh_containers();
    });

    auto btn_stop = Button("  Stop     ", [&] {
        if (containers.empty() ||
            selected < 0 ||
            selected >= static_cast<int>(containers.size())) {
            output_lines.push_back("No container selected.");
            return;
        }
        try {
            const auto& c = containers[static_cast<std::size_t>(selected)];
            m_engine->stopContainer(c.id);
            output_lines.push_back(fmt::format("✅ Stopped: {}", c.name));
            refresh_containers();
        } catch (const std::exception& ex) {
            output_lines.push_back(fmt::format("❌ Stop failed: {}", ex.what()));
        }
    });

    auto btn_kill = Button("  Kill     ", [&] {
        if (containers.empty() ||
            selected < 0 ||
            selected >= static_cast<int>(containers.size())) {
            output_lines.push_back("No container selected.");
            return;
        }
        try {
            const auto& c = containers[static_cast<std::size_t>(selected)];
            m_engine->killContainer(c.id);
            output_lines.push_back(fmt::format("✅ Killed: {}", c.name));
            refresh_containers();
        } catch (const std::exception& ex) {
            output_lines.push_back(fmt::format("❌ Kill failed: {}", ex.what()));
        }
    });

    auto manifest_input = Input(&manifest_path, "path/to/manifest.json");

    auto btn_run = Button("  Run  ", [&] {
        output_lines.clear();
        if (manifest_path.empty()) {
            output_lines.push_back("Please enter a manifest path.");
            return;
        }
        try {
            auto manifest = manifests::ManifestParser::parseFromFile(manifest_path);
            output_lines.push_back(
                fmt::format("▶ Running manifest: {}", manifest.test_name));

            perturbations::PerturbationEngine pert(m_engine);
            pert.applyAll(manifest);

            if (manifest.duration_s.has_value() && manifest.duration_s.value() > 0) {
                output_lines.push_back(
                    fmt::format("  ⏳ Waiting {}s...", manifest.duration_s.value()));
                std::this_thread::sleep_for(
                    std::chrono::seconds(manifest.duration_s.value()));
            }

            observability::ObservabilityEngine obs(m_engine);
            validation::ValidationEngine validator(std::move(obs));
            const auto results =
                validator.validate(manifest.target.id, manifest.expectations);

            for (const auto& r : results) {
                output_lines.push_back(fmt::format("  {} {}: {}",
                    r.passed ? "✅" : "❌", r.expectationType, r.message));
            }

            const bool passed = std::all_of(
                results.begin(), results.end(),
                [](const auto& r) { return r.passed; });
            output_lines.push_back(passed ? ">>> PASSED ✅" : ">>> FAILED ❌");
        } catch (const std::exception& ex) {
            output_lines.push_back(fmt::format("❌ Error: {}", ex.what()));
        }
    });

    // ── Layout ───────────────────────────────────────────────────────────────
    auto actions_col = Container::Vertical({btn_refresh, btn_stop, btn_kill});
    auto manifest_row = Container::Horizontal({manifest_input, btn_run});
    auto layout = Container::Vertical({container_menu, actions_col, manifest_row});

    // ── Renderer ─────────────────────────────────────────────────────────────
    auto screen = ScreenInteractive::Fullscreen();

    auto renderer = Renderer(layout, [&] {
        // Build output pane
        Elements out_elems;
        for (const auto& line : output_lines) {
            out_elems.push_back(text(line));
        }
        if (out_elems.empty()) {
            out_elems.push_back(text("(no output yet)") | dim);
        }

        return vbox({
                   // ── Title bar ──────────────────────────────────────────
                   hbox({
                       text(" 🔥 Chaos Orchestrator TUI") | bold | flex,
                       text(" [q/ESC] Quit ") | dim,
                   }) | color(Color::White) | bgcolor(Color::Blue),

                   separator(),

                   // ── Main body: container list | actions ────────────────
                   hbox({
                       vbox({
                           text(" CONTAINERS") | bold,
                           separator(),
                           container_labels.empty()
                               ? text("  (no containers found)") | dim
                               : container_menu->Render() | frame | flex,
                       }) | flex,

                       separator(),

                       vbox({
                           text(" ACTIONS") | bold,
                           separator(),
                           btn_refresh->Render(),
                           btn_stop->Render(),
                           btn_kill->Render(),
                       }) | size(WIDTH, EQUAL, 18),
                   }) | flex,

                   separator(),

                   // ── Manifest runner ────────────────────────────────────
                   hbox({
                       text(" Manifest: ") | bold,
                       manifest_input->Render() | flex,
                       btn_run->Render(),
                   }),

                   separator(),

                   // ── Output pane ────────────────────────────────────────
                   vbox({
                       text(" Output:") | bold,
                       separator(),
                       vbox(std::move(out_elems)) | frame | flex,
                   }) | size(HEIGHT, LESS_THAN, 12),
               }) |
               border;
    });

    // Catch quit keys
    auto final_renderer = CatchEvent(renderer, [&](Event event) -> bool {
        if (event == Event::Character('q') || event == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(final_renderer);
    return 0;
}

}  // namespace chaos::orchestrator::interfaces::tui
