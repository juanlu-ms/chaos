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
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "containers/Container.hpp"
#include "manifests/ManifestParser.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "shared/StateBroadcaster.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::interfaces::tui {

namespace {

struct SharedState {
    std::mutex state_mutex;
    std::vector<containers::Container> containers;
    std::vector<std::string> container_labels;
    int selected = 0;
    std::string manifest_path;
    std::vector<std::string> output_lines;
    std::atomic<bool> run_in_progress{false};
    std::atomic<int> wait_elapsed{0};
    std::atomic<int> wait_total{0};
    std::atomic<float> log_scroll{1.0F};
};

void post_refresh(ftxui::ScreenInteractive& screen) { screen.PostEvent(ftxui::Event::Custom); }

void push_output_line(SharedState& state, ftxui::ScreenInteractive& screen, std::string line) {
    {
        std::lock_guard lock(state.state_mutex);
        state.output_lines.push_back(std::move(line));
    }
    state.log_scroll = 1.0F;
    post_refresh(screen);
}

std::vector<std::string> build_labels(const std::vector<containers::Container>& fresh) {
    std::vector<std::string> labels;
    labels.reserve(fresh.size());
    for (const auto& c : fresh) {
        const auto display_id = c.id.size() > 12 ? c.id.substr(0, 12) : c.id;
        labels.push_back(fmt::format("{:<13} {:<22} {}", display_id, c.name, c.state));
    }
    return labels;
}

void refresh_containers(const std::shared_ptr<containers::IContainerEngine>& engine, SharedState& state,
                        const auto& push_output, ftxui::ScreenInteractive& screen) {
    try {
        auto fresh = engine->listContainers();
        auto labels = build_labels(fresh);

        std::lock_guard lock(state.state_mutex);
        state.containers = std::move(fresh);
        state.container_labels = std::move(labels);
        if (!state.container_labels.empty() && state.selected >= static_cast<int>(state.container_labels.size())) {
            state.selected = static_cast<int>(state.container_labels.size()) - 1;
        }
    } catch (const containers::ContainerEngineTransportError& ex) {
        push_output(fmt::format("ERROR listing containers (transport): {}", ex.what()));
    } catch (const containers::ContainerEngineApiError& ex) {
        push_output(fmt::format("ERROR listing containers (api): {}", ex.what()));
    } catch (const containers::ContainerEngineParseError& ex) {
        push_output(fmt::format("ERROR listing containers (parse): {}", ex.what()));
    } catch (const containers::ContainerEngineError& ex) {
        push_output(fmt::format("ERROR listing containers: {}", ex.what()));
    }
    post_refresh(screen);
}

std::optional<containers::Container> selected_container(SharedState& state) {
    std::lock_guard lock(state.state_mutex);
    if (state.containers.empty() || state.selected < 0 || state.selected >= static_cast<int>(state.containers.size())) {
        return std::nullopt;
    }
    return state.containers[static_cast<std::size_t>(state.selected)];
}

void reset_run_output(SharedState& state, ftxui::ScreenInteractive& screen) {
    state.wait_elapsed.store(0);
    state.wait_total.store(0);
    {
        std::lock_guard lock(state.state_mutex);
        state.output_lines.clear();
    }
    state.log_scroll = 1.0F;
    post_refresh(screen);
}

void wait_manifest_duration(const manifests::ChaosManifest& manifest, SharedState& state,
                            ftxui::ScreenInteractive& screen, const auto& push_output,
                            const std::stop_token& stop_token) {
    if (!manifest.duration_s.has_value() || manifest.duration_s.value() <= 0) {
        return;
    }

    const auto secs = static_cast<int>(manifest.duration_s.value());
    state.wait_total.store(secs);
    state.wait_elapsed.store(0);
    post_refresh(screen);

    for (int i = 0; i < secs && !stop_token.stop_requested(); ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        state.wait_elapsed.store(i + 1);
        post_refresh(screen);
    }

    state.wait_total.store(0);
    push_output(fmt::format("  ✓ Waited {}s.", secs));
}

struct RunRequest {
    std::string manifest_path;
    std::string cwd;
};

void execute_run(const std::shared_ptr<containers::IContainerEngine>& engine, SharedState& state,
                 ftxui::ScreenInteractive& screen, const RunRequest& request, const auto& push_output,
                 const std::stop_token& stop_token) {
    reset_run_output(state, screen);

    try {
        const auto manifest = manifests::ManifestParser::parseFromFile(request.manifest_path);
        push_output(fmt::format("▶ Running: {}", manifest.test_name));

        perturbations::PerturbationFactory factory;
        std::vector<std::unique_ptr<perturbations::IPerturbation>> perturbation_instances;
        for (const auto& spec : manifest.perturbations) {
            perturbation_instances.push_back(factory.create(engine, manifest.target, spec));
        }

        perturbations::PerturbationEngine pert_engine;
        const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));
        pert_engine.scheduleAllAsync(std::move(perturbation_instances), duration);

