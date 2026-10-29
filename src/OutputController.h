#pragma once

#include <QtWidgets>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>
#include <QtMultimedia/QVideoFrame>
#include <array>
#include <algorithm>
#include <cmath>

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
    void applyBlendMask(QImage& image) const {
        const double sharedFraction=overlap_<=0 ? 0.0 : double(overlap_) / (50.0+overlap_/2.0);
        const int rampPixels=std::min(image.width(),int(std::round(sharedFraction*image.width())));
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
                const int alpha=int(std::round(255.0*std::pow(contribution,1.0/2.2)));
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
    bool test_=true;
    QPolygonF quad_;
    QImage frame_;
    QImage grid_;
};

class OutputController final : public QObject {
public:
    OutputController() {
        for(int i=0;i<2;++i) outputs_[i]=new ProjectionWindow(i);
        player_.setVideoSink(&sink_);
        connect(&sink_,&QVideoSink::videoFrameChanged,this,[this](const QVideoFrame& frame){
            if(!frame.isValid()) return;
            const QImage image=frame.toImage();
            if(image.isNull()) return;
            for(auto* output:outputs_) output->setFrame(image);
        });
        connect(&player_,&QMediaPlayer::mediaStatusChanged,this,[this](QMediaPlayer::MediaStatus status){
            if(status==QMediaPlayer::EndOfMedia && loop_) {
                player_.setPosition(0);
                player_.play();
            }
        });
    }
    ~OutputController() override {
        player_.stop();
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
    bool showOutputs(int first,int second,QString& error) {
        auto list=screens();
        if(first<0||second<0||first>=list.size()||second>=list.size()||first==second) {
            error="Select two different, connected displays."; return false;
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
    void stopOutputs() { for(auto* output:outputs_) output->hide(); }
    void testPattern() { for(auto* output:outputs_) output->setTestMode(true); }
    void playFile(const QString& path) {
        for(auto* output:outputs_) output->setTestMode(false);
        player_.setSource(QUrl::fromLocalFile(path));
        player_.play();
    }
    void pause() { player_.pause(); }
    void stop() { player_.stop(); }
    void setLoop(bool enabled) { loop_=enabled; }
    void setOverlap(int percent) { for(auto* output:outputs_) output->setOverlapPercent(percent); }
    void setBrightness(int percent) { for(auto* output:outputs_) output->setBrightnessPercent(percent); }
    void setWarpQuad(int channel,const QPolygonF& quad) {
        if(channel>=0 && channel<2) outputs_[channel]->setWarpQuad(quad);
    }
private:
    std::array<ProjectionWindow*,2> outputs_{};
    QMediaPlayer player_;
    QVideoSink sink_;
    bool loop_=true;
};
