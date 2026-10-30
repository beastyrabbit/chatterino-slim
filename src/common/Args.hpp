// SPDX-FileCopyrightText: 2019 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include "common/ProviderId.hpp"
#include "common/WindowDescriptors.hpp"

#include <QApplication>

#include <optional>

namespace chatterino {

class Paths;

/// Command line arguments passed to Chatterino.
///
/// All accepted arguments:
///
/// Crash recovery:
///   --crash-recovery
///   --cr-exception-code code
///   --cr-exception-message message
///
/// Native messaging:
///   --parent-window
///   --x-attach-split-to-window=window-id
///
/// -v, --verbose
/// -V, --version
/// -c, --channels=t:channel1;t:channel2;...
/// -a, --activate=t:channel
///     --safe-mode
///
/// Slim layout:
///     --slim
///     --hide-split-header
///     --hide-header-buttons
///     --hide-add-tab-button
///     --hide-single-tab
///     --fill-tabs
///
/// Shared instance:
///     --instance=name
///     --remove=t:channel1;t:channel2;...
///
/// See documentation on `QGuiApplication` for documentation on Qt arguments like -platform.
class Args
{
public:
    struct Channel {
        ProviderId provider;
        QString name;
    };

    Args() = default;
    explicit Args(const QApplication &app);
    /// Parses `arguments` as given on the command line, starting with the
    /// program name
    explicit Args(const QStringList &arguments);

    bool printVersion{};

    bool crashRecovery{};
    /// Native, platform-specific exception code from crashpad
    std::optional<uint32_t> exceptionCode{};
    /// Text version of the exception code. Potentially contains more context.
    std::optional<QString> exceptionMessage{};

    bool shouldRunBrowserExtensionHost{};
    // Shows a single chat. Used on windows to embed in another application.
    bool isFramelessEmbed{};
    std::optional<unsigned long long> parentWindowId{};

    // Not settings directly
    bool dontSaveSettings{};
    bool dontLoadMainWindow{};
    std::vector<Channel> customChannels;
    std::optional<Channel> activateChannel;

    /// Slim layout options turned on for this session only. They act as if
    /// the matching setting were enabled, without saving it.
    struct Slim {
        bool hideSplitHeader{};
        bool hideHeaderButtons{};
        bool hideAddTabButton{};
        bool hideSingleTab{};
        bool fillTabWidth{};
    } slim;

    /// Name of a shared instance. A later launch with the same name hands its
    /// --channels and --remove to the running instance instead of starting.
    std::optional<QString> instanceName;
    std::vector<Channel> removeChannels;
    std::optional<QString> initialLogin;
    bool verbose{};
    bool safeMode{};

    bool portableEnable{};
    std::optional<QString> portableDirectory;

    bool useOldScaling = false;

#ifndef NDEBUG
    // twitch event websocket start-server --ssl --port 3012
    bool useLocalEventsub = false;
#endif

    QStringList currentArguments() const;
    std::optional<WindowLayout> makeCustomChannelLayout(
        const QString &windowLayoutFile) const;

private:
    QStringList currentArguments_;
};

}  // namespace chatterino