        observability::ObservabilityEngine obs(engine);
        shared::TargetState lastKnownState;
        shared::StateBroadcaster broadcaster;

        if (duration.count() > 0) {
            push_output(fmt::format("  Injecting faults for {}s...", duration.count()));
            auto start_time = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start_time < duration && !stop_token.stop_requested()) {
                lastKnownState = obs.observe(manifest.target.id);
                broadcaster.broadcast(lastKnownState);
                push_output(fmt::format("  📊 State: {} - {}", shared::toString(lastKnownState.status),
                                        lastKnownState.container_id));
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        } else {
            lastKnownState = obs.observe(manifest.target.id);
            broadcaster.broadcast(lastKnownState);
        }

        push_output("  Waiting for perturbations to revert...");
        pert_engine.waitForTeardown();
        push_output("  ✓ Perturbations reverted.");

        shared::TargetState targetState = obs.observe(manifest.target.id);
        const auto results = validation::validate(targetState, manifest.expectations);

        for (const auto& r : results) {
            push_output(fmt::format("  {} {}: {}", r.passed ? "✅" : "❌", r.expectationType, r.message));
        }

        const auto passed = std::ranges::all_of(results, [](const auto& r) { return r.passed; });
        push_output(passed ? ">>> PASSED ✅" : ">>> FAILED ❌");
    } catch (const manifests::ManifestParserError& ex) {
        push_output(fmt::format("❌ Manifest parse error: {}", ex.what()));
        push_output(fmt::format("  (Working directory: {})", request.cwd));
    } catch (const containers::ContainerEngineTransportError& ex) {
        push_output(fmt::format("❌ Container engine transport error: {}", ex.what()));
        push_output(fmt::format("  (Working directory: {})", request.cwd));
    } catch (const containers::ContainerEngineApiError& ex) {
        push_output(fmt::format("❌ Container engine API error: {}", ex.what()));
        push_output(fmt::format("  (Working directory: {})", request.cwd));
    } catch (const containers::ContainerEngineParseError& ex) {
        push_output(fmt::format("❌ Container engine parse error: {}", ex.what()));
        push_output(fmt::format("  (Working directory: {})", request.cwd));
    } catch (const containers::ContainerEngineError& ex) {
        push_output(fmt::format("❌ Container engine error: {}", ex.what()));
        push_output(fmt::format("  (Working directory: {})", request.cwd));
    } catch (const std::filesystem::filesystem_error& ex) {
        push_output(fmt::format("❌ File error: {}", ex.what()));
        push_output(fmt::format("  (Working directory: {})", request.cwd));
    }

    state.wait_total.store(0);
    state.run_in_progress.store(false);
    post_refresh(screen);
}

