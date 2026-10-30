// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

namespace chatterino {

class Args;

/// Lets later launches with the same --instance name open or close channel
/// tabs in this process instead of starting another Chatterino.
class InstanceServer : public QObject
{
public:
    explicit InstanceServer(const QString &name);

    /// Hands --channels and --remove to the running instance named by
    /// --instance. Returns true if this process has nothing left to do.
    static bool forward(const Args &args);

    /// Local socket the instance called `name` listens on
    static QString socketName(const QString &name);

private:
    void handle(const QByteArray &message);

    QLocalServer *server_;
};

}  // namespace chatterino
