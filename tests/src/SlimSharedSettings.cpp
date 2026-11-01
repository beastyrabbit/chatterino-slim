// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "Test.hpp"
#include "util/SharedSettings.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <rapidjson/document.h>
#include <rapidjson/pointer.h>

using namespace chatterino;
using SaveResult = pajlada::Settings::SettingManager::SaveResult;

namespace {

rapidjson::Document json(const char *text)
{
    rapidjson::Document document;
    document.Parse(text);
    EXPECT_FALSE(document.HasParseError()) << text;
    return document;
}

/// One Chatterino process: what it loaded and what it has now
struct Process {
    rapidjson::Document base;
    rapidjson::Document local;

    explicit Process(const QString &path)
    {
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::ReadOnly));
        auto data = file.readAll();
        this->base.Parse(data.constData(), data.size());
        this->local.CopyFrom(this->base, this->local.GetAllocator());
    }

    void set(const char *pointer, const rapidjson::Value &value)
    {
        rapidjson::Pointer(pointer).Set(this->local, value);
    }

    SaveResult save(const QString &path)
    {
        return sharedsettings::save(path, this->local, this->base, 3);
    }
};

void write(const QString &path, const char *text)
{
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
}

rapidjson::Document read(const QString &path)
{
    QFile file(path);
    EXPECT_TRUE(file.open(QIODevice::ReadOnly));
    auto data = file.readAll();
    rapidjson::Document document;
    document.Parse(data.constData(), data.size());
    return document;
}

}  // namespace

TEST(SlimSharedSettings, AppliesOnlyLocalChanges)
{
    auto base = json(R"({"a": {"x": 1, "y": 1}, "gone": 1, "keep": [1]})");
    auto local = json(R"({"a": {"x": 2, "y": 1}, "new": true, "keep": [1]})");
    auto target =
        json(R"({"a": {"x": 1, "y": 5}, "gone": 1, "keep": [1, 2], "o": 3})");

    EXPECT_TRUE(sharedsettings::applyChanges(base, local, target,
                                             target.GetAllocator()));
    EXPECT_EQ(target, json(R"({"a": {"x": 2, "y": 5}, "keep": [1, 2],
                               "o": 3, "new": true})"));
}

TEST(SlimSharedSettings, NoLocalChangesLeavesTargetAlone)
{
    auto base = json(R"({"a": 1})");
    auto target = json(R"({"a": 2, "b": 3})");
    EXPECT_FALSE(sharedsettings::applyChanges(base, base, target,
                                              target.GetAllocator()));
    EXPECT_EQ(target, json(R"({"a": 2, "b": 3})"));
}

TEST(SlimSharedSettings, ObjectsAddedInTwoProcessesAreMerged)
{
    auto base = json(R"({})");
    auto local = json(R"({"header": {"uptime": true}})");
    auto target = json(R"({"header": {"game": true}})");
    EXPECT_TRUE(sharedsettings::applyChanges(base, local, target,
                                             target.GetAllocator()));
    EXPECT_EQ(target, json(R"({"header": {"game": true, "uptime": true}})"));
}

/// Another window removed the account. An edit here must not bring back the
/// rest of it, such as the old token.
TEST(SlimSharedSettings, RemovedObjectGetsOnlyChangedKeysBack)
{
    auto base = json(R"({"uid1": {"username": "a", "oauthToken": "old"}})");
    auto local = json(R"({"uid1": {"username": "b", "oauthToken": "old"}})");
    auto target = json(R"({})");
    EXPECT_TRUE(sharedsettings::applyChanges(base, local, target,
                                             target.GetAllocator()));
    EXPECT_EQ(target, json(R"({"uid1": {"username": "b"}})"));
}

TEST(SlimSharedSettings, RemovedElsewhereStaysRemovedUnlessChangedHere)
{
    auto base = json(R"({"untouched": 1, "edited": 1})");
    auto local = json(R"({"untouched": 1, "edited": 2})");
    auto target = json(R"({})");
    EXPECT_TRUE(sharedsettings::applyChanges(base, local, target,
                                             target.GetAllocator()));
    EXPECT_EQ(target, json(R"({"edited": 2})"));
}

/// Two windows share settings.json. The one closed last must not undo what
/// the other changed, or bring back a login the other replaced.
TEST(SlimSharedSettings, StaleProcessDoesNotOverwriteOthers)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    auto path = dir.filePath("settings.json");
    write(path, R"({"appearance": {"showReplyButton": false},
                    "accounts": {"uid1": {"oauthToken": "old"}},
                    "misc": {"x": 1}})");

    Process first(path);
    Process second(path);

    first.set("/appearance/showReplyButton", rapidjson::Value(true));
    rapidjson::Value token("new");
    first.set("/accounts/uid1/oauthToken", token);
    EXPECT_EQ(first.save(path), SaveResult::Success);

    // Unchanged: nothing to write
    EXPECT_EQ(second.save(path), SaveResult::Skipped);

    second.set("/misc/x", rapidjson::Value(2));
    EXPECT_EQ(second.save(path), SaveResult::Success);

    EXPECT_EQ(read(path), json(R"({"appearance": {"showReplyButton": true},
                                   "accounts": {"uid1": {"oauthToken": "new"}},
                                   "misc": {"x": 2}})"));

    // A later change in the same window is applied on top of the last save
    first.set("/misc/y", rapidjson::Value(3));
    EXPECT_EQ(first.save(path), SaveResult::Success);
    EXPECT_EQ(read(path)["misc"], json(R"({"x": 2, "y": 3})"));
}

TEST(SlimSharedSettings, UnparsableFileIsNotOverwritten)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    auto path = dir.filePath("settings.json");
    write(path, "{broken");

    auto base = json(R"({"a": 1})");
    auto local = json(R"({"a": 2})");
    EXPECT_EQ(sharedsettings::save(path, local, base, 3), SaveResult::Failed);
    QFile file(path);
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), "{broken");
}

TEST(SlimSharedSettings, WritesEverythingWhenFileIsMissing)
{
    QTemporaryDir dir;
    ASSERT_TRUE(dir.isValid());
    auto path = dir.filePath("settings.json");

    rapidjson::Document base(rapidjson::kObjectType);
    auto local = json(R"({"a": 1})");
    EXPECT_EQ(sharedsettings::save(path, local, base, 3), SaveResult::Success);
    EXPECT_EQ(read(path), local);
    EXPECT_EQ(base, local);
}
