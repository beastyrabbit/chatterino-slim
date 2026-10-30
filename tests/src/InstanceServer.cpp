// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "singletons/InstanceServer.hpp"

#include "common/Args.hpp"
#include "common/ProviderId.hpp"
#include "Test.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QUuid>

using namespace chatterino;

namespace {

Args::Channel twitch(const QString &name)
{
    return {.provider = ProviderId::Twitch, .name = name};
}

QString uniqueName()
{
    return "test-" + QUuid::createUuid().toString(QUuid::Id128).left(16);
}

}  // namespace

TEST(InstanceServer, NoInstanceNameStartsNormally)
{
    Args args;
    args.customChannels = {twitch("forsen")};
    EXPECT_FALSE(InstanceServer::forward(args));
}

TEST(InstanceServer, NothingRunningStartsWithChannels)
{
    Args args;
    args.instanceName = uniqueName();
    args.customChannels = {twitch("forsen")};
    EXPECT_FALSE(InstanceServer::forward(args));
}

/// Closing tabs in an instance that isn't running has nothing to do
TEST(InstanceServer, NothingRunningRemoveOnlyExits)
{
    Args args;
    args.instanceName = uniqueName();
    args.removeChannels = {twitch("forsen")};
    EXPECT_TRUE(InstanceServer::forward(args));
}

TEST(InstanceServer, SocketNameIncludesInstanceName)
{
    EXPECT_TRUE(
        InstanceServer::socketName("abc").endsWith("chatterino-instance-abc"));
}

TEST(InstanceServer, ForwardsChannelsToRunningInstance)
{
    auto name = uniqueName();
    QLocalServer server;
    ASSERT_TRUE(server.listen(InstanceServer::socketName(name)))
        << server.errorString().toStdString();

    Args args;
    args.instanceName = name;
    args.customChannels = {twitch("Forsen"), twitch("#pajlada")};
    args.removeChannels = {twitch("#xQc")};
    EXPECT_TRUE(InstanceServer::forward(args));

    ASSERT_TRUE(server.waitForNewConnection(1000));
    auto *socket = server.nextPendingConnection();
    ASSERT_NE(socket, nullptr);
    if (socket->bytesAvailable() == 0)
    {
        socket->waitForReadyRead(1000);
    }
    auto document = QJsonDocument::fromJson(socket->readAll());
    ASSERT_TRUE(document.isObject());

    // Names are cleaned like Chatterino's own channel names: lower case,
    // without a leading '#'
    EXPECT_EQ(document.object()["add"].toArray(),
              QJsonArray({"forsen", "pajlada"}));
    EXPECT_EQ(document.object()["remove"].toArray(), QJsonArray({"xqc"}));
}
