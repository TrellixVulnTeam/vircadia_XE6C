//
//  Snapshot.cpp
//  interface/src/ui
//
//  Created by Stojce Slavkovski on 1/26/14.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTemporaryFile>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonArray>
#include <QtNetwork/QHttpMultiPart>
#include <QtGui/QImage>

#include <AccountManager.h>
#include <AddressManager.h>
#include <avatar/AvatarManager.h>
#include <avatar/MyAvatar.h>
#include <shared/FileUtils.h>
#include <NodeList.h>
#include <OffscreenUi.h>
#include <SharedUtil.h>
#include <SecondaryCamera.h>
#include <plugins/DisplayPlugin.h>

#include "Application.h"
#include "scripting/WindowScriptingInterface.h"
#include "MainWindow.h"
#include "Snapshot.h"
#include "SnapshotUploader.h"

// filename format: hifi-snap-by-%username%-on-%date%_%time%_@-%location%.jpg
// %1 <= username, %2 <= date and time, %3 <= current location
const QString FILENAME_PATH_FORMAT = "hifi-snap-by-%1-on-%2.jpg";

const QString DATETIME_FORMAT = "yyyy-MM-dd_hh-mm-ss";
const QString SNAPSHOTS_DIRECTORY = "Snapshots";

const QString URL = "highfidelity_url";

Setting::Handle<QString> Snapshot::snapshotsLocation("snapshotsLocation");

SnapshotMetaData* Snapshot::parseSnapshotData(QString snapshotPath) {

    if (!QFile(snapshotPath).exists()) {
        return NULL;
    }

    QUrl url;

    if (snapshotPath.right(3) == "jpg") {
        QImage shot(snapshotPath);

        // no location data stored
        if (shot.text(URL).isEmpty()) {
            return NULL;
        }

        // parsing URL
        url = QUrl(shot.text(URL), QUrl::ParsingMode::StrictMode);
    } else {
        return NULL;
    }

    SnapshotMetaData* data = new SnapshotMetaData();
    data->setURL(url);

    return data;
}

QString Snapshot::saveSnapshot(QImage image, const QString& filename) {

    QFile* snapshotFile = savedFileForSnapshot(image, false, filename);

    // we don't need the snapshot file, so close it, grab its filename and delete it
    snapshotFile->close();

    QString snapshotPath = QFileInfo(*snapshotFile).absoluteFilePath();

    delete snapshotFile;

    return snapshotPath;
}