ftxui::Element render_log_content(SharedState& state) {
    using namespace ftxui;

    std::vector<std::string> snapshot;
    {
        std::lock_guard lock(state.state_mutex);
        snapshot = state.output_lines;
    }

    Elements elems;
    elems.reserve(snapshot.size() + 2);
    for (const auto& line : snapshot) {
        elems.push_back(text(line));
    }

    if (const auto total = state.wait_total.load(); total > 0) {
        const auto elapsed = state.wait_elapsed.load();
        const auto ratio = static_cast<float>(elapsed) / static_cast<float>(total);
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
}

bool handle_log_scroll(ftxui::Event const& e, SharedState& state, ftxui::ScreenInteractive& screen) {
    constexpr float step = 0.05F;
    constexpr float page = 0.25F;

    if (e == ftxui::Event::ArrowDown) {
        state.log_scroll = std::min(1.0F, state.log_scroll + step);
    } else if (e == ftxui::Event::ArrowUp) {
        state.log_scroll = std::max(0.0F, state.log_scroll - step);
    } else if (e == ftxui::Event::PageDown) {
        state.log_scroll = std::min(1.0F, state.log_scroll + page);
    } else if (e == ftxui::Event::PageUp) {
        state.log_scroll = std::max(0.0F, state.log_scroll - page);
    } else if (e == ftxui::Event::Home) {
        state.log_scroll = 0.0F;
    } else if (e == ftxui::Event::End) {
        state.log_scroll = 1.0F;
    } else {
        return false;
    }

    post_refresh(screen);
    return true;
}

struct UiComponents {
    ftxui::Component container_menu;
    ftxui::Component btn_refresh;
    ftxui::Component btn_stop;
    ftxui::Component btn_kill;
    ftxui::Component manifest_input;
    ftxui::Component btn_run;
    ftxui::Component log_scroller;
};

ftxui::Component make_stop_button(const std::shared_ptr<containers::IContainerEngine>& engine, SharedState& state,
                                  const auto& push_output, const auto& refresh) {
    return ftxui::Button("Stop", [engine, &state, &push_output, refresh] {
        const auto c = selected_container(state);
        if (!c.has_value()) {
            push_output("No container selected.");
            return;
        }

        try {
            engine->stopContainer(c->id);
            push_output(fmt::format("✅ Stopped: {}", c->name));
            refresh();
        } catch (const containers::ContainerEngineTransportError& ex) {
            push_output(fmt::format("❌ Stop failed (transport): {}", ex.what()));
        } catch (const containers::ContainerEngineApiError& ex) {
            push_output(fmt::format("❌ Stop failed (api): {}", ex.what()));
        } catch (const containers::ContainerEngineParseError& ex) {
            push_output(fmt::format("❌ Stop failed (parse): {}", ex.what()));
        } catch (const containers::ContainerEngineError& ex) {
            push_output(fmt::format("❌ Stop failed: {}", ex.what()));
        }
    });
}

ftxui::Component make_kill_button(const std::shared_ptr<containers::IContainerEngine>& engine, SharedState& state,
                                  const auto& push_output, const auto& refresh) {
    return ftxui::Button("Kill", [engine, &state, &push_output, refresh] {
        const auto c = selected_container(state);
        if (!c.has_value()) {
            push_output("No container selected.");
            return;
        }

        try {
            engine->killContainer(c->id);
            push_output(fmt::format("✅ Killed: {}", c->name));
            refresh();
        } catch (const containers::ContainerEngineTransportError& ex) {
            push_output(fmt::format("❌ Kill failed (transport): {}", ex.what()));
        } catch (const containers::ContainerEngineApiError& ex) {
            push_output(fmt::format("❌ Kill failed (api): {}", ex.what()));
        } catch (const containers::ContainerEngineParseError& ex) {
            push_output(fmt::format("❌ Kill failed (parse): {}", ex.what()));
        } catch (const containers::ContainerEngineError& ex) {
            push_output(fmt::format("❌ Kill failed: {}", ex.what()));
        }
    });
}

void start_run_worker(const std::shared_ptr<containers::IContainerEngine>& engine, SharedState& state,
                      ftxui::ScreenInteractive& screen, std::jthread& run_thread, const auto& push_output,
                      RunRequest request) {
    run_thread = std::jthread(
        [engine, &state, &screen, &push_output, request = std::move(request)](const std::stop_token& stop_token) {
            execute_run(engine, state, screen, request, push_output, stop_token);
        });
}

void on_run_click(const std::shared_ptr<containers::IContainerEngine>& engine, SharedState& state,
                  ftxui::ScreenInteractive& screen, std::jthread& run_thread, const auto& push_output,
                  const std::string& cwd) {
    if (state.run_in_progress.exchange(true)) {
        push_output("⚠ A run is already in progress.");
        return;
    }

    if (state.manifest_path.empty()) {
        state.run_in_progress.store(false);
        push_output("Please enter a manifest path.");
        return;
    }

    start_run_worker(engine, state, screen, run_thread, push_output,
                     RunRequest{.manifest_path = state.manifest_path, .cwd = cwd});
}

ftxui::Component make_run_button(const std::shared_ptr<containers::IContainerEngine>& engine, SharedState& state,
                                 ftxui::ScreenInteractive& screen, std::jthread& run_thread, const auto& push_output,
                                 const std::string& cwd) {
    return ftxui::Button("Run", [engine, &state, &screen, &run_thread, &push_output, cwd] {
        on_run_click(engine, state, screen, run_thread, push_output, cwd);
    });
}

ftxui::Element render_ui(SharedState& state, const std::string& cwd, const UiComponents& ui) {
    using namespace ftxui;

    const auto in_progress = state.run_in_progress.load();

    Element container_panel;
    {
        std::lock_guard lock(state.state_mutex);
        container_panel = state.container_labels.empty()
                              ? text("  (no containers found)") | dim
                              : ui.container_menu->Render() | vscroll_indicator | frame | flex;
    }

    return vbox({
        hbox({
            text("Chaos Orchestrator TUI") | bold | flex,
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
                ui.btn_refresh->Render() | hcenter,
                separator(),
                ui.btn_stop->Render() | hcenter,
                ui.btn_kill->Render() | hcenter,
            }) | border |
                size(WIDTH, EQUAL, 18),
        }) | flex,

        hbox({
            text(" Manifest: ") | bold,
            ui.manifest_input->Render() | flex,
            ui.btn_run->Render(),
        }) | border,

        vbox({
            hbox({
                text(" Output:") | bold | flex,
                text(" Tab to focus · ↑↓ PgUp/PgDn Home/End to scroll ") | dim,
            }),
            separator(),
            ui.log_scroller->Render() | flex,
        }) | border |
            size(HEIGHT, LESS_THAN, 16),

        text(fmt::format(" CWD: {}", cwd)) | dim,
    });
}

}  // namespace

