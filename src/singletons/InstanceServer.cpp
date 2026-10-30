// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/InstanceServer.hpp"

#include "Application.hpp"
#include "common/Args.hpp"
#include "common/Channel.hpp"
#include "common/QLogging.hpp"
#include "providers/twitch/TwitchIrcServer.hpp"
#include "singletons/WindowManager.hpp"
#include "util/Twitch.hpp"
#include "widgets/Notebook.hpp"
#include "widgets/splits/Split.hpp"
#include "widgets/splits/SplitContainer.hpp"
#include "widgets/Window.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

namespace {

using namespace chatterino;

constexpr int TIMEOUT_MS = 1000;

QJsonArray channelNames(const std::vector<Args::Channel> &channels)
{
    QJsonArray names;
    for (const auto &channel : channels)
    {
        // Same normalization as TwitchIrcServer::getOrAddChannel
        names.append(cleanChannelName(channel.name));
    }
    return names;
}

/// Whether every split on the page shows `name`
bool pageShowsOnly(SplitContainer *page, const QString &name)
{
    auto splits = page->getSplits();
    return !splits.empty() && std::ranges::all_of(splits, [&](Split *split) {
        return split->getChannel()->getName().compare(name,
                                                      Qt::CaseInsensitive) == 0;
    });
}

void removeChannel(SplitNotebook &notebook, const QString &name)
{
    for (int i = notebook.getPageCount() - 1; i >= 0; i--)
    {
        auto *page = dynamic_cast<SplitContainer *>(notebook.getPageAt(i));
        if (page != nullptr && pageShowsOnly(page, name))
        {
            notebook.removePage(page);
        }
    }
}

void activateOrAddChannel(SplitNotebook &notebook, const QString &name)
{
    for (int i = 0; i < notebook.getPageCount(); i++)
    {
        auto *page = dynamic_cast<SplitContainer *>(notebook.getPageAt(i));
        if (page != nullptr && pageShowsOnly(page, name))
        {
            notebook.select(page);
            return;
        }
    }

    auto *container = notebook.addPage(true);
    auto *split = new Split(container);
    split->setChannel(getApp()->getTwitch()->getOrAddChannel(name));
    container->insertSplit(split);
}

}  // namespace

namespace chatterino {

QString InstanceServer::socketName(const QString &name)
{
    auto base = QStringLiteral("chatterino-instance-") + name;
#ifdef Q_OS_WIN
    return base;
#else
    // Keep the socket private to this user instead of in /tmp
    auto dir =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    return dir.isEmpty() ? base : dir + u'/' + base;
#endif
}

InstanceServer::InstanceServer(const QString &name)
    : server_(new QLocalServer(this))
{
    auto socketName = InstanceServer::socketName(name);
    this->server_->setSocketOptions(QLocalServer::UserAccessOption);
    // A crashed instance leaves its socket file behind
    QLocalServer::removeServer(socketName);
    if (!this->server_->listen(socketName))
    {
        qCWarning(chatterinoApp) << "Failed to listen as instance" << name
                                 << this->server_->errorString();
        return;
    }

    QObject::connect(this->server_, &QLocalServer::newConnection, this, [this] {
        while (auto *socket = this->server_->nextPendingConnection())
        {
            auto buffer = std::make_shared<QByteArray>();
            QObject::connect(socket, &QLocalSocket::readyRead, this,
                             [socket, buffer] {
                                 buffer->append(socket->readAll());
                             });
            QObject::connect(socket, &QLocalSocket::disconnected, this,
                             [this, socket, buffer] {
                                 buffer->append(socket->readAll());
                                 socket->deleteLater();
                                 this->handle(*buffer);
                             });
        }
    });
}

bool InstanceServer::forward(const Args &args)
{
    if (!args.instanceName)
    {
        return false;
    }

    QLocalSocket socket;
    socket.connectToServer(socketName(*args.instanceName));
    if (!socket.waitForConnected(TIMEOUT_MS))
    {
        // An instance that isn't running has no tabs to close
        return args.customChannels.empty() && !args.removeChannels.empty();
    }

    QJsonObject message{
        {"add", channelNames(args.customChannels)},
        {"remove", channelNames(args.removeChannels)},
    };
    socket.write(QJsonDocument(message).toJson(QJsonDocument::Compact));
    socket.waitForBytesWritten(TIMEOUT_MS);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
    {
        socket.waitForDisconnected(TIMEOUT_MS);
    }
    return true;
}

void InstanceServer::handle(const QByteArray &message)
{
    auto document = QJsonDocument::fromJson(message);
    if (!document.isObject())
    {
        qCWarning(chatterinoApp) << "Ignoring malformed instance message";
        return;
    }

    auto &window = getApp()->getWindows()->getMainWindow();
    auto &notebook = window.getNotebook();

    for (const auto &name : document.object()["remove"].toArray())
    {
        removeChannel(notebook, cleanChannelName(name.toString()));
    }
    for (const auto &name : document.object()["add"].toArray())
    {
        auto channelName = cleanChannelName(name.toString());
        if (!channelName.isEmpty())
        {
            activateOrAddChannel(notebook, channelName);
        }
    }

    if (notebook.getPageCount() == 0)
    {
        window.close();
    }
}

}  // namespace chatterino
