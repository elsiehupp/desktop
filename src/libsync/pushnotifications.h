/*
 * Copyright (C) by Felix Weilbach <felix.weilbach@nextcloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once

#include <QWebSocket>
#include <QTimer>

#include "capabilities.h"

namespace OCC {

class Account;
class AbstractCredentials;

class PushNotifications : public QObject
{
    Q_OBJECT

public:
    explicit PushNotifications(Account *account, QObject *parent = nullptr);

    ~PushNotifications();

    /**
     * Setup push notifications
     *
     * This method needs to be called before push notifications can be used.
     */
    void setup();

    /**
     * Set the interval for reconnection attempts
     */
    void setReconnectTimerInterval(uint32_t interval);

    /**
     * Indicates if push notifications ready to use
     *
     * Ready to use means connected and authenticated.
     */
    bool isReady() const;

signals:
    /**
     * Will be emitted after a successful connection and authentication
     */
    void ready();

    /**
     * Will be emitted if files on the server changed
     */
    void filesChanged(Account *account);

    /**
     * Will be emitted if there is a new notification or activity on the server
     */
    void notification(Account *account);

    /**
     * Will be emitted if push notifications are unable to authenticate
     *
     * It's save to call #PushNotifications::setup() after this signal has been emitted.
     */
    void authenticationFailed();

    /**
     * Will be emitted if push notifications are unable to connect or the connection timed out
     *
     * It's save to call #PushNotifications::setup() after this signal has been emitted.
     */
    void connectionLost();

private slots:
    void onWebSocketConnected();
    void onWebSocketDisconnected();
    void onWebSocketTextMessageReceived(const QString &message);
    void onWebSocketError(QAbstractSocket::SocketError error);
    void onWebSocketSslErrors(const QList<QSslError> &errors);

private:
    void openWebSocket();
    void reconnectToWebSocket();
    void closeWebSocket();
    void authenticateOnWebSocket();
    bool tryReconnectToWebSocket();
    void initReconnectTimer();

    void handleAuthenticated();
    void handleNotifyFile();
    void handleInvalidCredentials();
    void handleNotification();

    Account *_account = nullptr;
    QWebSocket *_webSocket = nullptr;
    uint8_t _failedAuthenticationAttemptsCount = 0;
    QTimer *_reconnectTimer = nullptr;
    uint32_t _reconnectTimerInterval = 20 * 1000;
    bool _isReady = false;
};

}
