/**
 * @file TuiApp.cpp
 * @brief Terminal User Interface implementation for the chaos orchestrator.
 */

#include "interfaces/tui/TuiApp.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
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

TuiApp::TuiApp(std::shared_ptr<containers::IContainerEngine> engine) : m_engine(std::move(engine)) {}

int TuiApp::run() {
    using namespace ftxui;

    const std::string cwd = std::filesystem::current_path().string();

    // ── Shared State ──────────────────────────────────────────────────────────
    std::mutex state_mutex;
    std::vector<containers::Container> containers;
    std::vector<std::string> container_labels;
    int selected = 0;
    std::string manifest_path;
    std::vector<std::string> output_lines;
    std::atomic<bool> run_in_progress{false};

    // Wait progress: total_seconds == 0 means no active wait bar
    std::atomic<int> wait_elapsed{0};
    std::atomic<int> wait_total{0};

    // Log scroll position: 0.0 = top, 1.0 = bottom
    float log_scroll = 1.0F;

    auto screen = ScreenInteractive::Fullscreen();

    auto push_output = [&](std::string line) {
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            output_lines.push_back(std::move(line));
        }
        // Auto-follow bottom when new output arrives
        log_scroll = 1.0F;
        screen.PostEvent(Event::Custom);
    };

    auto refresh_containers = [&] {
        try {
            auto fresh = m_engine->listContainers();
            std::vector<std::string> labels;
            labels.reserve(fresh.size());
            for (const auto& c : fresh) {
                const std::string display_id = c.id.size() > 12 ? c.id.substr(0, 12) : c.id;
                labels.push_back(fmt::format("{:<13} {:<22} {}", display_id, c.name, c.state));
            }
            std::lock_guard<std::mutex> lock(state_mutex);
            containers = std::move(fresh);
            container_labels = std::move(labels);
            if (!container_labels.empty() && selected >= static_cast<int>(container_labels.size())) {
                selected = static_cast<int>(container_labels.size()) - 1;
            }
        } catch (const std::exception& ex) {
            push_output(fmt::format("ERROR listing containers: {}", ex.what()));
        }
        screen.PostEvent(Event::Custom);
    };

    refresh_containers();

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto container_menu = Menu(&container_labels, &selected);

    auto btn_refresh = Button("Refresh", [&] { refresh_containers(); });

    auto btn_stop = Button("Stop", [&] {
        containers::Container c;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            if (containers.empty() || selected < 0 || selected >= static_cast<int>(containers.size())) {
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
            if (containers.empty() || selected < 0 || selected >= static_cast<int>(containers.size())) {
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
            wait_elapsed.store(0);
            wait_total.store(0);
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                output_lines.clear();
            }
            log_scroll = 1.0F;
            screen.PostEvent(Event::Custom);

            try {
                auto manifest = manifests::ManifestParser::parseFromFile(path_snapshot);
                push_output(fmt::format("▶ Running: {}", manifest.test_name));

                perturbations::PerturbationEngine pert(m_engine);
                pert.applyAll(manifest);
                push_output("  ✓ Perturbations applied.");

                if (manifest.duration_s.has_value() && manifest.duration_s.value() > 0) {
                    const int secs = static_cast<int>(manifest.duration_s.value());
                    wait_total.store(secs);
                    wait_elapsed.store(0);
                    screen.PostEvent(Event::Custom);
                    for (int i = 0; i < secs; ++i) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        wait_elapsed.store(i + 1);
                        screen.PostEvent(Event::Custom);
                    }
                    wait_total.store(0);  // hide bar when done
                    push_output(fmt::format("  ✓ Waited {}s.", secs));
                }

                observability::ObservabilityEngine obs(m_engine);
                validation::ValidationEngine validator(std::move(obs));
                const auto results = validator.validate(manifest.target.id, manifest.expectations);

                for (const auto& r : results) {
                    push_output(fmt::format("  {} {}: {}", r.passed ? "✅" : "❌", r.expectationType, r.message));
                }
                const bool passed = std::all_of(results.begin(), results.end(), [](const auto& r) { return r.passed; });
                push_output(passed ? ">>> PASSED ✅" : ">>> FAILED ❌");
            } catch (const std::exception& ex) {
                push_output(fmt::format("❌ Error: {}", ex.what()));
                push_output(fmt::format("  (Working directory: {})", cwd));
            }

            wait_total.store(0);
            run_in_progress.store(false);
            screen.PostEvent(Event::Custom);
        }).detach();
    });

    // ── Log content renderer ──────────────────────────────────────────────────
    // Renders ALL lines plus an optional live progress bar.
    // Wrapped in a scrollable shell below.
    auto log_content = Renderer([&] {
        std::vector<std::string> snap;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            snap = output_lines;
        }

        Elements elems;
        elems.reserve(snap.size() + 2);
        for (const auto& line : snap) {
            elems.push_back(text(line));
        }

        // Progress bar row — only visible during a timed wait
        const int total = wait_total.load();
        if (total > 0) {
            const int elapsed = wait_elapsed.load();
            const float ratio = static_cast<float>(elapsed) / static_cast<float>(total);
            elems.push_back(hbox({
                                text(fmt::format("  ⏳ Waiting  {}/{}s  ", elapsed, total)),
                                gauge(ratio) | flex,
                            }) |
                            color(Color::Yellow));
        }

        if (elems.empty()) {
            elems.push_back(text("(no output yet)") | dim);
        }

        return vbox(std::move(elems));
    });

    // ── Scrollable shell ──────────────────────────────────────────────────────
    // Uses focusPositionRelative(0, log_scroll) | frame — the canonical FTXUI
    // pattern for a scrollable viewport controlled by a float position.
    // CatchEvent intercepts ↑/↓/PgUp/PgDn/Home/End to move log_scroll.
    auto log_scroller = CatchEvent(
        Renderer(log_content,
                 [&] { return log_content->Render() | focusPositionRelative(0, log_scroll) | frame | flex; }),
        [&](Event e) -> bool {
            const float step = 0.05F;  // ~5% per arrow key
            const float page = 0.25F;  // ~25% per page
            if (e == Event::ArrowDown) {
                log_scroll = std::min(1.0F, log_scroll + step);
            } else if (e == Event::ArrowUp) {
                log_scroll = std::max(0.0F, log_scroll - step);
            } else if (e == Event::PageDown) {
                log_scroll = std::min(1.0F, log_scroll + page);
            } else if (e == Event::PageUp) {
                log_scroll = std::max(0.0F, log_scroll - page);
            } else if (e == Event::Home) {
                log_scroll = 0.0F;
            } else if (e == Event::End) {
                log_scroll = 1.0F;
            } else {
                return false;
            }
            screen.PostEvent(Event::Custom);
            return true;
        });

    // ── Layout & top-level renderer ───────────────────────────────────────────
    auto actions_col = Container::Vertical({btn_refresh, btn_stop, btn_kill});
    auto manifest_row = Container::Horizontal({manifest_input, btn_run});
    auto layout = Container::Vertical({container_menu, actions_col, manifest_row, log_scroller});

    auto renderer = Renderer(layout, [&] {
        bool in_progress = run_in_progress.load();

        Element container_panel;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            container_panel = container_labels.empty() ? text("  (no containers found)") | dim
                                                       : container_menu->Render() | vscroll_indicator | frame | flex;
        }

        return vbox({
            // Title bar
            hbox({
                text("Chaos Orchestrator TUI") | bold | flex,
                text(in_progress ? " ⏳ Running... " : " [q/ESC] Quit ") | dim,
            }) | color(Color::White) |
                bgcolor(Color::Blue),

            // Containers | Actions
            hbox({
                vbox({
                    text(" CONTAINERS") | bold,
                    separator(),
                    container_panel,
                }) | border |
                    flex,

                vbox({
                    text(" ACTIONS") | bold,
                    separator(),
                    btn_refresh->Render() | hcenter,
                    separator(),
                    btn_stop->Render() | hcenter,
                    btn_kill->Render() | hcenter,
                }) | border |
                    size(WIDTH, EQUAL, 18),
            }) | flex,

            // Manifest runner
            hbox({
                text(" Manifest: ") | bold,
                manifest_input->Render() | flex,
                btn_run->Render(),
            }) | border,

            // Output log (scrollable)
            vbox({
                hbox({
                    text(" Output:") | bold | flex,
                    text(" Tab to focus · ↑↓ PgUp/PgDn Home/End to scroll ") | dim,
                }),
                separator(),
                log_scroller->Render() | flex,
            }) | border |
                size(HEIGHT, LESS_THAN, 16),

            text(fmt::format(" CWD: {}", cwd)) | dim,
        });
    });

    auto final_renderer = CatchEvent(renderer, [&](Event e) -> bool {
        if (e == Event::Character('q') || e == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(final_renderer);
    return 0;
}

}  // namespace chaos::orchestrator::interfaces::tui