void Snapshot::save360Snapshot(const QString& filename) {
    SecondaryCameraJobConfig* secondaryCameraRenderConfig = static_cast<SecondaryCameraJobConfig*>(qApp->getRenderEngine()->getConfiguration()->getConfig("SecondaryCamera"));

    // Save initial values of secondary camera render config
    auto oldAttachedEntityId = secondaryCameraRenderConfig->property("attachedEntityId");
    auto oldOrientation = secondaryCameraRenderConfig->property("orientation");
    auto oldvFoV = secondaryCameraRenderConfig->property("vFoV");
    auto oldNearClipPlaneDistance = secondaryCameraRenderConfig->property("nearClipPlaneDistance");
    auto oldFarClipPlaneDistance = secondaryCameraRenderConfig->property("farClipPlaneDistance");

    // Initialize some secondary camera render config options for 360 snapshot capture
    secondaryCameraRenderConfig->resetSizeSpectatorCamera(2048, 2048);
    secondaryCameraRenderConfig->setProperty("attachedEntityId", "");
    secondaryCameraRenderConfig->setProperty("vFoV", 90.0f);
    secondaryCameraRenderConfig->setProperty("nearClipPlaneDistance", 0.5f);
    secondaryCameraRenderConfig->setProperty("farClipPlaneDistance", 1000.0f);

    secondaryCameraRenderConfig->setOrientation(glm::quat(glm::radians(glm::vec3(-90.0f, 0.0f, 0.0f))));

    qint16 snapshotIndex = 0;

    QTimer* snapshotTimer = new QTimer();
    snapshotTimer->setSingleShot(false);
    snapshotTimer->setInterval(200);
    connect(snapshotTimer, &QTimer::timeout, [&] {
        SecondaryCameraJobConfig* config = static_cast<SecondaryCameraJobConfig*>(qApp->getRenderEngine()->getConfiguration()->getConfig("SecondaryCamera"));
        qDebug() << "ZRF HERE" << snapshotIndex;
        if (snapshotIndex == 0) {
            QImage downImage = qApp->getActiveDisplayPlugin()->getSecondaryCameraScreenshot();
            Snapshot::saveSnapshot(downImage, "down");
            config->setOrientation(glm::quat(glm::radians(glm::vec3(0.0f, 0.0f, 0.0f))));
        } else if (snapshotIndex == 1) {
            QImage frontImage = qApp->getActiveDisplayPlugin()->getSecondaryCameraScreenshot();
            Snapshot::saveSnapshot(frontImage, "front");
            config->setOrientation(glm::quat(glm::radians(glm::vec3(0.0f, 90.0f, 0.0f))));
        } else if (snapshotIndex == 2) {
            QImage leftImage = qApp->getActiveDisplayPlugin()->getSecondaryCameraScreenshot();
            Snapshot::saveSnapshot(leftImage, "left");
            config->setOrientation(glm::quat(glm::radians(glm::vec3(0.0f, 180.0f, 0.0f))));
        } else if (snapshotIndex == 3) {
            QImage backImage = qApp->getActiveDisplayPlugin()->getSecondaryCameraScreenshot();
            Snapshot::saveSnapshot(backImage, "back");
            config->setOrientation(glm::quat(glm::radians(glm::vec3(0.0f, 270.0f, 0.0f))));
        } else if (snapshotIndex == 4) {
            QImage rightImage = qApp->getActiveDisplayPlugin()->getSecondaryCameraScreenshot();
            Snapshot::saveSnapshot(rightImage, "right");
            config->setOrientation(glm::quat(glm::radians(glm::vec3(90.0f, 0.0f, 0.0f))));
        } else if (snapshotIndex == 5) {
            QImage upImage = qApp->getActiveDisplayPlugin()->getSecondaryCameraScreenshot();
            Snapshot::saveSnapshot(upImage, "up");
        } else if (snapshotIndex == 6) {
            // Reset secondary camera render config
            config->resetSizeSpectatorCamera(qApp->getWindow()->geometry().width(), qApp->getWindow()->geometry().height());
            config->setProperty("attachedEntityId", oldAttachedEntityId);
            config->setProperty("vFoV", oldvFoV);
            config->setProperty("nearClipPlaneDistance", oldNearClipPlaneDistance);
            config->setProperty("farClipPlaneDistance", oldFarClipPlaneDistance);

            QFile* snapshotFile = savedFileForSnapshot(qApp->getActiveDisplayPlugin()->getSecondaryCameraScreenshot(), false, filename);

            // we don't need the snapshot file, so close it, grab its filename and delete it
            snapshotFile->close();

            QString snapshotPath = QFileInfo(*snapshotFile).absoluteFilePath();

            delete snapshotFile;

            snapshotTimer->stop();
            snapshotTimer->deleteLater();

            emit DependencyManager::get<WindowScriptingInterface>()->stillSnapshotTaken(snapshotPath, true);
        }

        snapshotIndex++;
    });
    snapshotTimer->start();
}

QTemporaryFile* Snapshot::saveTempSnapshot(QImage image) {
    // return whatever we get back from saved file for snapshot
    return static_cast<QTemporaryFile*>(savedFileForSnapshot(image, true));
}

