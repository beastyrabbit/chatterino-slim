// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/SharedSettings.hpp"

#include "common/QLogging.hpp"
#include "util/FilesystemHelpers.hpp"

#include <pajlada/settings/backup.hpp>
#include <QFile>
#include <QLockFile>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <fstream>
#include <system_error>

namespace chatterino::sharedsettings {

namespace {

/// Long enough for a slow disk, short enough not to freeze the window for
/// long if another process hangs while holding the lock
constexpr int LOCK_TIMEOUT_MS = 5000;

}  // namespace

bool applyChanges(const rapidjson::Value &base, const rapidjson::Value &local,
                  rapidjson::Value &target,
                  rapidjson::Document::AllocatorType &allocator)
{
    if (base == local)
    {
        return false;
    }

    if (!base.IsObject() || !local.IsObject() || !target.IsObject())
    {
        if (target == local)
        {
            return false;
        }
        target.CopyFrom(local, allocator);
        return true;
    }

    // A key this process added counts as changed from an empty object, so
    // keys that other processes added to the same object are kept
    static const rapidjson::Value emptyObject(rapidjson::kObjectType);

    bool changed = false;
    for (const auto &member : local.GetObject())
    {
        auto inBase = base.FindMember(member.name);
        const auto &old =
            inBase == base.MemberEnd() ? emptyObject : inBase->value;
        if (old == member.value)
        {
            continue;
        }

        auto inTarget = target.FindMember(member.name);
        if (inTarget == target.MemberEnd())
        {
            // Added here, or changed here after another process removed it.
            // Only what changed here is written, not the unchanged rest.
            target.AddMember(rapidjson::Value(member.name, allocator),
                             rapidjson::Value(member.value.IsObject()
                                                  ? rapidjson::kObjectType
                                                  : rapidjson::kNullType),
                             allocator);
            inTarget = target.FindMember(member.name);
            changed = true;
        }
        changed = applyChanges(old, member.value, inTarget->value, allocator) ||
                  changed;
    }

    for (const auto &member : base.GetObject())
    {
        if (!local.HasMember(member.name) && target.HasMember(member.name))
        {
            target.EraseMember(member.name);
            changed = true;
        }
    }

    return changed;
}

pajlada::Settings::SettingManager::SaveResult save(
    const QString &path, const rapidjson::Value &local,
    rapidjson::Document &base, std::uint8_t backupSlots)
{
    using SaveResult = pajlada::Settings::SettingManager::SaveResult;

    if (base == local)
    {
        return SaveResult::Skipped;
    }

    QLockFile lock(path + ".lock");
    if (!lock.tryLock(LOCK_TIMEOUT_MS))
    {
        qCWarning(chatterinoSettings)
            << "Not saving settings: another Chatterino holds" << path
            << "locked";
        return SaveResult::Failed;
    }

    rapidjson::Document onDisk;
    bool changed = true;
    QFile file(path);
    if (file.exists())
    {
        if (!file.open(QIODevice::ReadOnly))
        {
            qCWarning(chatterinoSettings)
                << "Not saving settings: can't read" << path;
            return SaveResult::Failed;
        }
        auto data = file.readAll();
        file.close();
        onDisk.Parse(data.constData(), data.size());
        if (onDisk.HasParseError() || !onDisk.IsObject())
        {
            // Writing our whole copy could undo other windows' changes
            qCWarning(chatterinoSettings)
                << "Not saving settings: can't parse" << path;
            return SaveResult::Failed;
        }
        changed = applyChanges(base, local, onDisk, onDisk.GetAllocator());
    }
    else
    {
        // Nothing to merge with: write this process's settings, as upstream
        onDisk.CopyFrom(local, onDisk.GetAllocator());
    }

    if (changed)
    {
        std::error_code ec;
        pajlada::Settings::Backup::saveWithBackup(
            qStringToStdPath(path), {.enabled = true, .numSlots = backupSlots},
            [&onDisk](const std::filesystem::path &tmpPath,
                      std::error_code &writeError) {
                rapidjson::StringBuffer buffer;
                rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
                onDisk.Accept(writer);

                std::ofstream fh(tmpPath, std::ios::binary | std::ios::out);
                fh.write(buffer.GetString(),
                         static_cast<std::streamsize>(buffer.GetSize()));
                fh.close();
                if (!fh)
                {
                    writeError = std::make_error_code(std::errc::io_error);
                }
            },
            ec);
        if (ec)
        {
            qCWarning(chatterinoSettings)
                << "Saving shared settings failed:" << ec.message().c_str();
            return SaveResult::Failed;
        }
    }

    // A fresh document, so the old copy's memory is freed
    rapidjson::Document fresh;
    fresh.CopyFrom(local, fresh.GetAllocator());
    base.Swap(fresh);

    return changed ? SaveResult::Success : SaveResult::Skipped;
}

}  // namespace chatterino::sharedsettings
