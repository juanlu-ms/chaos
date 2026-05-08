#include "interfaces/tui/TuiApp.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
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
#include "manifests/Manifest.hpp"
#include "observability/ObservabilityEngine.hpp"
#include "perturbations/PerturbationEngine.hpp"
#include "perturbations/PerturbationFactory.hpp"
#include "shared/ContainerStatus.hpp"
#include "shared/StateBroadcaster.hpp"
#include "shared/TargetState.hpp"
#include "validation/ValidationEngine.hpp"

namespace chaos::orchestrator::interfaces::tui {

namespace {

struct SharedState {
    std::mutex mtx;
    std::vector<containers::Container> containers;
    std::vector<std::string> container_labels;
    int selected_target = 0;
    std::optional<shared::TargetState> latest_state;
    enum Mode { Dashboard, Wizard } mode = Dashboard;
    int wizard_step = 0;
    std::string test_name = "chaos-test";
    std::string target_id;
    std::vector<manifests::Perturbation> perturbations;
    std::vector<manifests::Expectation> expectations;
    int duration_s = 30;
    std::atomic<bool> run_in_progress{false};
    std::vector<std::string> output_lines;
    std::atomic<int> wait_elapsed{0};
    std::atomic<int> wait_total{0};
};

void post_refresh(ftxui::ScreenInteractive& screen) { screen.PostEvent(ftxui::Event::Custom); }

void push_line(SharedState& state, ftxui::ScreenInteractive& screen, std::string line) {
    {
        std::lock_guard lock(state.mtx);
        state.output_lines.push_back(std::move(line));
    }
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
                        ftxui::ScreenInteractive& screen) {
    try {
        auto fresh = engine->listContainers();
        auto labels = build_labels(fresh);
        {
            std::lock_guard lock(state.mtx);
            state.containers = std::move(fresh);
            state.container_labels = std::move(labels);
            if (state.selected_target >= static_cast<int>(state.container_labels.size())) {
                state.selected_target = std::max(0, static_cast<int>(state.container_labels.size()) - 1);
            }
        }
    } catch (const containers::ContainerEngineError& ex) {
        push_line(state, screen, fmt::format("ERROR: {}", ex.what()));
    }
    post_refresh(screen);
}

ftxui::Element render_dashboard(SharedState& state) {
    using namespace ftxui;

    std::vector<std::string> labels;
    int selected = 0;
    std::optional<shared::TargetState> latest;
    {
        std::lock_guard lock(state.mtx);
        labels = state.container_labels;
        selected = state.selected_target;
        latest = state.latest_state;
    }

    Elements container_elems;
    if (labels.empty()) {
        container_elems.push_back(text("  (no containers)") | dim);
    } else {
        for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
            auto prefix = (i == selected) ? text(" > ") | bold : text("   ");
            auto line = text(labels[static_cast<std::size_t>(i)]);
            if (i == selected) {
                line = line | bold | inverted;
            }
            container_elems.push_back(hbox({prefix, line}));
        }
    }

    Elements right_elems;
    right_elems.push_back(text(" Target State") | bold);
    right_elems.push_back(separator());
    if (latest.has_value()) {
        const auto& ts = latest.value();
        right_elems.push_back(text(fmt::format(" ID:     {}", ts.container_id)));
        right_elems.push_back(text(fmt::format(" Status: {}", shared::toString(ts.status))));
        if (ts.cpu_usage_percent.has_value()) {
            right_elems.push_back(text(fmt::format(" CPU:    {:.1f}%", ts.cpu_usage_percent.value())));
        }
        if (ts.memory_usage_mb.has_value()) {
            right_elems.push_back(text(fmt::format(" Memory: {:.1f} MB", ts.memory_usage_mb.value())));
        }
        if (ts.container_ip.has_value()) {
            right_elems.push_back(text(fmt::format(" IP:     {}", ts.container_ip.value())));
        }
    } else {
        right_elems.push_back(text(" (no state data)") | dim);
    }

