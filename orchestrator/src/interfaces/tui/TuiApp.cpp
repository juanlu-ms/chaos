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

    // ── Shared State (guarded for background thread) ──────────────────────────
    std::mutex state_mutex;
    std::vector<containers::Container> containers;
    std::vector<std::string> container_labels;
    int selected = 0;
    std::string manifest_path;
    std::vector<std::string> output_lines;
    std::atomic<bool> run_in_progress{false};

    // ── Output scroll state ───────────────────────────────────────────────────
    // scroll_offset = index of the first visible line.
    // follow_tail = true  → always jump to the last line on new output (log mode).
    // follow_tail = false → user has scrolled up manually; stop auto-following.
    int scroll_offset = 0;
    bool follow_tail = true;

    auto screen = ScreenInteractive::Fullscreen();

    auto push_output = [&](std::string line) {
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            output_lines.push_back(std::move(line));
        }
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

    // ── Components ───────────────────────────────────────────────────────────
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
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                output_lines.clear();
            }
            // Reset scroll to top / tail-follow for new run
            scroll_offset = 0;
            follow_tail = true;
            screen.PostEvent(Event::Custom);

            try {
                auto manifest = manifests::ManifestParser::parseFromFile(path_snapshot);
                push_output(fmt::format("▶ Running: {}", manifest.test_name));

                perturbations::PerturbationEngine pert(m_engine);
                pert.applyAll(manifest);
                push_output("  Perturbations applied.");

                if (manifest.duration_s.has_value() && manifest.duration_s.value() > 0) {
                    const unsigned int secs = manifest.duration_s.value();
                    push_output(fmt::format("  ⏳ Waiting {}s for faults to propagate...", secs));
                    for (unsigned int i = 0; i < secs; ++i) {
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        push_output(fmt::format("  ⏳ {}s / {}s", i + 1, secs));
                    }
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

            run_in_progress.store(false);
            screen.PostEvent(Event::Custom);
        }).detach();
    });

    // ── Output scroller component ─────────────────────────────────────────────
    // A proper CatchEvent-backed component so arrow/page keys actually work.
    auto output_scroller = CatchEvent(
        Renderer([&] {
            std::vector<std::string> snap;
            std::size_t total = 0;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                snap = output_lines;
                total = snap.size();
            }

            // Tail-follow: pin to the last line when new output arrives
            const int max_scroll = std::max(0, static_cast<int>(total) - 1);
            if (follow_tail) {
                scroll_offset = max_scroll;
            } else {
                scroll_offset = std::clamp(scroll_offset, 0, max_scroll);
            }

            // Render lines from scroll_offset onward
            Elements elems;
            for (std::size_t i = static_cast<std::size_t>(scroll_offset); i < snap.size(); ++i) {
                elems.push_back(text(snap[i]) | flex_shrink);
            }
            if (snap.empty()) {
                elems.push_back(text("(no output yet)") | dim);
            }

            // Show "lines above" hint when scrolled up
            Element content = vbox(std::move(elems));
            if (scroll_offset > 0) {
                return vbox({
                    text(fmt::format(" ↑ {} line(s) above — PgUp/↑ to scroll, End to follow", scroll_offset)) | dim,
                    separator(),
                    content | flex,
                });
            }
            return content | flex;
        }),
        [&](Event e) -> bool {
            std::size_t total = 0;
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                total = output_lines.size();
            }
            const int max_scroll = std::max(0, static_cast<int>(total) - 1);

            if (e == Event::ArrowDown) {
                scroll_offset = std::min(scroll_offset + 1, max_scroll);
            } else if (e == Event::ArrowUp) {
                scroll_offset = std::max(0, scroll_offset - 1);
            } else if (e == Event::PageDown) {
                scroll_offset = std::min(scroll_offset + 10, max_scroll);
            } else if (e == Event::PageUp) {
                scroll_offset = std::max(0, scroll_offset - 10);
            } else if (e == Event::Home) {
                scroll_offset = 0;
            } else if (e == Event::End) {
                scroll_offset = max_scroll;
            } else {
                return false;
            }

            // Stop auto-following when user scrolls up; resume at the end
            follow_tail = (scroll_offset >= max_scroll);
            screen.PostEvent(Event::Custom);
            return true;
        }
    );

    // ── Layout ───────────────────────────────────────────────────────────────
    auto actions_col = Container::Vertical({btn_refresh, btn_stop, btn_kill});
    auto manifest_row = Container::Horizontal({manifest_input, btn_run});
    auto layout = Container::Vertical({container_menu, actions_col, manifest_row, output_scroller});

    // ── Renderer ─────────────────────────────────────────────────────────────
    auto renderer = Renderer(layout, [&] {
        bool in_progress = run_in_progress.load();

        Element container_panel;
        {
            std::lock_guard lock(state_mutex);
            container_panel = container_labels.empty() ? text("  (no containers found)") | dim
                                                       : container_menu->Render() | vscroll_indicator | frame | flex;
        }

        return vbox({
            hbox({
                text(" 🔥 Chaos Orchestrator TUI") | bold | flex,
                text(in_progress ? " ⏳ Running... " : " [q/ESC] Quit ") | dim,
            }) | color(Color::White) |
                bgcolor(Color::Blue),

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

            hbox({
                text(" Manifest: ") | bold,
                manifest_input->Render() | flex,
                btn_run->Render(),
            }) | border,

            vbox({
                text(" Output:") | bold,
                separator(),
                output_scroller->Render() | flex,
            }) | border | size(HEIGHT, LESS_THAN, 14),

            text(fmt::format(" CWD: {}", cwd)) | dim,
        });
    });

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
