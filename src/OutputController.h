#pragma once

#include <QtWidgets>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>
#include <QtMultimedia/QVideoFrame>
#include <array>
#include <algorithm>

class ProjectionWindow final : public QWidget {
public:
    explicit ProjectionWindow(int channel) : channel_(channel) {
        setWindowTitle(QString("Projection Output %1").arg(channel + 1));
        setWindowFlag(Qt::FramelessWindowHint);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setFocusPolicy(Qt::StrongFocus);
    }
    void setFrame(const QImage& frame) { frame_ = frame; update(); }
    void setTestMode(bool test) { test_ = test; update(); }
    void setOverlapPercent(int percent) { overlap_ = std::clamp(percent,0,40); update(); }
protected:
    void keyPressEvent(QKeyEvent* event) override {
        if(event->key()==Qt::Key_Escape) { hide(); event->accept(); }
        else QWidget::keyPressEvent(event);
    }
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(),Qt::black);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        if(test_ || frame_.isNull()) {
            const int cell = std::max(24,width()/16);
            for(int x=0;x<width();x+=cell)
                for(int y=0;y<height();y+=cell)
                    p.fillRect(QRect(x,y,cell,cell),((x/cell+y/cell)%2) ? QColor("#17243b") : QColor("#263d53"));
            p.setPen(QPen(QColor("#00e5ff"),3));
            p.drawRect(rect().adjusted(8,8,-9,-9));
            p.setPen(Qt::white);
            QFont f=p.font(); f.setPointSize(24); f.setBold(true); p.setFont(f);
            p.drawText(rect(),Qt::AlignCenter,QString("PROJECTOR %1 / TEST GRID").arg(channel_+1));
            return;
        }
        const double half = .5;
        const double overlap = overlap_ / 200.0;
        const double left = channel_==0 ? 0.0 : half-overlap;
        const double right = channel_==0 ? half+overlap : 1.0;
        QRectF crop(left*frame_.width(),0,(right-left)*frame_.width(),frame_.height());
        // Simple single-timeline split. Pixel-perfect warping and edge blending are future stages.
        p.drawImage(QRectF(rect()),frame_,crop);
    }
private:
    int channel_;
    bool test_=true;
    int overlap_=15;
    QImage frame_;
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
            if(status==QMediaPlayer::EndOfMedia && loop_) player_.setPosition(0),player_.play();
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
        for(int i=0;i<2;++i) outputs_[i]->hide();
        for(int i=0;i<2;++i) {
            QScreen* screen=list[i==0?first:second];
            outputs_[i]->setScreen(screen);
            outputs_[i]->showFullScreen();
            outputs_[i]->raise();
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
    void play() { for(auto* output:outputs_) output->setTestMode(false); player_.play(); }
    void pause() { player_.pause(); }
    void stop() { player_.stop(); }
    void setLoop(bool enabled) { loop_=enabled; }
    void setOverlap(int percent) { for(auto* output:outputs_) output->setOverlapPercent(percent); }
private:
    std::array<ProjectionWindow*,2> outputs_{};
    QMediaPlayer player_;
    QVideoSink sink_;
    bool loop_=true;
};
