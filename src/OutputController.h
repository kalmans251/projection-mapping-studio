#pragma once

#include <QtWidgets>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>
#include <QtMultimedia/QVideoFrame>
#include <array>
#include "rendering/D3D11Engine.h"
#include "rendering/D3D11Presenter.h"
#include "media/VideoTransform.h"
#include <algorithm>
#include <cmath>
#include <functional>

// Early CPU/Qt renderer. The two outputs share decoded frames, but hardware scanout is not genlocked.
class ProjectionWindow final : public QWidget {
public:
    explicit ProjectionWindow(int channel) : channel_(channel) {
        setWindowTitle(QString("Projection Output %1").arg(channel + 1));
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setFocusPolicy(Qt::StrongFocus);
        quad_ << QPointF(.02,.02) << QPointF(.98,.02)
              << QPointF(.98,.98) << QPointF(.02,.98);
        buildGrid();
    }
    void setFrame(const QImage& frame) { frame_ = frame; if(!test_) update(); }
    void setTestMode(bool test) { test_ = test; update(); }
    void setOverlapPercent(int percent) { overlap_ = std::clamp(percent,0,40); update(); }
    void setBrightnessPercent(int percent) { brightness_ = std::clamp(percent,0,100); update(); }
    void setBlendGamma(double gamma) { gamma_ = std::clamp(gamma,1.0,3.5); update(); }
    void setRgbGain(int r,int g,int b) {
        gains_={std::clamp(r,0,100),std::clamp(g,0,100),std::clamp(b,0,100)};
        update();
    }
    void setWarpQuad(const QPolygonF& quad) {
        if(quad.size()!=4) return;
        quad_ = quad;
        update();
    }
protected:
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Escape) { hide(); event->accept(); }
        else QWidget::keyPressEvent(event);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.fillRect(rect(),Qt::black);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const QImage& source = test_ || frame_.isNull() ? grid_ : frame_;
        if(source.isNull()) return;

        // Both channels sample the same source time. The shared source stripe must
        // appear in both images for an optically blended overlap.
        const double overlapHalf = overlap_ / 200.0;
        const double sourceLeft = channel_==0 ? 0.0 : .5-overlapHalf;
        const double sourceRight = channel_==0 ? .5+overlapHalf : 1.0;
        const int x0 = std::clamp(int(std::floor(sourceLeft*source.width())),0,source.width()-1);
        const int x1 = std::clamp(int(std::ceil(sourceRight*source.width())),x0+1,source.width());
        QImage segment = source.copy(x0,0,x1-x0,source.height()).convertToFormat(QImage::Format_ARGB32_Premultiplied);
        applyColorGain(segment);
        applyBlendMask(segment);

        QPolygonF destination;
        for(const QPointF& point:quad_)
            destination << QPointF(point.x()*width(),point.y()*height());
        QPolygonF sourceCorners;
        sourceCorners << QPointF(0,0) << QPointF(segment.width(),0)
                      << QPointF(segment.width(),segment.height()) << QPointF(0,segment.height());
        QTransform warp;
        if(!QTransform::quadToQuad(sourceCorners,destination,warp)) return;
        painter.setTransform(warp);
        painter.drawImage(QPointF(0,0),segment);
    }
private:
    void applyColorGain(QImage& image) const {
        if(gains_[0]==100 && gains_[1]==100 && gains_[2]==100) return;
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        painter.fillRect(image.rect(), QColor(
            int(std::round(gains_[0]*2.55)),
            int(std::round(gains_[1]*2.55)),
            int(std::round(gains_[2]*2.55))));
    }
    void applyBlendMask(QImage& image) const {
        const double sharedFraction=overlap_<=0 ? 0.0 : double(overlap_) / (50.0+overlap_/2.0);
        const int rampPixels=(std::min)(image.width(),int(std::round(sharedFraction*image.width())));
        QPainter layerPainter(&image);
        layerPainter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        if(rampPixels>0) {
            const int samples=32;
            QLinearGradient gradient(channel_==0 ? image.width()-rampPixels : 0,0,
                                     channel_==0 ? image.width() : rampPixels,0);
            for(int i=0;i<=samples;++i) {
                const double t=double(i)/samples;
                // Approximate gamma-aware luminance weighting; projector black levels
                // and color matching require later on-site calibration.
                const double contribution=channel_==0 ? (1.0-t) : t;
                const int alpha=int(std::round(255.0*std::pow(contribution,1.0/gamma_)));
                gradient.setColorAt(t,QColor(255,255,255,alpha));
            }
            layerPainter.fillRect(channel_==0 ? QRect(image.width()-rampPixels,0,rampPixels,image.height())
                                             : QRect(0,0,rampPixels,image.height()),gradient);
        }
        if(brightness_<100) {
            layerPainter.fillRect(image.rect(),QColor(255,255,255,int(std::round(brightness_*2.55))));
        }
    }
    void buildGrid() {
        grid_=QImage(1280,720,QImage::Format_RGB32);
        grid_.fill(QColor("#17243b"));
        QPainter p(&grid_);
        p.setPen(QPen(QColor("#51718b"),2));
        for(int x=0;x<=grid_.width();x+=80) p.drawLine(x,0,x,grid_.height());
        for(int y=0;y<=grid_.height();y+=80) p.drawLine(0,y,grid_.width(),y);
        p.setPen(QPen(QColor("#00e5ff"),8));
        p.drawRect(grid_.rect().adjusted(8,8,-9,-9));
        p.setPen(Qt::white);
        QFont f=p.font(); f.setPointSize(40); f.setBold(true); p.setFont(f);
        p.drawText(grid_.rect(),Qt::AlignCenter,"PROJECTION CALIBRATION GRID");
    }
    int channel_;
    int overlap_=15;
    int brightness_=100;
    double gamma_=2.2;
    std::array<int,3> gains_={100,100,100};
    bool test_=true;
    QPolygonF quad_;
    QImage frame_;
    QImage grid_;
};

