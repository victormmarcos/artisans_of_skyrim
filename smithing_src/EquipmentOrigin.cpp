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

#include "NPCSmithServices/EquipmentOrigin.h"

#include "NPCSmithServices/Domain.h"
#include "NPCSmithServices/FormResolver.h"

#include "RE/Skyrim.h"

namespace NPCSS
{
    bool IsAllowedEquipmentOrigin(
        const RE::TESForm* a_form,
        bool a_includeModAddedEquipment)
    {
        if (a_includeModAddedEquipment) {
            return a_form != nullptr;
        }
        if (!a_form) {
            return false;
        }
        // An item a patch names is wanted on purpose: the installer's
        // "official equipment only" choice only limits what is found on its own.
        if (FormResolver::GetSingleton().IsAllowed(a_form->GetFormID())) {
            return true;
        }

        // Index zero is the file that created the form. Later overrides do
        // not change origin, including ESL/light-plugin overrides.
        const auto origin = a_form->GetFile(0);
        const auto filename = origin ?
            origin->GetFilename() :
            std::string_view{};
        return filename == "ArtisansOfSkyrim.esp" ||
            (!filename.empty() && IsOfficialEquipmentPlugin(filename));
    }
}
