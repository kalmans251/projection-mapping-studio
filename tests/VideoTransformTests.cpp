#include "../src/media/VideoTransform.h"
#include <QColor>
#include <QImage>
#include <iostream>

int main() {
    QImage source(108,192,QImage::Format_RGB32);
    source.fill(Qt::white);
    const QSize canvas(320,90);

    const auto stretch=makeProjectionCanvas(source,{0,0},canvas);
    if(stretch.size()!=canvas || stretch.pixelColor(0,0)!=QColor(Qt::white)) {
        std::cerr << "legacy stretch failed\n"; return 1;
    }
    const auto fitted=makeProjectionCanvas(source,{90,1},canvas);
    if(fitted.size()!=canvas || fitted.pixelColor(0,45)!=QColor(Qt::black) ||
       fitted.pixelColor(160,45)!=QColor(Qt::white)) {
        std::cerr << "rotated fit/letterbox failed\n"; return 2;
    }
    const auto filled=makeProjectionCanvas(source,{270,2},canvas);
    if(filled.pixelColor(0,45)!=QColor(Qt::white) ||
       filled.pixelColor(319,45)!=QColor(Qt::white)) {
        std::cerr << "rotated fill failed\n"; return 3;
    }
    return 0;
}
