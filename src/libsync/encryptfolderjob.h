/*
 * Copyright (C) by Kevin Ottens <kevin.ottens@nextcloud.com>
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

#include "account.h"
#include "encryptedfoldermetadatahandler.h"
#include "syncfileitem.h"
#include "owncloudpropagator.h"

#include <QObject>

namespace OCC {
class SyncJournalDb;

class OWNCLOUDSYNC_EXPORT EncryptFolderJob : public QObject
{
    Q_OBJECT
public:
    enum Status {
        Success = 0,
        Error,
    };
    Q_ENUM(Status)

    explicit EncryptFolderJob(const AccountPtr &account,
                              SyncJournalDb *journal,
                              const QString &path,
                              const QByteArray &fileId,
                              OwncloudPropagator *propagator = nullptr,
                              SyncFileItemPtr item = {},
                              QObject *parent = nullptr);
    void start();

    [[nodiscard]] QString errorString() const;

signals:
    void finished(int status, EncryptionStatusEnums::ItemEncryptionStatus encryptionStatus);

public slots:
    void setPathNonEncrypted(const QString &pathNonEncrypted);

private:
    void uploadMetadata();

private slots:
    void slotEncryptionFlagSuccess(const QByteArray &folderId);
    void slotEncryptionFlagError(const QByteArray &folderId, const int httpReturnCode, const QString &errorMessage);
    void slotUploadMetadataFinished(int statusCode, const QString &message);
    void slotSetEncryptionFlag();

private:
    AccountPtr _account;
    SyncJournalDb *_journal;
    QString _path;
    QString _pathNonEncrypted;
    QByteArray _fileId;
    QString _errorString;
    OwncloudPropagator *_propagator = nullptr;
    SyncFileItemPtr _item;
    QScopedPointer<EncryptedFolderMetadataHandler> _encryptedFolderMetadataHandler;
};
}