    return vbox({
        hbox({
            text(" CHAOS TUI -- Dashboard") | bold | flex,
            text(" [Tab] Wizard  [r] Refresh  [q] Quit ") | dim,
        }) | color(Color::White) |
            bgcolor(Color::Blue),
        hbox({
            vbox({
                text(" CONTAINERS") | bold,
                separator(),
                vbox(std::move(container_elems)) | vscroll_indicator | frame | flex,
            }) | border |
                flex,
            vbox(std::move(right_elems)) | border | size(WIDTH, GREATER_THAN, 40),
        }) | flex,
    });
}

ftxui::Element render_wizard(SharedState& state) {
    using namespace ftxui;

    int step = 0;
    {
        std::lock_guard lock(state.mtx);
        step = state.wizard_step;
    }

    constexpr std::array<const char*, 4> step_names = {"Target", "Perturbations", "Expectations", "Run"};

    auto header = hbox({
                      text(" CHAOS TUI -- Wizard") | bold | flex,
                      text(" [Tab] Dashboard  [q] Back  [Enter] Confirm ") | dim,
                  }) |
                  color(Color::White) | bgcolor(Color::Blue);

    Elements steps;
    for (int i = 0; i < 4; ++i) {
        auto s = fmt::format(" Step {}/4: {}", i + 1, step_names[static_cast<std::size_t>(i)]);
        steps.push_back((i == step) ? text(s) | bold : text(s) | dim);
        if (i < 3) {
            steps.push_back(text(" -> ") | dim);
        }
    }
    auto step_bar = hbox(std::move(steps));

    Element content;

    switch (step) {
        case 0: {
            std::lock_guard lock(state.mtx);
            Elements elems;
            if (state.container_labels.empty()) {
                elems.push_back(text("  (no containers)") | dim);
            } else {
                for (int i = 0; i < static_cast<int>(state.container_labels.size()); ++i) {
                    auto prefix = (i == state.selected_target) ? text(" > ") | bold : text("   ");
                    auto line = text(state.container_labels[static_cast<std::size_t>(i)]);
                    if (i == state.selected_target) {
                        line = line | bold | inverted;
                    }
                    elems.push_back(hbox({prefix, line}));
                }
            }
            content = vbox({
                          text(" Select Target Container") | bold,
                          separator(),
                          vbox(std::move(elems)) | vscroll_indicator | frame | flex,
                          separator(),
                          text(" arrow keys to navigate, Enter to confirm ") | dim,
                      }) |
                      border | flex;
            break;
        }
        case 1: {
            std::lock_guard lock(state.mtx);
            Elements elems;
            for (const auto& p : state.perturbations) {
                elems.push_back(text(fmt::format("  - {} ({} params)", p.type, p.parameters.size())));
            }
            if (elems.empty()) {
                elems.push_back(text("  (no perturbations)") | dim);
            }
            content = vbox({
                          text(" Configure Perturbations") | bold,
                          separator(),
                          vbox(std::move(elems)) | flex,
                          separator(),
                          text(" [a] Add kill  [d] Delete last  [Enter] Confirm  [Backspace] Back ") | dim,
                      }) |
                      border | flex;
            break;
        }
        case 2: {
            std::lock_guard lock(state.mtx);
            Elements elems;
            for (const auto& e : state.expectations) {
                elems.push_back(text(fmt::format("  - {} ({} params)", e.type, e.parameters.size())));
            }
            if (elems.empty()) {
                elems.push_back(text("  (no expectations)") | dim);
            }
            content = vbox({
                          text(" Set Expectations") | bold,
                          separator(),
                          vbox(std::move(elems)) | flex,
                          separator(),
                          text(" [a] Add container_running  [d] Delete last  [Enter] Confirm  [Backspace] Back ") | dim,
                      }) |
                      border | flex;
            break;
        }
        case 3: {
            std::string tname;
            std::string tid;
            int dur = 0;
            bool running = false;
            int elapsed = 0, total = 0;
            std::size_t num_perts = 0, num_exps = 0;
            std::vector<std::string> output;
            {
                std::lock_guard lock(state.mtx);
                tname = state.test_name;
                tid = state.target_id;
                dur = state.duration_s;
                running = state.run_in_progress.load();
                elapsed = state.wait_elapsed.load();
                total = state.wait_total.load();
                output = state.output_lines;
                num_perts = state.perturbations.size();
                num_exps = state.expectations.size();
            }

            Elements summary;
            summary.push_back(text(fmt::format(" Test Name:       {}", tname)));
            summary.push_back(text(fmt::format(" Target ID:       {}", tid)));
            summary.push_back(text(fmt::format(" Perturbations:   {}", num_perts)));
            summary.push_back(text(fmt::format(" Expectations:    {}", num_exps)));
            summary.push_back(text(fmt::format(" Duration:        {}s", dur)));

            Elements output_elems;
            for (const auto& l : output) {
                output_elems.push_back(text(l));
            }
            if (output_elems.empty() && !running) {
                output_elems.push_back(text(" (press Enter to start the run)") | dim);
            }

            if (total > 0) {
                auto ratio = static_cast<float>(elapsed) / static_cast<float>(total);
                output_elems.push_back(hbox({
                                           text(fmt::format(" Running  {}/{}s  ", elapsed, total)),
                                           gauge(ratio) | flex,
                                       }) |
                                       color(Color::Yellow));
            }

            content = vbox({
                          text(" Run Chaos Test") | bold,
                          separator(),
                          vbox(std::move(summary)) | border,
                          separator(),
                          vbox(std::move(output_elems)) | vscroll_indicator | frame | flex,
                          separator(),
                          running ? text(" (run in progress...) ") | dim
                                  : text(" [Enter] Start run  [Backspace] Back ") | dim,
                      }) |
                      border | flex;
            break;
        }
    }

    return vbox({
        header,
        step_bar,
        separator(),
        content | flex,
    });
}

manifests::ChaosManifest build_manifest(const SharedState& state) {
    manifests::ChaosManifest m;
    m.test_name = state.test_name;
    m.target.id = state.target_id;
    m.perturbations = state.perturbations;
    m.expectations = state.expectations;
    m.duration_s = static_cast<uint32_t>(state.duration_s);
    return m;
}

void execute_run(const std::shared_ptr<containers::IContainerEngine>& engine, SharedState& state,
                 ftxui::ScreenInteractive& screen, const std::stop_token& stop) {
    manifests::ChaosManifest manifest;
    {
        std::lock_guard lock(state.mtx);
        manifest = build_manifest(state);
    }

    auto pl = [&](std::string line) { push_line(state, screen, std::move(line)); };

    auto on_state = [&](const shared::TargetState& ts) {
        std::lock_guard lock(state.mtx);
        state.latest_state = ts;
    };

    try {
        pl(fmt::format("> Running: {}", manifest.test_name));

        perturbations::PerturbationFactory factory;
        std::vector<std::unique_ptr<perturbations::IPerturbation>> instances;
        for (const auto& spec : manifest.perturbations) {
            instances.push_back(factory.create(engine, manifest.target, spec));
        }

        perturbations::PerturbationEngine pert_engine;
        const auto duration = std::chrono::seconds(manifest.duration_s.value_or(0));
        pert_engine.scheduleAllAsync(std::move(instances), duration);

        observability::ObservabilityEngine obs(engine);
        shared::StateBroadcaster broadcaster;
        auto handle = broadcaster.subscribe(on_state);

        if (duration.count() > 0) {
            pl(fmt::format("  Injecting faults for {}s...", duration.count()));
            auto start_time = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start_time < duration && !stop.stop_requested()) {
                auto ts = obs.observe(manifest.target.id);
                broadcaster.broadcast(ts);
                auto elapsed_s =
                    std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time)
                        .count();
                state.wait_elapsed.store(static_cast<int>(elapsed_s));
                state.wait_total.store(static_cast<int>(duration.count()));
                post_refresh(screen);
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        } else {
            auto ts = obs.observe(manifest.target.id);
            broadcaster.broadcast(ts);
        }

        broadcaster.unsubscribe(handle);

        pl("  Waiting for perturbations to revert...");
        pert_engine.waitForTeardown();
        pl("  Reverted.");

        auto final_state = obs.observe(manifest.target.id);
        auto results = validation::validate(final_state, manifest.expectations);

        for (const auto& r : results) {
            pl(fmt::format("  {}: {}", r.passed ? "PASS" : "FAIL", r.message));
        }

        bool all_passed = std::all_of(results.begin(), results.end(), [](const auto& r) { return r.passed; });
        pl(all_passed ? ">>> PASSED" : ">>> FAILED");

    } catch (const std::exception& ex) {
        pl(fmt::format("ERROR: {}", ex.what()));
    }

