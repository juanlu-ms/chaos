#pragma once

#include <ftxui/screen/color.hpp>

namespace chaos::orchestrator::ui::theme {

/** @brief Primary background color (amber theme: dark brown). */
inline const auto kBgPrimary = ftxui::Color::RGB(0x1a, 0x14, 0x10);
/** @brief Surface/card background color. */
inline const auto kBgSurface = ftxui::Color::RGB(0x24, 0x1e, 0x18);
/** @brief Border color. */
inline const auto kBorder = ftxui::Color::RGB(0x3d, 0x32, 0x28);
/** @brief Primary text color. */
inline const auto kTextPrimary = ftxui::Color::RGB(0xe8, 0xdd, 0xd0);
/** @brief Accent/highlight color (amber/gold). */
inline const auto kAccent = ftxui::Color::RGB(0xe6, 0xa8, 0x17);
/** @brief Success/healthy state color (green). */
inline const auto kSuccess = ftxui::Color::RGB(0x4c, 0xaf, 0x50);
/** @brief Error/failure state color (red). */
inline const auto kError = ftxui::Color::RGB(0xef, 0x53, 0x50);
/** @brief Warning/caution color (amber). */
inline const auto kWarning = ftxui::Color::RGB(0xff, 0x98, 0x00);
/** @brief Dim/muted text color. */
inline const auto kTextDim = ftxui::Color::RGB(0x9e, 0x8f, 0x7e);

}  // namespace chaos::orchestrator::ui::theme
