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

#include "ArtisansOfSkyrim/Localization.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <algorithm>
#include <cctype>
#include <optional>
#include <mutex>
#include <unordered_map>
#include <vector>

using namespace std::literals;

namespace AOS::Text
{
    namespace
    {
        constexpr std::string_view kKeyPrefix = "$AOS_";

        std::mutex cacheMutex;
        std::unordered_map<std::string, std::optional<std::string>> cache;

        // Direct translator lookup. SKSE::Translation::Translate is not used
        // because it treats "{}" in keys as nested keys.
        [[nodiscard]] std::optional<std::string> Lookup(const std::string& a_key, bool& a_ready)
        {
            a_ready = false;
            const auto manager = RE::BSScaleformManager::GetSingleton();
            const auto loader = manager ? manager->loader : nullptr;
            const auto translator = loader ?
                loader->GetStateAddRef<RE::GFxTranslator>(RE::GFxState::StateType::kTranslator) :
                nullptr;
            if (!translator) return std::nullopt;
            a_ready = true;

            const auto key = SKSE::stl::utf8_to_utf16(a_key).value_or(L""s);
            RE::GFxWStringBuffer result;
            RE::GFxTranslator::TranslateInfo info;
            info.key = key.c_str();
            info.result = std::addressof(result);
            translator->Translate(std::addressof(info));
            translator->Release();
            if (result.empty()) return std::nullopt;

            auto text = SKSE::stl::utf16_to_utf8(result.c_str()).value_or(""s);
            if (text.empty() || text == a_key) return std::nullopt;
            return text;
        }

        [[nodiscard]] std::optional<std::string> Find(std::string_view a_english)
        {
            std::string key{ kKeyPrefix };
            key += a_english;
            {
                const std::scoped_lock lock{ cacheMutex };
                if (const auto found = cache.find(key); found != cache.end()) return found->second;
            }
            bool ready = false;
            auto value = Lookup(key, ready);
            // The game keeps translated texts in a case-insensitive pool, so a
            // value such as "Name" can come back as "NAME" from another file.
            // Same text in another case means "no translation": keep ours.
            if (value && std::ranges::equal(*value, a_english, [](unsigned char a, unsigned char b) {
                    return std::tolower(a) == std::tolower(b);
                })) {
                value.reset();
            }
            if (ready) {
                const std::scoped_lock lock{ cacheMutex };
                cache.insert_or_assign(std::move(key), value);
            }
            return value;
        }

        [[nodiscard]] bool HasLetter(std::string_view a_text)
        {
            return std::ranges::any_of(a_text, [](unsigned char a_char) { return std::isalpha(a_char) != 0; });
        }

        // "Ready in 3 days" -> "Ready in {} days" + { "3" }.
        [[nodiscard]] std::string NumberTemplate(std::string_view a_text, std::vector<std::string>& a_numbers)
        {
            std::string result;
            for (std::size_t i = 0; i < a_text.size();) {
                if (std::isdigit(static_cast<unsigned char>(a_text[i]))) {
                    std::size_t end = i;
                    while (end < a_text.size() &&
                           (std::isdigit(static_cast<unsigned char>(a_text[end])) ||
                               ((a_text[end] == '.' || a_text[end] == ',') && end + 1 < a_text.size() &&
                                   std::isdigit(static_cast<unsigned char>(a_text[end + 1]))))) {
                        ++end;
                    }
                    a_numbers.emplace_back(a_text.substr(i, end - i));
                    result += "{}";
                    i = end;
                } else {
                    result += a_text[i++];
                }
            }
            return result;
        }
    }

    void Initialize()
    {
        // SKSE already loads translation files of active plugins; loading again
        // only fills keys that are still missing.
        SKSE::Translation::ParseTranslation("ArtisansOfSkyrim");
        const std::scoped_lock lock{ cacheMutex };
        cache.clear();
    }

    std::string Localize(std::string_view a_english)
    {
        if (a_english.empty() || !HasLetter(a_english)) return std::string{ a_english };
        if (auto found = Find(a_english)) return std::move(*found);

        std::vector<std::string> numbers;
        const auto pattern = NumberTemplate(a_english, numbers);
        if (!numbers.empty()) {
            if (auto found = Find(pattern)) {
                auto& text = *found;
                std::size_t position = 0;
                for (const auto& number : numbers) {
                    position = text.find("{}", position);
                    if (position == std::string::npos) break;
                    text.replace(position, 2, number);
                    position += number.size();
                }
                return text;
            }
        }

        // Material lists such as "Iron / Steel".
        constexpr std::string_view separator = " / ";
        if (a_english.find(separator) != std::string_view::npos) {
            std::string result;
            std::size_t start = 0;
            while (true) {
                const auto end = a_english.find(separator, start);
                result += Localize(a_english.substr(start, end - start));
                if (end == std::string_view::npos) break;
                result += separator;
                start = end + separator.size();
            }
            return result;
        }
        return std::string{ a_english };
    }

    std::string LocalizeMessage(std::string_view a_text)
    {
        std::string result;
        std::size_t start = 0;
        while (true) {
            const auto end = a_text.find('\n', start);
            auto line = a_text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
            // Keep list markers such as "- " outside the key.
            if (line.starts_with("- ")) {
                result += "- ";
                line.remove_prefix(2);
            }
            result += Localize(line);
            if (end == std::string_view::npos) break;
            result += '\n';
            start = end + 1;
        }
        return result;
    }

    std::string Template(std::string_view a_english)
    {
        if (auto found = Find(a_english)) return std::move(*found);
        return std::string{ a_english };
    }
}
