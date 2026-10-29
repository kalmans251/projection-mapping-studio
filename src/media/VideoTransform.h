#pragma once
#include <QImage>
#include <QPainter>
#include <QTransform>
#include <algorithm>

struct VideoTransformOptions {
    // Clockwise rotation: 0, 90, 180, 270 degrees.
    int rotationDegrees=0;
    // 0: legacy stretch; 1: contain (letterbox); 2: cover (crop).
    int scaleMode=0;
};

inline QImage makeProjectionCanvas(const QImage& source,
                                   const VideoTransformOptions& options,
                                   QSize canvasSize=QSize(2560,720)) {
    if(source.isNull() || !canvasSize.isValid()) return {};
    QImage oriented=source;
    if(options.rotationDegrees==90 || options.rotationDegrees==180 ||
       options.rotationDegrees==270) {
        QTransform rotation;
        rotation.rotate(options.rotationDegrees);
        oriented=source.transformed(rotation,Qt::SmoothTransformation);
    }
    QImage canvas(canvasSize,QImage::Format_RGB32);
    canvas.fill(Qt::black);
    const double targetW=canvas.width(), targetH=canvas.height();
    const double sourceW=oriented.width(), sourceH=oriented.height();
    if(sourceW<=0 || sourceH<=0) return canvas;
    QRectF target(0,0,targetW,targetH);
    if(options.scaleMode==1 || options.scaleMode==2) {
        const double widthScale=targetW/sourceW, heightScale=targetH/sourceH;
        const double scale=options.scaleMode==1
            ? (std::min)(widthScale,heightScale)
            : (std::max)(widthScale,heightScale);
        const double w=sourceW*scale, h=sourceH*scale;
        target=QRectF((targetW-w)*.5,(targetH-h)*.5,w,h);
    }
    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target,oriented);
    return canvas;
}
