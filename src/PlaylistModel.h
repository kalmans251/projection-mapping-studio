#pragma once
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <algorithm>
#include "media/VideoTransform.h"

struct PlaylistEntry {
    QString path;
    VideoTransformOptions transform;
};

inline QJsonArray playlistToJson(const QVector<PlaylistEntry>& entries) {
    QJsonArray result;
    for (const auto& entry : entries) {
        QJsonObject object;
        object["path"] = entry.path;
        object["rotation"] = entry.transform.rotationDegrees;
        object["scaleMode"] = entry.transform.scaleMode;
        result.append(object);
    }
    return result;
}
inline bool playlistFromJson(const QJsonValue& value, QVector<PlaylistEntry>& entries) {
    if (!value.isArray()) return false;
    QVector<PlaylistEntry> parsed;
    const auto array = value.toArray();
    if (array.size() > 10000) return false;
    for (const auto& item : array) {
        if (!item.isObject()) return false;
        const auto object = item.toObject();
        const QString path = object.value("path").toString();
        if (path.isEmpty()) return false;
        const int rotation = object.value("rotation").toInt(0);
        const int mode = object.value("scaleMode").toInt(0);
        if ((rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270)
            || mode < 0 || mode > 2) return false;
        parsed.append({path, {rotation, mode}});
    }
    entries = std::move(parsed);
    return true;
}
inline int nextPlaylistIndex(int current, int count, bool repeat) {
    if (count <= 0 || current < 0 || current >= count) return -1;
    if (current + 1 < count) return current + 1;
    return repeat ? 0 : -1;
}
