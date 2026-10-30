#include <QCoreApplication>
#include <QJsonDocument>

#include "../src/PlaylistModel.h"
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    QVector<PlaylistEntry> items {
        {"C:/videos/waves.mp4",{0,2}},
        {"C:/videos/fish.mp4",{90,1},20},
        {"C:/videos/night.mp4",{270,0}}
    };
    const auto json=playlistToJson(items);
    QVector<PlaylistEntry> read;
    if (!(playlistFromJson(json,read))) return 1;
    if (!(read.size()==3)) return 1;
    if (!(read[1].transform.rotationDegrees==90)) return 1;
    if (!(read[1].transform.scaleMode==1)) return 1;
    if (!(read[1].durationSeconds==20)) return 1;
    if (!(read[2].path==items[2].path)) return 1;
    if (!(nextPlaylistIndex(0,3,true)==1)) return 1;
    if (!(nextPlaylistIndex(2,3,true)==0)) return 1;
    if (!(nextPlaylistIndex(2,3,false)==-1)) return 1;
    if (!(nextPlaylistIndex(-1,3,true)==-1)) return 1;
    QJsonArray bad=json;
    QJsonObject entry=bad[1].toObject();
    entry["rotation"]=45;
    bad[1]=entry;
    if (!(!playlistFromJson(bad,read))) return 1;
    if (!(read.size()==3)) return 1;
    entry["rotation"]=90;
    entry["durationSeconds"]=-5;
    bad[1]=entry;
    if (playlistFromJson(bad,read)) return 1;
    return 0;
}
