#include "propagateremotemoveencrypted.h"
#include "clientsideencryptionjobs.h"
#include "clientsideencryption.h"
#include "owncloudpropagator.h"

#include <QLoggingCategory>
#include <QMimeDatabase>
#include <QFileInfo>
#include <QDir>

using namespace OCC;

Q_LOGGING_CATEGORY(PROPAGATE_REMOVE_ENCRYPTED, "nextcloud.sync.propagator.remove.encrypted")

PropagateRemoteMoveEncrypted::PropagateRemoteMoveEncrypted(OwncloudPropagator *propagator, SyncFileItemPtr item, QObject *parent)
    : QObject(parent)
    , _propagator(propagator)
    , _item(item)
{

}

QByteArray PropagateRemoteMoveEncrypted::folderToken()
{
    return _folderToken;
}

void PropagateRemoteMoveEncrypted::start()
{
    Q_ASSERT(!_item->_encryptedFileName.isEmpty());
    QFileInfo info(_item->_encryptedFileName);
    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Folder is encrypted, let's get the Id from it.";
    auto job = new LsColJob(_propagator->account(), info.path(), this);
    job->setProperties({"resourcetype", "http://owncloud.org/ns:fileid"});
    connect(job, &LsColJob::directoryListingSubfolders, this, &PropagateRemoteMoveEncrypted::slotFolderEncryptedIdReceived);
    connect(job, &LsColJob::finishedWithError, this, &PropagateRemoteMoveEncrypted::taskFailed);
    job->start();
}

void PropagateRemoteMoveEncrypted::slotFolderEncryptedIdReceived(const QStringList &list)
{
    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Received id of folder, trying to lock it so we can prepare the metadata";
    auto job = qobject_cast<LsColJob *>(sender());
    const ExtraFolderInfo folderInfo = job->_folderInfos.value(list.first());
    slotTryLock(folderInfo.fileId);
}

void PropagateRemoteMoveEncrypted::slotTryLock(const QByteArray &folderId)
{
    auto lockJob = new LockEncryptFolderApiJob(_propagator->account(), folderId, this);
    connect(lockJob, &LockEncryptFolderApiJob::success, this, &PropagateRemoteMoveEncrypted::slotFolderLockedSuccessfully);
    connect(lockJob, &LockEncryptFolderApiJob::error, this, &PropagateRemoteMoveEncrypted::taskFailed);
    lockJob->start();
}

void PropagateRemoteMoveEncrypted::slotFolderLockedSuccessfully(const QByteArray &folderId, const QByteArray &token)
{
    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Folder id" << folderId << "Locked Successfully for Upload, Fetching Metadata";
    _folderLocked = true;
    _folderToken = token;
    _folderId = folderId;

    auto job = new GetMetadataApiJob(_propagator->account(), _folderId);
    connect(job, &GetMetadataApiJob::jsonReceived, this, &PropagateRemoteMoveEncrypted::slotFolderEncryptedMetadataReceived);
    connect(job, &GetMetadataApiJob::error, this, &PropagateRemoteMoveEncrypted::taskFailed);
    job->start();
}

void PropagateRemoteMoveEncrypted::slotFolderEncryptedMetadataReceived(const QJsonDocument &json, int statusCode)
{
    if (statusCode == 404) {
        qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Metadata not found, ignoring.";
        emit finished(true);
        return;
    }

    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Metadata Received, Preparing it for the new file.";

    // Encrypt File!
    FolderMetadata metadata(_propagator->account(), json.toJson(QJsonDocument::Compact), statusCode);

    QFileInfo info(_propagator->fullLocalPath(_item->_file));
    const QString fileName = info.fileName();

    // Find existing metadata for this file
    bool found = false;
    const QVector<EncryptedFile> files = metadata.files();
    for (const EncryptedFile &file : files) {
        if (file.originalFilename == fileName) {
            metadata.removeEncryptedFile(file);
            found = true;
            break;
        }
    }

    if (!found) {
        // The removed file was not in the JSON so nothing else to do
        emit finished(true);
        return;
    }

    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Metadata updated, sending to the server.";

    auto job = new UpdateMetadataApiJob(_propagator->account(),
                                        _folderId,
                                        metadata.encryptedMetadata(),
                                        _folderToken);

    connect(job, &UpdateMetadataApiJob::success, this, [this] { emit finished(true); });
    connect(job, &UpdateMetadataApiJob::error, this, &PropagateRemoteMoveEncrypted::taskFailed);
    job->start();
}

void PropagateRemoteMoveEncrypted::unlockFolder()
{
    if (!_folderLocked) {
        emit folderUnlocked();
        return;
    }

    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Unlocking folder" << _folderId;
    auto unlockJob = new UnlockEncryptFolderApiJob(_propagator->account(),
                                                   _folderId, _folderToken, this);

    connect(unlockJob, &UnlockEncryptFolderApiJob::success, [this] {
        qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Folder successfully unlocked" << _folderId;
        _folderLocked = false;
        emit folderUnlocked();
    });
    connect(unlockJob, &UnlockEncryptFolderApiJob::error, this, &PropagateRemoteMoveEncrypted::taskFailed);
    unlockJob->start();
}

void PropagateRemoteMoveEncrypted::taskFailed()
{
    qCDebug(PROPAGATE_REMOVE_ENCRYPTED) << "Task failed of job" << sender();
    if (_folderLocked) {
        connect(this, &PropagateRemoteMoveEncrypted::folderUnlocked, this, [this] { emit finished(false); });
        unlockFolder();
    } else {
        emit finished(false);
    }
}