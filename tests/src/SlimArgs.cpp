// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "common/Args.hpp"
#include "Test.hpp"

#include <QStringList>

using namespace chatterino;

namespace {

Args parse(QStringList arguments)
{
    arguments.prepend("chatterino");
    return Args(arguments);
}

}  // namespace

TEST(SlimArgs, OffByDefault)
{
    auto args = parse({});
    EXPECT_FALSE(args.slim.hideSplitHeader);
    EXPECT_FALSE(args.slim.hideHeaderButtons);
    EXPECT_FALSE(args.slim.hideAddTabButton);
    EXPECT_FALSE(args.slim.hideSingleTab);
    EXPECT_FALSE(args.slim.fillTabWidth);
    EXPECT_FALSE(args.instanceName.has_value());
    EXPECT_TRUE(args.removeChannels.empty());
}

TEST(SlimArgs, SlimImpliesEverySlimOption)
{
    auto args = parse({"--slim"});
    EXPECT_TRUE(args.slim.hideSplitHeader);
    EXPECT_TRUE(args.slim.hideHeaderButtons);
    EXPECT_TRUE(args.slim.hideAddTabButton);
    EXPECT_TRUE(args.slim.hideSingleTab);
    EXPECT_TRUE(args.slim.fillTabWidth);
    // Slim options are session-only and must not stop settings from saving
    EXPECT_FALSE(args.dontSaveSettings);
    // Saving is separate from the layout
    EXPECT_FALSE(args.slim.saveSettings);
}

TEST(SlimArgs, SaveSettings)
{
    auto args = parse({"--save-settings", "-c", "t:forsen"});
    EXPECT_TRUE(args.slim.saveSettings);
    // -c still keeps its layout and changelog prompt out, as upstream
    EXPECT_TRUE(args.dontSaveSettings);
    EXPECT_TRUE(args.currentArguments().contains("--save-settings"));
}

TEST(SlimArgs, SingleOptions)
{
    auto args = parse({"--hide-split-header", "--fill-tabs"});
    EXPECT_TRUE(args.slim.hideSplitHeader);
    EXPECT_TRUE(args.slim.fillTabWidth);
    EXPECT_FALSE(args.slim.hideHeaderButtons);
    EXPECT_FALSE(args.slim.hideAddTabButton);
    EXPECT_FALSE(args.slim.hideSingleTab);

    args = parse({"--hide-header-buttons", "--hide-add-tab-button",
                  "--hide-single-tab"});
    EXPECT_FALSE(args.slim.hideSplitHeader);
    EXPECT_FALSE(args.slim.fillTabWidth);
    EXPECT_TRUE(args.slim.hideHeaderButtons);
    EXPECT_TRUE(args.slim.hideAddTabButton);
    EXPECT_TRUE(args.slim.hideSingleTab);
}

TEST(SlimArgs, Instance)
{
    auto args = parse({"--instance", "twitch-follows_1", "-c",
                       "t:forsen;t:Pajlada", "--remove", "t:xqc"});
    ASSERT_TRUE(args.instanceName.has_value());
    EXPECT_EQ(*args.instanceName, "twitch-follows_1");

    ASSERT_EQ(args.customChannels.size(), 2);
    EXPECT_EQ(args.customChannels[0].name, "forsen");
    EXPECT_EQ(args.customChannels[1].name, "Pajlada");

    ASSERT_EQ(args.removeChannels.size(), 1);
    EXPECT_EQ(args.removeChannels[0].name, "xqc");
}

TEST(SlimArgs, InvalidInstanceNameIsIgnored)
{
    for (const auto *name : {"", "../escape", "has space", "a/b"})
    {
        auto args = parse({"--instance", name});
        EXPECT_FALSE(args.instanceName.has_value()) << name;
    }

    auto args = parse({"--instance", QString(65, u'a')});
    EXPECT_FALSE(args.instanceName.has_value());
    args = parse({"--instance", QString(64, u'a')});
    EXPECT_TRUE(args.instanceName.has_value());
}

/// A restart must keep the session's slim layout and instance, but must not
/// repeat --remove
TEST(SlimArgs, RestartKeepsSlimOptions)
{
    auto args = parse({"--slim", "--fill-tabs", "--instance", "main",
                       "--remove", "t:forsen"});
    auto restart = args.currentArguments();
    EXPECT_TRUE(restart.contains("--slim")) << restart.join(' ').toStdString();
    EXPECT_TRUE(restart.contains("--fill-tabs"))
        << restart.join(' ').toStdString();
    EXPECT_TRUE(restart.contains("--instance"))
        << restart.join(' ').toStdString();
    EXPECT_FALSE(restart.contains("--remove"))
        << restart.join(' ').toStdString();
}
