/**
 * @file TuiApp.cpp
 * @brief Terminal User Interface implementation for the chaos orchestrator.
 */

#include "interfaces/tui/TuiApp.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
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

    // ── CWD hint shown next to the manifest input ─────────────────────────────
    const std::string cwd = std::filesystem::current_path().string();

    // ── Shared State (guarded for background thread) ──────────────────────────
    std::mutex state_mutex;
    std::vector<containers::Container> containers;
    std::vector<std::string> container_labels;
    int selected = 0;
    std::string manifest_path;
    std::vector<std::string> output_lines;
    std::atomic<bool> run_in_progress{false};
    std::string status_msg;  // transient status bar message (e.g. "Exported to...")

    auto screen = ScreenInteractive::Fullscreen();

    // Helper: push a line to output and trigger a redraw from any thread
    auto push_output = [&](std::string line) {
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            output_lines.push_back(std::move(line));
        }
        screen.PostEvent(Event::Custom);
    };

    // Export output lines to a timestamped file in CWD
    auto export_output = [&] {
        std::lock_guard<std::mutex> lock(state_mutex);
        if (output_lines.empty()) {
            status_msg = "Nothing to export.";
            return;
        }
        // Build a filename like chaos-output-20260413-212345.txt
        const auto now = std::chrono::system_clock::now();
        const auto t = std::chrono::system_clock::to_time_t(now);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", std::localtime(&t));
        const std::string filename =
            fmt::format("{}/chaos-output-{}.txt", cwd, ts);

        std::ofstream out(filename);
        if (!out) {
            status_msg = fmt::format("Export failed: could not write to {}", filename);
            return;
        }
        for (const auto& line : output_lines) {
            out << line << "\n";
        }
        status_msg = fmt::format("Exported → {}", filename);
    };

    auto refresh_containers = [&] {
        try {
            auto fresh = m_engine->listContainers();
            std::vector<std::string> labels;
            labels.reserve(fresh.size());
            for (const auto& c : fresh) {
                const std::string display_id =
                    c.id.size() > 12 ? c.id.substr(0, 12) : c.id;
                labels.push_back(
                    fmt::format("{:<13} {:<22} {}", display_id, c.name, c.state));
            }
            std::lock_guard<std::mutex> lock(state_mutex);
            containers = std::move(fresh);
            container_labels = std::move(labels);
            if (!container_labels.empty() &&
                selected >= static_cast<int>(container_labels.size())) {
                selected = static_cast<int>(container_labels.size()) - 1;
            }
        } catch (const std::exception& ex) {
            push_output(fmt::format("ERROR listing containers: {}", ex.what()));
        }
        screen.PostEvent(Event::Custom);
    };

    refresh_containers();

    // ── Components ───────────────────────────────────────────────────────────
    auto container_menu = Menu(&container_labels, &selected);

    auto btn_refresh = Button("Refresh", [&] {
        refresh_containers();
    });

    auto btn_stop = Button("Stop", [&] {
        containers::Container c;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (containers.empty() || selected < 0 ||
                selected >= static_cast<int>(containers.size())) {
                output_lines.push_back("No container selected.");
                return;
            }
            c = containers[static_cast<std::size_t>(selected)];
        }
        try {
            m_engine->stopContainer(c.id);
            push_output(fmt::format("✅ Stopped: {}", c.name));
            refresh_containers();
        } catch (const std::exception& ex) {
            push_output(fmt::format("❌ Stop failed: {}", ex.what()));
        }
    });

    auto btn_kill = Button("Kill", [&] {
        containers::Container c;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (containers.empty() || selected < 0 ||
                selected >= static_cast<int>(containers.size())) {
                output_lines.push_back("No container selected.");
                return;
            }
            c = containers[static_cast<std::size_t>(selected)];
        }
        try {
            m_engine->killContainer(c.id);
            push_output(fmt::format("✅ Killed: {}", c.name));
            refresh_containers();
        } catch (const std::exception& ex) {
            push_output(fmt::format("❌ Kill failed: {}", ex.what()));
        }
    });

    // Manifest input: placeholder shows CWD so user knows what base path to use
    auto manifest_input = Input(&manifest_path, cwd + "/examples/...");

    auto btn_run = Button("Run", [&] {
        if (run_in_progress.load()) {
            push_output("⚠ A run is already in progress.");
            return;
        }
        if (manifest_path.empty()) {
            push_output("Please enter a manifest path.");
            return;
        }

        const std::string path_snapshot = manifest_path;

        std::thread([&, path_snapshot] {
            run_in_progress.store(true);
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                output_lines.clear();
                status_msg.clear();
            }
            screen.PostEvent(Event::Custom);

            try {
                auto manifest = manifests::ManifestParser::parseFromFile(path_snapshot);
                push_output(fmt::format("▶ Running: {}", manifest.test_name));

                perturbations::PerturbationEngine pert(m_engine);
                pert.applyAll(manifest);
                push_output("  Perturbations applied.");

                if (manifest.duration_s.has_value() && manifest.duration_s.value() > 0) {
                    const int secs = manifest.duration_s.value();
                    push_output(fmt::format("  ⏳ Waiting {}s for faults to propagate...", secs));
                    for (int i = 0; i < secs; ++i) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        push_output(fmt::format("  ⏳ {}s / {}s", i + 1, secs));
                    }
                }

                observability::ObservabilityEngine obs(m_engine);
                validation::ValidationEngine validator(std::move(obs));
                const auto results =
                    validator.validate(manifest.target.id, manifest.expectations);

                for (const auto& r : results) {
                    push_output(fmt::format("  {} {}: {}",
                        r.passed ? "✅" : "❌", r.expectationType, r.message));
                }

                const bool passed = std::all_of(
                    results.begin(), results.end(),
                    [](const auto& r) { return r.passed; });
                push_output(passed ? ">>> PASSED ✅" : ">>> FAILED ❌");
            } catch (const std::exception& ex) {
                push_output(fmt::format("❌ Error: {}", ex.what()));
                push_output(fmt::format("  (Working directory: {})", cwd));
            }

            run_in_progress.store(false);
            screen.PostEvent(Event::Custom);
        }).detach();
    });

    // ── Layout ───────────────────────────────────────────────────────────────
    auto actions_col = Container::Vertical({btn_refresh, btn_stop, btn_kill});
    auto manifest_row = Container::Horizontal({manifest_input, btn_run});
    auto layout = Container::Vertical({container_menu, actions_col, manifest_row});

    // ── Renderer ─────────────────────────────────────────────────────────────
    auto renderer = Renderer(layout, [&] {
        std::vector<std::string> snap_output;
        bool in_progress = false;
        std::string snap_status;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            snap_output = output_lines;
            in_progress = run_in_progress.load();
            snap_status = status_msg;
        }

        // Build output elements
        Elements out_elems;
        out_elems.reserve(snap_output.size());
        for (const auto& line : snap_output) {
            out_elems.push_back(text(line) | flex_shrink);
        }
        if (out_elems.empty()) {
            out_elems.push_back(text("(no output yet)") | dim);
        }

        // Container list panel
        Element container_panel;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            container_panel = container_labels.empty()
                ? text("  (no containers found)") | dim
                : container_menu->Render() | vscroll_indicator | frame | flex;
        }

        // Status bar content
        const std::string status_text = snap_status.empty()
            ? fmt::format(" CWD: {}  |  [e] Export output  |  [q/ESC] Quit", cwd)
            : fmt::format(" {}", snap_status);

        return vbox({
                   // ── Title bar ──────────────────────────────────────────────
                   hbox({
                       text(" 🔥 Chaos Orchestrator TUI") | bold | flex,
                       text(in_progress ? " ⏳ Running... " : " ") | dim,
                   }) | color(Color::White) | bgcolor(Color::Blue),

                   // ── Main split: containers | actions ──────────────────────
                   hbox({
                       vbox({
                           text(" CONTAINERS") | bold,
                           separator(),
                           container_panel,
                       }) | border | flex,

                       vbox({
                           text(" ACTIONS") | bold,
                           separator(),
                           btn_refresh->Render() | hcenter,
                           separator(),
                           btn_stop->Render() | hcenter,
                           btn_kill->Render() | hcenter,
                       }) | border | size(WIDTH, EQUAL, 18),
                   }) | flex,

                   // ── Manifest runner row ────────────────────────────────────
                   hbox({
                       text(" Manifest: ") | bold,
                       manifest_input->Render() | flex,
                       btn_run->Render(),
                   }) | border,

                   // ── Output pane ────────────────────────────────────────────
                   vbox({
                       hbox({
                           text(" Output:") | bold | flex,
                           text(" [e] export ") | dim,
                       }),
                       separator(),
                       vbox(std::move(out_elems)) | vscroll_indicator | frame | flex,
                   }) | border | size(HEIGHT, LESS_THAN, 14),

                   // ── Status bar ─────────────────────────────────────────────
                   text(status_text) | dim,
               });
    });

    // Global key handler
    auto final_renderer = CatchEvent(renderer, [&](Event event) -> bool {
        if (event == Event::Character('q') || event == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == Event::Character('e')) {
            export_output();
            screen.PostEvent(Event::Custom);
            return true;
        }
        return false;
    });

    screen.Loop(final_renderer);
    return 0;
}

}  // namespace chaos::orchestrator::interfaces::tui