TuiApp::TuiApp(std::shared_ptr<containers::IContainerEngine> engine) : m_engine(std::move(engine)) {}

int TuiApp::run() const {
    using namespace ftxui;

    const auto cwd = std::filesystem::current_path().string();
    SharedState state;

    auto screen = ScreenInteractive::Fullscreen();
    auto push_output = [&state, &screen](std::string line) { push_output_line(state, screen, std::move(line)); };

    auto refresh = [engine = m_engine, &state, &push_output, &screen] {
        refresh_containers(engine, state, push_output, screen);
    };

    std::jthread run_thread;

    refresh();

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto container_menu = Menu(&state.container_labels, &state.selected);

    auto btn_refresh = Button("Refresh", [refresh] { refresh(); });
    auto btn_stop = make_stop_button(m_engine, state, push_output, refresh);
    auto btn_kill = make_kill_button(m_engine, state, push_output, refresh);

    auto manifest_input = Input(&state.manifest_path, cwd + "/examples/...");
    auto btn_run = make_run_button(m_engine, state, screen, run_thread, push_output, cwd);

    // ── Log content renderer ──────────────────────────────────────────────────
    // Renders ALL lines plus an optional live progress bar.
    // Wrapped in a scrollable shell below.
    auto log_content = Renderer([&state] { return render_log_content(state); });

    // ── Scrollable shell ──────────────────────────────────────────────────────
    // Uses focusPositionRelative(0, log_scroll) | frame — the canonical FTXUI
    // pattern for a scrollable viewport controlled by a float position.
    // CatchEvent intercepts ↑/↓/PgUp/PgDn/Home/End to move log_scroll.
    auto log_scroller = CatchEvent(Renderer(log_content,
                                            [log_content, &state] {
                                                return log_content->Render() |
                                                       focusPositionRelative(0, state.log_scroll) | frame | flex;
                                            }),
                                   [&state, &screen](Event const& e) { return handle_log_scroll(e, state, screen); });

    // ── Layout & top-level renderer ───────────────────────────────────────────
    auto actions_col = Container::Vertical({btn_refresh, btn_stop, btn_kill});
    auto manifest_row = Container::Horizontal({manifest_input, btn_run});
    auto layout = Container::Vertical({container_menu, actions_col, manifest_row, log_scroller});
    const UiComponents ui_components{
        .container_menu = container_menu,
        .btn_refresh = btn_refresh,
        .btn_stop = btn_stop,
        .btn_kill = btn_kill,
        .manifest_input = manifest_input,
        .btn_run = btn_run,
        .log_scroller = log_scroller,
    };

    auto renderer = Renderer(layout, [&state, &cwd, &ui_components] { return render_ui(state, cwd, ui_components); });

    auto final_renderer = CatchEvent(renderer, [&screen](Event const& e) {
        if (e == Event::Character('q') || e == Event::Escape) {
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(final_renderer);

    if (run_thread.joinable()) {
        run_thread.request_stop();
        run_thread.join();
    }

    return 0;
}

}  // namespace chaos::orchestrator::interfaces::tui
