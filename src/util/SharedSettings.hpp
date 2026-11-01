// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <pajlada/settings/settingmanager.hpp>
#include <QString>
#include <rapidjson/document.h>

#include <cstdint>

/// chatterino-slim: saving a settings.json that several Chatterino processes
/// use at the same time. Each process writes only the settings it changed, so
/// one process can't undo another's changes with its older copy.
namespace chatterino::sharedsettings {

/// Applies what changed from `base` to `local` onto `target` and leaves every
/// other value in `target` alone. Objects are merged key by key; arrays and
/// plain values are replaced whole. Returns true if `target` changed.
bool applyChanges(const rapidjson::Value &base, const rapidjson::Value &local,
                  rapidjson::Value &target,
                  rapidjson::Document::AllocatorType &allocator);

/// Saves `local` to `path`. Under a lock shared by all processes, it reads
/// the file again and applies only the changes from `base` to `local`. On
/// success, `base` becomes a copy of `local`.
pajlada::Settings::SettingManager::SaveResult save(
    const QString &path, const rapidjson::Value &local,
    rapidjson::Document &base, std::uint8_t backupSlots);

}  // namespace chatterino::sharedsettings
