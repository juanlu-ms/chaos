#pragma once

#include <ftxui/screen/color.hpp>

namespace chaos::orchestrator::ui::theme {

/** @brief Primary background color (amber theme: dark brown). */
inline constexpr auto kBgPrimary = ftxui::Color::RGB(0x1a, 0x14, 0x10);
/** @brief Surface/card background color. */
inline constexpr auto kBgSurface = ftxui::Color::RGB(0x24, 0x1e, 0x18);
/** @brief Border color. */
inline constexpr auto kBorder = ftxui::Color::RGB(0x3d, 0x32, 0x28);
/** @brief Primary text color. */
inline constexpr auto kTextPrimary = ftxui::Color::RGB(0xe8, 0xdd, 0xd0);
/** @brief Accent/highlight color (amber/gold). */
inline constexpr auto kAccent = ftxui::Color::RGB(0xe6, 0xa8, 0x17);
/** @brief Success/healthy state color (green). */
inline constexpr auto kSuccess = ftxui::Color::RGB(0x4c, 0xaf, 0x50);
/** @brief Error/failure state color (red). */
inline constexpr auto kError = ftxui::Color::RGB(0xef, 0x53, 0x50);

}  // namespace chaos::orchestrator::ui::theme