QFile* Snapshot::savedFileForSnapshot(QImage & shot, bool isTemporary, const QString& userSelectedFilename) {

    // adding URL to snapshot
    QUrl currentURL = DependencyManager::get<AddressManager>()->currentPublicAddress();
    shot.setText(URL, currentURL.toString());

    QString username = DependencyManager::get<AccountManager>()->getAccountInfo().getUsername();
    // normalize username, replace all non alphanumeric with '-'
    username.replace(QRegExp("[^A-Za-z0-9_]"), "-");

    QDateTime now = QDateTime::currentDateTime();

    // If user has requested specific filename then use it, else create the filename
	// 'jpg" is appended, as the image is saved in jpg format.  This is the case for all snapshots
	//       (see definition of FILENAME_PATH_FORMAT)
    QString filename;
    if (!userSelectedFilename.isNull()) {
        filename = userSelectedFilename + ".jpg";
    } else {
        filename = FILENAME_PATH_FORMAT.arg(username, now.toString(DATETIME_FORMAT));
    }

    const int IMAGE_QUALITY = 100;

    if (!isTemporary) {
        QString snapshotFullPath = snapshotsLocation.get();

        if (snapshotFullPath.isEmpty()) {
            snapshotFullPath = OffscreenUi::getExistingDirectory(nullptr, "Choose Snapshots Directory", QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
            snapshotsLocation.set(snapshotFullPath);
        }

        if (!snapshotFullPath.isEmpty()) { // not cancelled

            if (!snapshotFullPath.endsWith(QDir::separator())) {
                snapshotFullPath.append(QDir::separator());
            }

            snapshotFullPath.append(filename);

            QFile* imageFile = new QFile(snapshotFullPath);
            imageFile->open(QIODevice::WriteOnly);

            shot.save(imageFile, 0, IMAGE_QUALITY);
            imageFile->close();

            return imageFile;
        }

    }
    // Either we were asked for a tempororary, or the user didn't set a directory.
    QTemporaryFile* imageTempFile = new QTemporaryFile(QDir::tempPath() + "/XXXXXX-" + filename);

    if (!imageTempFile->open()) {
        qDebug() << "Unable to open QTemporaryFile for temp snapshot. Will not save.";
        return NULL;
    }
    imageTempFile->setAutoRemove(isTemporary);

    shot.save(imageTempFile, 0, IMAGE_QUALITY);
    imageTempFile->close();

    return imageTempFile;
}

void Snapshot::uploadSnapshot(const QString& filename, const QUrl& href) {

    const QString SNAPSHOT_UPLOAD_URL = "/api/v1/snapshots";
    QUrl url = href;
    if (url.isEmpty()) {
        SnapshotMetaData* snapshotData = Snapshot::parseSnapshotData(filename);
        if (snapshotData) {
            url = snapshotData->getURL();
        }
        delete snapshotData;
    }
    if (url.isEmpty()) {
        url = QUrl(DependencyManager::get<AddressManager>()->currentShareableAddress());
    }
    SnapshotUploader* uploader = new SnapshotUploader(url, filename);
    
    QFile* file = new QFile(filename);
    Q_ASSERT(file->exists());
    file->open(QIODevice::ReadOnly);

    QHttpPart imagePart;
    if (filename.right(3) == "gif") {
        imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/gif"));
    } else {
        imagePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/jpeg"));
    }
    imagePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                        QVariant("form-data; name=\"image\"; filename=\"" + file->fileName() + "\""));
    imagePart.setBodyDevice(file);
    
    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    file->setParent(multiPart); // we cannot delete the file now, so delete it with the multiPart
    multiPart->append(imagePart);
    
    auto accountManager = DependencyManager::get<AccountManager>();
    JSONCallbackParameters callbackParams(uploader, "uploadSuccess", uploader, "uploadFailure");

    accountManager->sendRequest(SNAPSHOT_UPLOAD_URL,
                                AccountManagerAuth::Required,
                                QNetworkAccessManager::PostOperation,
                                callbackParams,
                                nullptr,
                                multiPart);
}

QString Snapshot::getSnapshotsLocation() {
    return snapshotsLocation.get("");
}

void Snapshot::setSnapshotsLocation(const QString& location) {
    snapshotsLocation.set(location);
}