    state.wait_total.store(0);
    state.run_in_progress.store(false);
    post_refresh(screen);
}

}  // namespace

TuiApp::TuiApp(std::shared_ptr<containers::IContainerEngine> engine) : m_engine(std::move(engine)) {}

int TuiApp::run() const {
    using namespace ftxui;

    SharedState state;
    auto screen = ScreenInteractive::Fullscreen();

    auto do_refresh = [engine = m_engine, &state, &screen] { refresh_containers(engine, state, screen); };

    do_refresh();
    std::jthread run_thread;

    auto renderer = Renderer(
        [&state] { return state.mode == SharedState::Dashboard ? render_dashboard(state) : render_wizard(state); });

    auto component = CatchEvent(renderer, [&](Event const& e) {
        if (e == Event::Tab) {
            state.mode = (state.mode == SharedState::Dashboard) ? SharedState::Wizard : SharedState::Dashboard;
            post_refresh(screen);
            return true;
        }

        if (state.mode == SharedState::Dashboard) {
            if (e == Event::Character('q') || e == Event::Escape) {
                screen.ExitLoopClosure()();
                return true;
            }
            if (e == Event::ArrowUp || e == Event::Character('k')) {
                std::lock_guard lock(state.mtx);
                state.selected_target = std::max(0, state.selected_target - 1);
                post_refresh(screen);
                return true;
            }
            if (e == Event::ArrowDown || e == Event::Character('j')) {
                std::lock_guard lock(state.mtx);
                int max_val = std::max(0, static_cast<int>(state.container_labels.size()) - 1);
                state.selected_target = std::min(max_val, state.selected_target + 1);
                post_refresh(screen);
                return true;
            }
            if (e == Event::Character('r')) {
                do_refresh();
                return true;
            }
            return false;
        }

        // Wizard mode
        if (e == Event::Character('q') || e == Event::Escape) {
            state.mode = SharedState::Dashboard;
            post_refresh(screen);
            return true;
        }

        int step = state.wizard_step;

        if (e == Event::Backspace) {
            if (step == 0) {
                state.mode = SharedState::Dashboard;
            } else {
                state.wizard_step = step - 1;
            }
            post_refresh(screen);
            return true;
        }

        if (e == Event::Return) {
            if (step == 0) {
                std::lock_guard lock(state.mtx);
                if (!state.containers.empty() && state.selected_target >= 0 &&
                    state.selected_target < static_cast<int>(state.containers.size())) {
                    state.target_id = state.containers[static_cast<std::size_t>(state.selected_target)].id;
                }
                state.wizard_step = 1;
                post_refresh(screen);
                return true;
            }
            if (step == 1) {
                state.wizard_step = 2;
                post_refresh(screen);
                return true;
            }
            if (step == 2) {
                state.wizard_step = 3;
                post_refresh(screen);
                return true;
            }
            if (step == 3 && !state.run_in_progress.exchange(true)) {
                {
                    std::lock_guard lock(state.mtx);
                    state.output_lines.clear();
                }
                state.wait_elapsed.store(0);
                state.wait_total.store(state.duration_s);
                post_refresh(screen);
                run_thread = std::jthread([engine = m_engine, &state, &screen](const std::stop_token& st) {
                    execute_run(engine, state, screen, st);
                });
                post_refresh(screen);
                return true;
            }
        }

        if (e == Event::Character('a')) {
            if (step == 1) {
                std::lock_guard lock(state.mtx);
                state.perturbations.push_back({"kill", {}});
                post_refresh(screen);
                return true;
            }
            if (step == 2) {
                std::lock_guard lock(state.mtx);
                state.expectations.push_back({"container_running", {}});
                post_refresh(screen);
                return true;
            }
        }

        if (e == Event::Character('d')) {
            if (step == 1) {
                std::lock_guard lock(state.mtx);
                if (!state.perturbations.empty()) {
                    state.perturbations.pop_back();
                }
                post_refresh(screen);
                return true;
            }
            if (step == 2) {
                std::lock_guard lock(state.mtx);
                if (!state.expectations.empty()) {
                    state.expectations.pop_back();
                }
                post_refresh(screen);
                return true;
            }
        }

        if (step == 0 && (e == Event::ArrowUp || e == Event::Character('k'))) {
            std::lock_guard lock(state.mtx);
            state.selected_target = std::max(0, state.selected_target - 1);
            post_refresh(screen);
            return true;
        }

        if (step == 0 && (e == Event::ArrowDown || e == Event::Character('j'))) {
            std::lock_guard lock(state.mtx);
            int max_val = std::max(0, static_cast<int>(state.container_labels.size()) - 1);
            state.selected_target = std::min(max_val, state.selected_target + 1);
            post_refresh(screen);
            return true;
        }

        return false;
    });

    screen.Loop(component);

    if (run_thread.joinable()) {
        run_thread.request_stop();
        run_thread.join();
    }

    return 0;
}

}  // namespace chaos::orchestrator::interfaces::tui