class GpuOutputWindow final : public QWindow {
public:
    GpuOutputWindow() { setFlags(Qt::Window | Qt::FramelessWindowHint); }
protected:
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Escape) { hide(); event->accept(); }
        else QWindow::keyPressEvent(event);
    }
};

class OutputController final : public QObject {
public:
    OutputController() {
        for(int i=0;i<2;++i) {
            outputs_[i]=new ProjectionWindow(i);
            gpuWindows_[i]=new GpuOutputWindow;
            gpuSettings_[i].quad << QPointF(.02,.02) << QPointF(.98,.02)
                                 << QPointF(.98,.98) << QPointF(.02,.98);
        }
        gpuGrid_=QImage(1280,720,QImage::Format_RGB32);
        gpuGrid_.fill(QColor("#17243b"));
        {
            QPainter painter(&gpuGrid_);
            painter.setPen(QPen(QColor("#51718b"),2));
            for(int x=0;x<=gpuGrid_.width();x+=80) painter.drawLine(x,0,x,gpuGrid_.height());
            for(int y=0;y<=gpuGrid_.height();y+=80) painter.drawLine(0,y,gpuGrid_.width(),y);
            painter.setPen(QPen(QColor("#00e5ff"),8));
            painter.drawRect(gpuGrid_.rect().adjusted(8,8,-9,-9));
        }
        player_.setVideoSink(&sink_);
        connect(&sink_,&QVideoSink::videoFrameChanged,this,[this](const QVideoFrame& frame){
            if(!frame.isValid()) return;
            const QImage image=frame.toImage();
            if(image.isNull()) return;
            lastDecodedFrame_=image;
            presentDecodedFrame();
        });
        connect(&player_,&QMediaPlayer::mediaStatusChanged,this,[this](QMediaPlayer::MediaStatus status){
            if(status==QMediaPlayer::EndOfMedia) {
                if(onPlaybackFinished_) onPlaybackFinished_();
                else if(loop_) { player_.setPosition(0); player_.play(); }
            }
        });
    }
    ~OutputController() override {
        player_.stop();
        presenter_.detach();
        for(auto* output:gpuWindows_) delete output;
        for(auto* output:outputs_) delete output;
    }
    static QList<QScreen*> screens() { return QGuiApplication::screens(); }
    QStringList screenDescriptions() const {
        QStringList descriptions;
        for(auto* screen:screens()) {
            const auto g=screen->geometry();
            descriptions << QString("%1 — %2x%3 (%4,%5)%6")
                .arg(screen->name()).arg(g.width()).arg(g.height()).arg(g.x()).arg(g.y())
                .arg(screen==QGuiApplication::primaryScreen() ? " [Primary]" : "");
        }
        return descriptions;
    }
    bool setGpuMode(bool enabled,QString& error) {
        if(enabled && !presenter_.initialize(error)) {
            gpuError_=error; return false;
        }
        stopOutputs();
        gpuMode_=enabled;
        return true;
    }
    bool gpuMode() const { return gpuMode_; }
    void renderGpu() {
        if(!gpuMode_) return;
        for(int i=0;i<2;++i) {
            QString error;
            if(!presenter_.render(i,gpuSettings_[i],error) && !error.isEmpty())
                gpuError_=error;
        }
    }
    bool showOutputs(int first,int second,QString& error) {
        auto list=screens();
        if(first<0||second<0||first>=list.size()||second>=list.size()||first==second) {
            error="Select two different, connected displays."; return false;
        }
        if(gpuMode_) {
            for(auto* output:outputs_) output->hide();
            for(auto* window:gpuWindows_) window->hide();
            presenter_.detach();
            for(int i=0;i<2;++i) {
                QScreen* screen=list[i==0?first:second];
                auto* window=gpuWindows_[i];
                window->setScreen(screen);
                window->setGeometry(screen->geometry());
                window->show();
                window->setGeometry(screen->geometry());
                if(!presenter_.attach(i,reinterpret_cast<HWND>(window->winId()),window->size(),error)) {
                    stopOutputs();return false;
                }
            }
            testPattern();
            return true;
        }
        // Windows can choose the primary display for showFullScreen() when the
        // QWidget's native handle was created on another monitor. Instead,
        // explicitly bind its QWindow and use a borderless window sized to the
        // target screen's *logical desktop geometry*.
        for(auto* output:outputs_) output->hide();
        for(int i=0;i<2;++i) {
            QScreen* screen=list[i==0?first:second];
            ProjectionWindow* output=outputs_[i];
            output->setWindowState(Qt::WindowNoState);
            output->winId(); // Ensure a native QWindow exists before selecting the screen.
            if(QWindow* handle=output->windowHandle()) handle->setScreen(screen);
            output->setGeometry(screen->geometry());
            output->show();
            output->setGeometry(screen->geometry()); // Defend against OS window placement.
            output->raise();
            if(output->geometry()!=screen->geometry()) {
                error=QString("Output %1 could not be placed on %2. Check Windows display scaling.")
                          .arg(i+1).arg(screen->name());
                stopOutputs();
                return false;
            }
        }
        return true;
    }
    bool initializeGpu(QString& error) {
        const bool ok=gpuEngine_.initialize(error);
        if(!ok) gpuError_=error;
        return ok;
    }
    void setGpuUploadEnabled(bool enabled) { gpuUploadEnabled_=enabled && gpuEngine_.isReady(); }
    QString gpuStatus() const {
        return gpuError_.isEmpty() ? gpuEngine_.description() : "GPU error: "+gpuError_;
    }
    void stopOutputs() {
        for(auto* output:outputs_) output->hide();
        for(auto* window:gpuWindows_) window->hide();
        presenter_.detach();
    }
    void testPattern() {
        if(gpuMode_) {
            gpuTestMode_=true;
            QString error;
            if(!gpuEngine_.uploadFrame(gpuGrid_,error)) gpuError_=error;
            else renderGpu();
        } else for(auto* output:outputs_) output->setTestMode(true);
    }
    void setPlaybackFinishedCallback(std::function<void()> callback) { onPlaybackFinished_=std::move(callback); }
    void playFile(const QString& path) {
        if(gpuMode_) gpuTestMode_=false;
        else for(auto* output:outputs_) output->setTestMode(false);
        player_.setSource(QUrl::fromLocalFile(path));
        player_.play();
    }
    void setVideoTransform(int rotation,int scaleMode) {
        videoOptions_.rotationDegrees=(rotation==90||rotation==180||rotation==270) ? rotation : 0;
        videoOptions_.scaleMode=std::clamp(scaleMode,0,2);
        presentDecodedFrame();
    }
    void pause() { player_.pause(); }
    void stop() { player_.stop(); }
    void setLoop(bool enabled) { loop_=enabled; }
    void setOverlap(int percent) {
        for(auto* output:outputs_) output->setOverlapPercent(percent);
        for(auto& settings:gpuSettings_) settings.overlap=percent;
        renderGpu();
    }
    void setBrightness(int percent) {
        for(auto* output:outputs_) output->setBrightnessPercent(percent);
        for(auto& settings:gpuSettings_) settings.brightness=percent;
        renderGpu();
    }
    void setBlendGamma(double gamma) {
        for(auto* output:outputs_) output->setBlendGamma(gamma);
        for(auto& settings:gpuSettings_) settings.blendGamma=gamma;
        renderGpu();
    }
    void setRgbGain(int channel,int r,int g,int b) {
        if(channel>=0 && channel<2) {
            outputs_[channel]->setRgbGain(r,g,b);
            gpuSettings_[channel].gains={r,g,b};
            renderGpu();
        }
    }
    void setWarpQuad(int channel,const QPolygonF& quad) {
        if(channel>=0 && channel<2) {
            outputs_[channel]->setWarpQuad(quad);
            gpuSettings_[channel].quad=quad;
            renderGpu();
        }
    }
private:
    void presentDecodedFrame() {
        if(lastDecodedFrame_.isNull()) return;
        QImage canvas=makeProjectionCanvas(lastDecodedFrame_,videoOptions_);
        if(canvas.isNull()) return;
        if(gpuMode_) {
            if(gpuTestMode_) return;
            QString error;
            if(!gpuEngine_.uploadFrame(canvas,error)) gpuError_=error;
            else renderGpu();
        } else {
            if(gpuUploadEnabled_) {
                QString error;
                if(!gpuEngine_.uploadFrame(canvas,error)) {
                    gpuUploadEnabled_=false;
                    gpuError_=error;
                }
            }
            for(auto* output:outputs_) output->setFrame(canvas);
        }
    }
    std::function<void()> onPlaybackFinished_;
    QImage lastDecodedFrame_;
    VideoTransformOptions videoOptions_;
    std::array<ProjectionWindow*,2> outputs_{};
    QMediaPlayer player_;
    QVideoSink sink_;
    bool loop_=true;
    D3D11Engine gpuEngine_;
    D3D11Presenter presenter_{gpuEngine_};
    std::array<GpuOutputWindow*,2> gpuWindows_{};
    std::array<GpuCalibration,2> gpuSettings_{};
    QImage gpuGrid_;
    bool gpuMode_=false;
    bool gpuTestMode_=true;
    bool gpuUploadEnabled_=false;
    QString gpuError_;
};
