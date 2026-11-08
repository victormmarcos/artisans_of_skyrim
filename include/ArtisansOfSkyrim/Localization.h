// Artisans of Skyrim
// Copyright (C) 2026 victormmarcos
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <format>
#include <string>
#include <string_view>

// Display text translation.
//
// The code always works with English text. Right before text reaches the
// player (menu, message boxes, courier letters) it is looked up in
// Interface/Translations/ArtisansOfSkyrim_<LANGUAGE>.txt with the key
// "$AOS_<English text>". Missing keys simply keep the English text, so a
// partial translation never breaks anything.
//
// Numbers are matched with "{}": "Ready in 3 days" also finds
// "$AOS_Ready in {} days". Material lists joined with " / " are translated
// part by part.
namespace AOS::Text
{
    // Loads the translation file for the game language (call at kDataLoaded).
    void Initialize();

    // Translated text for one English string, or the string itself.
    [[nodiscard]] std::string Localize(std::string_view a_english);

    // Translates a message line by line (translation files hold one line each).
    [[nodiscard]] std::string LocalizeMessage(std::string_view a_text);

    // Translated "{}" template for an English template, or the template itself.
    [[nodiscard]] std::string Template(std::string_view a_english);

    // std::format with a translated template. A broken translation falls back to English.
    template <class... Args>
    [[nodiscard]] std::string Format(std::string_view a_english, const Args&... a_args)
    {
        try {
            return std::vformat(Template(a_english), std::make_format_args(a_args...));
        } catch (const std::format_error&) {
            return std::vformat(a_english, std::make_format_args(a_args...));
        }
    }
}
