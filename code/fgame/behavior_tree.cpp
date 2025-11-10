/*
===========================================================================
Copyright (C) 2024 the OpenMoHAA team

This file is part of OpenMoHAA source code.

OpenMoHAA source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

OpenMoHAA source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenMoHAA source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

// Added in OPM - Phase 3 Task 3.5
//  Implementation of Blackboard introspection methods for debug visualization

#include "behavior_tree.h"
#include "entity.h"
#include "sentient.h"
#include <sstream>
#include <iomanip>

std::string Blackboard::GetAsString(const std::string &key) const
{
    auto it = data.find(key);
    if (it == data.end()) {
        return "<not set>";
    }

    const std::any &value = it->second;

    // Try common types used in bot AI
    try {
        // Numeric types
        if (value.type() == typeid(int)) {
            return std::to_string(std::any_cast<int>(value));
        }
        if (value.type() == typeid(float)) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << std::any_cast<float>(value);
            return oss.str();
        }
        if (value.type() == typeid(double)) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(2) << std::any_cast<double>(value);
            return oss.str();
        }
        if (value.type() == typeid(size_t)) {
            return std::to_string(std::any_cast<size_t>(value));
        }

        // Boolean
        if (value.type() == typeid(bool)) {
            return std::any_cast<bool>(value) ? "true" : "false";
        }

        // String types
        if (value.type() == typeid(std::string)) {
            return std::any_cast<std::string>(value);
        }
        if (value.type() == typeid(const char *)) {
            return std::string(std::any_cast<const char *>(value));
        }
        if (value.type() == typeid(str)) {
            return std::any_cast<str>(value).c_str();
        }

        // Vector types
        if (value.type() == typeid(Vector)) {
            Vector v = std::any_cast<Vector>(value);
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(1);
            oss << "(" << v.x << ", " << v.y << ", " << v.z << ")";
            return oss.str();
        }

        // Pointer types (show as address)
        if (value.type() == typeid(Entity *)) {
            Entity *ent = std::any_cast<Entity *>(value);
            if (ent) {
                std::ostringstream oss;
                oss << "Entity@" << std::hex << reinterpret_cast<uintptr_t>(ent);
                return oss.str();
            }
            return "nullptr";
        }
        if (value.type() == typeid(Sentient *)) {
            Sentient *sent = std::any_cast<Sentient *>(value);
            if (sent) {
                std::ostringstream oss;
                oss << "Sentient@" << std::hex << reinterpret_cast<uintptr_t>(sent);
                return oss.str();
            }
            return "nullptr";
        }

        // SafePtr types
        if (value.type() == typeid(SafePtr<Entity>)) {
            auto safePtr = std::any_cast<SafePtr<Entity>>(value);
            if (safePtr.Pointer()) {
                std::ostringstream oss;
                oss << "SafePtr<Entity>@" << std::hex << reinterpret_cast<uintptr_t>(safePtr.Pointer());
                return oss.str();
            }
            return "SafePtr<null>";
        }
        if (value.type() == typeid(SafePtr<Sentient>)) {
            auto safePtr = std::any_cast<SafePtr<Sentient>>(value);
            if (safePtr.Pointer()) {
                std::ostringstream oss;
                oss << "SafePtr<Sentient>@" << std::hex << reinterpret_cast<uintptr_t>(safePtr.Pointer());
                return oss.str();
            }
            return "SafePtr<null>";
        }

        // Shared pointers (common in utility AI)
        if (value.type() == typeid(std::shared_ptr<void>)) {
            return "<shared_ptr>";
        }

    } catch (const std::bad_any_cast &) {
        // Type mismatch, fall through to unsupported
    }

    // Unknown type - show type name
    std::ostringstream oss;
    oss << "<" << value.type().name() << ">";
    return oss.str();
}
