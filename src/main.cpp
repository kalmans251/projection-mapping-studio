#include <QtWidgets>
#include <array>
#include <cmath>
#include <algorithm>
#include <functional>
#include "OutputController.h"

struct MappingSettings {
    std::array<QPolygonF, 2> quads;
    int blend = 15;
    int brightness = 100;
    int blendGammaTenths = 22;
    std::array<std::array<int,3>,2> rgbGains={{{100,100,100},{100,100,100}}};

    MappingSettings() {
        quads[0] << QPointF(.02,.02) << QPointF(.98,.02) << QPointF(.98,.98) << QPointF(.02,.98);
        quads[1] << QPointF(.02,.02) << QPointF(.98,.02) << QPointF(.98,.98) << QPointF(.02,.98);
    }
};

class MappingPreview final : public QWidget {
public:
    explicit MappingPreview(MappingSettings& settings, QWidget* parent = nullptr)
        : QWidget(parent), settings_(settings) {
        setMinimumSize(550, 285);
        setMouseTracking(true);
        setCursor(Qt::CrossCursor);
    }
    void setSelectedProjector(int index) { selected_ = index; update(); }
    void setChangedCallback(std::function<void(int,const QPolygonF&)> callback) { onChanged_ = std::move(callback); }
private:
    MappingSettings& settings_;
    int selected_ = 0;
    int dragging_ = -1;
    std::function<void(int,const QPolygonF&)> onChanged_;

    QRectF area() const { return rect().adjusted(28, 28, -28, -28); }
    QPointF screenPoint(const QPointF& p) const {
        auto a = area();
        return {a.left() + p.x()*a.width(), a.top() + p.y()*a.height()};
    }
    QPointF normalizedPoint(const QPointF& p) const {
        auto a = area();
        return {std::clamp((p.x()-a.left())/a.width(), 0.0, 1.0),
                std::clamp((p.y()-a.top())/a.height(), 0.0, 1.0)};
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#111827"));
        auto a = area();
        painter.setPen(QPen(QColor("#334155"), 1));
        for(int i = 0; i <= 20; ++i) {
            const auto x = a.left() + i*a.width()/20.0;
            painter.drawLine(QPointF(x,a.top()), QPointF(x,a.bottom()));
        }
        for(int i = 0; i <= 10; ++i) {
            const auto y = a.top() + i*a.height()/10.0;
            painter.drawLine(QPointF(a.left(),y), QPointF(a.right(),y));
        }
        const QColor colors[2] = {QColor(6,182,212,105), QColor(99,102,241,105)};
        for(int n = 0; n < 2; ++n) {
            QPolygonF polygon;
            for(const auto& point : settings_.quads[n]) polygon << screenPoint(point);
            painter.setBrush(colors[n]);
            painter.setPen(QPen(n == selected_ ? QColor("#ffffff") : QColor("#94a3b8"), n == selected_ ? 2.0 : 1.0));
            painter.drawPolygon(polygon);
            painter.setPen(Qt::white);
            painter.drawText(polygon.boundingRect(), Qt::AlignCenter, QString("Projector %1").arg(n+1));
            if(n == selected_) {
                for(int i = 0; i < polygon.size(); ++i) {
                    painter.setBrush(Qt::white);
                    painter.setPen(QPen(QColor("#0f172a"),2));
                    painter.drawEllipse(polygon[i],6,6);
                }
            }
        }
        painter.setPen(QColor("#94a3b8"));
        painter.drawText(12,17, "SIMULATION / NOT HDMI OUTPUT");
    }
    void mousePressEvent(QMouseEvent* event) override {
        if(event->button() != Qt::LeftButton) return;
        for(int i = 0; i < 4; ++i) {
            if(QLineF(event->position(), screenPoint(settings_.quads[selected_][i])).length() < 15.0) {
                dragging_ = i;
                return;
            }
        }
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if(dragging_ < 0) return;
        settings_.quads[selected_][dragging_] = normalizedPoint(event->position());
        if(onChanged_) onChanged_(selected_,settings_.quads[selected_]);
        update();
    }
    void mouseReleaseEvent(QMouseEvent*) override { dragging_ = -1; }
};

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("Projection Mapping Studio — v0.6");
        resize(1040, 660);
        auto* central = new QWidget;
        auto* root = new QVBoxLayout(central);
        root->setContentsMargins(20,20,20,20);
        root->setSpacing(14);

        auto* title = new QLabel("Projection Mapping Studio");
        QFont titleFont = title->font();
        titleFont.setPointSize(18);
        titleFont.setBold(true);
        title->setFont(titleFont);
        root->addWidget(title);
        auto* notice = new QLabel("Live 4-point warp + overlap brightness fade (CPU preview). Physical alignment and gamma calibration required.");
        notice->setStyleSheet("color:#9ca3af");
        root->addWidget(notice);

        auto* tabs = new QTabWidget;
        root->addWidget(tabs,1);
        auto* calibration = new QWidget;
        auto* calibrationLayout = new QHBoxLayout(calibration);
        preview_ = new MappingPreview(settings_);
        preview_->setChangedCallback([this](int channel,const QPolygonF& quad){output_.setWarpQuad(channel,quad);});
        calibrationLayout->addWidget(preview_, 3);
        auto* controls = new QWidget;
        controls->setMinimumWidth(240);
        auto* side = new QVBoxLayout(controls);
        auto* target = new QComboBox;
        target->addItems({"Projector 1", "Projector 2"});
        side->addWidget(new QLabel("Editing projector"));
        side->addWidget(target);
        connect(target, QOverload<int>::of(&QComboBox::currentIndexChanged),
                preview_, &MappingPreview::setSelectedProjector);

        auto addSlider = [side](const QString& name, int maximum, int value, auto callback) {
            auto* label = new QLabel(QString("%1: %2%").arg(name).arg(value));
            auto* slider = new QSlider(Qt::Horizontal);
            slider->setRange(0, maximum);
            slider->setValue(value);
            side->addSpacing(18);
            side->addWidget(label);
            side->addWidget(slider);
            QObject::connect(slider, &QSlider::valueChanged,
                [label, name, callback](int v) { label->setText(QString("%1: %2%").arg(name).arg(v)); callback(v); });
            return slider;
        };
        blendSlider_ = addSlider("Overlap width (split preview)",40,settings_.blend,[this](int v){settings_.blend=v; output_.setOverlap(v);});
        brightnessSlider_ = addSlider("Output brightness",100,settings_.brightness,[this](int v){settings_.brightness=v; output_.setBrightness(v);});
        auto* reset = new QPushButton("Reset calibration");
        side->addSpacing(12);
        side->addWidget(reset);
        connect(reset,&QPushButton::clicked,this,[this]{
            settings_.quads = MappingSettings{}.quads;
            for(int i=0;i<2;++i) output_.setWarpQuad(i,settings_.quads[i]);
            preview_->update();
        });
        side->addStretch();
        calibrationLayout->addWidget(controls,1);
        tabs->addTab(calibration,"Mapping calibration");

        auto* projectors = new QWidget;
        auto* projectionLayout = new QVBoxLayout(projectors);
        projectionLayout->addWidget(new QLabel("Choose two different extended displays. Never select your management monitor unless intended."));
        auto* refresh = new QPushButton("Refresh connected displays");
        firstScreen_ = new QComboBox;
        secondScreen_ = new QComboBox;
        projectionLayout->addWidget(refresh);
        projectionLayout->addWidget(new QLabel("Projector 1 display"));
        projectionLayout->addWidget(firstScreen_);
        projectionLayout->addWidget(new QLabel("Projector 2 display"));
        projectionLayout->addWidget(secondScreen_);
        auto* showTest = new QPushButton("Open warped test patterns on selected displays");
        auto* closeOutput = new QPushButton("Close projector windows");
        projectionLayout->addWidget(showTest);
        projectionLayout->addWidget(closeOutput);
        projectionLayout->addWidget(new QLabel("Press Esc inside a projector window to close it. Windows display mode must be Extend."));
        projectionLayout->addStretch();
        tabs->addTab(projectors,"Projectors");

        auto* media = new QWidget;
        auto* mediaLayout = new QVBoxLayout(media);
        selectedFile_ = new QLabel("No video selected");
        selectedFile_->setWordWrap(true);
        auto* choose = new QPushButton("Choose MP4 / video file...");
        auto* play = new QPushButton("Play on both projector outputs");
        auto* pause = new QPushButton("Pause");
        auto* stop = new QPushButton("Stop playback");
        auto* loop = new QCheckBox("Loop video");
        loop->setChecked(true);
        mediaLayout->addWidget(choose);
        mediaLayout->addWidget(selectedFile_);
        mediaLayout->addWidget(play);
        mediaLayout->addWidget(pause);
        mediaLayout->addWidget(stop);
        mediaLayout->addWidget(loop);
        mediaLayout->addWidget(new QLabel("Warp and overlap fade are active. This CPU/Qt prototype is not frame-genlocked or color calibrated."));
        mediaLayout->addStretch();
        tabs->addTab(media,"Media");

        // Each projector can be attenuated independently to match its neighbor.
        auto* colorTab = new QWidget;
        auto* colorLayout = new QVBoxLayout(colorTab);
        colorLayout->addWidget(new QLabel("Blend transition shape and projector RGB gain (attenuation only)."));
        colorLayout->addWidget(new QLabel("Blend gamma: 1.0 = linear alpha; 2.2 = approximate optical compensation."));
        auto* gammaText = new QLabel("Blend gamma: 2.2");
        gammaSlider_ = new QSlider(Qt::Horizontal);
        gammaSlider_->setRange(10,35);
        gammaSlider_->setValue(settings_.blendGammaTenths);
        colorLayout->addWidget(gammaText);
        colorLayout->addWidget(gammaSlider_);
        connect(gammaSlider_,&QSlider::valueChanged,this,[this,gammaText](int tenths){
            settings_.blendGammaTenths=tenths;
            gammaText->setText(QString("Blend gamma: %1").arg(tenths/10.0,0,'f',1));
            output_.setBlendGamma(tenths/10.0);
        });

        colorProjector_ = new QComboBox;
        colorProjector_->addItems({"Projector 1","Projector 2"});
        colorLayout->addWidget(new QLabel("Adjust RGB intensity of selected projector"));
        colorLayout->addWidget(colorProjector_);
        const QString channelLabels[3]={"Red","Green","Blue"};
        for(int c=0;c<3;++c) {
            auto* label=new QLabel(channelLabels[c]+": 100%");
            auto* slider=new QSlider(Qt::Horizontal);
            slider->setRange(0,100);
            slider->setValue(100);
            colorLabels_[c]=label;
            colorSliders_[c]=slider;
            colorLayout->addWidget(label);
            colorLayout->addWidget(slider);
            connect(slider,&QSlider::valueChanged,this,[this,c](int value){
                const int channel=colorProjector_->currentIndex();
                if(channel<0 || channel>1) return;
                settings_.rgbGains[channel][c]=value;
                colorLabels_[c]->setText(QString("%1: %2%").arg(QStringList{"Red","Green","Blue"}[c]).arg(value));
                const auto& gains=settings_.rgbGains[channel];
                output_.setRgbGain(channel,gains[0],gains[1],gains[2]);
            });
        }
        connect(colorProjector_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int channel){
            if(channel<0||channel>1) return;
            for(int c=0;c<3;++c) {
                QSignalBlocker blocker(colorSliders_[c]);
                const int gain=settings_.rgbGains[channel][c];
                colorSliders_[c]->setValue(gain);
                colorLabels_[c]->setText(QString("%1: %2%").arg(QStringList{"Red","Green","Blue"}[c]).arg(gain));
            }
        });
        colorLayout->addWidget(new QLabel("Black-level mismatch cannot be eliminated by dimming. Test with dark content."));
        colorLayout->addStretch();
        tabs->addTab(colorTab,"Color & Blend");

        auto* gpuTab = new QWidget;
        auto* gpuLayout = new QVBoxLayout(gpuTab);
        gpuLayout->addWidget(new QLabel("Direct3D 11 hardware renderer (experimental). CPU/Qt renderer remains the default."));
        auto* initGpu = new QPushButton("Initialize D3D11 hardware GPU");
        auto* uploadToggle = new QCheckBox("Upload decoded video frames to a GPU texture (diagnostic)");
        uploadToggle->setEnabled(false);
        auto* gpuStatus = new QLabel("GPU not initialized");
        gpuStatus->setWordWrap(true);
        gpuLayout->addWidget(initGpu);
        gpuLayout->addWidget(uploadToggle);
        auto* gpuModeToggle = new QCheckBox("Use experimental D3D11 renderer for both projector outputs");
        gpuLayout->addWidget(gpuModeToggle);
        gpuLayout->addWidget(gpuStatus);
        gpuLayout->addWidget(new QLabel("Changing renderer closes the output windows. Re-open test patterns in Projectors. GPU mode is not genlocked."));
        gpuLayout->addStretch();
        tabs->addTab(gpuTab,"GPU renderer");
        connect(initGpu,&QPushButton::clicked,this,[this,uploadToggle,gpuStatus] {
            QString error;
            if(!output_.initializeGpu(error)) {
                QMessageBox::warning(this,"D3D11",error);
                gpuStatus->setText(error);
                return;
            }
            uploadToggle->setEnabled(true);
            gpuStatus->setText(output_.gpuStatus());
        });
        connect(uploadToggle,&QCheckBox::toggled,this,[this](bool enabled){
            output_.setGpuUploadEnabled(enabled);
        });
        connect(gpuModeToggle,&QCheckBox::toggled,this,[this,gpuModeToggle,gpuStatus](bool enabled){
            QString error;
            if(!output_.setGpuMode(enabled,error)) {
                QMessageBox::warning(this,"GPU renderer",error);
                QSignalBlocker blocker(gpuModeToggle);
                gpuModeToggle->setChecked(false);
                gpuStatus->setText(error);
                return;
            }
            gpuStatus->setText(output_.gpuStatus());
            QMessageBox::information(this,"Renderer changed",
                 "Projector windows were closed. Use Projectors > Open warped test patterns to reopen them.");
        });
        auto* gpuTimer = new QTimer(this);
        connect(gpuTimer,&QTimer::timeout,this,[this,gpuStatus]{
            gpuStatus->setText(output_.gpuStatus());
        });
        gpuTimer->start(1000);

        auto refreshScreens = [this]() {
            const auto names=output_.screenDescriptions();
            firstScreen_->clear(); secondScreen_->clear();
            firstScreen_->addItems(names); secondScreen_->addItems(names);
            if(names.size()>1) firstScreen_->setCurrentIndex(1);
            if(names.size()>2) secondScreen_->setCurrentIndex(2);
            else if(!names.isEmpty()) secondScreen_->setCurrentIndex(0);
        };
        connect(refresh,&QPushButton::clicked,this,refreshScreens);
        connect(showTest,&QPushButton::clicked,this,[this] {
            const auto screens=OutputController::screens();
            const int first=firstScreen_->currentIndex(), second=secondScreen_->currentIndex();
            if(first<0 || second<0 || first>=screens.size() || second>=screens.size() || first==second) {
                QMessageBox::warning(this,"Display selection","Select two different connected displays.");
                return;
            }
            if(screens[first]==QGuiApplication::primaryScreen() || screens[second]==QGuiApplication::primaryScreen()) {
                const auto choice=QMessageBox::warning(this,"Primary screen selected",
                    "One of the selected projector outputs is the primary (laptop) display.\n"
                    "This would cover your settings window. Continue anyway?",
                    QMessageBox::Yes|QMessageBox::No,QMessageBox::No);
                if(choice!=QMessageBox::Yes) return;
            }
            QString error;
            if(!output_.showOutputs(first,second,error)) {
                QMessageBox::warning(this,"Display selection",error); return;
            }
            output_.testPattern();
        });
        connect(closeOutput,&QPushButton::clicked,this,[this]{output_.stopOutputs();});
        connect(choose,&QPushButton::clicked,this,[this] {
            const auto path=QFileDialog::getOpenFileName(this,"Choose video",QString(),"Video files (*.mp4 *.mov *.mkv *.avi);;All files (*)");
            if(path.isEmpty()) return;
            videoPath_=path;
            selectedFile_->setText(path);
        });
        connect(play,&QPushButton::clicked,this,[this] {
            if(videoPath_.isEmpty()) { QMessageBox::information(this,"Video","Choose a file first.");return; }
            output_.playFile(videoPath_);
        });
        connect(pause,&QPushButton::clicked,this,[this]{output_.pause();});
        connect(stop,&QPushButton::clicked,this,[this]{output_.stop();output_.testPattern();});
        connect(loop,&QCheckBox::toggled,this,[this](bool enabled){output_.setLoop(enabled);});
        refreshScreens();
        auto* actions = new QHBoxLayout;
        auto* save = new QPushButton("Save settings...");
        auto* load = new QPushButton("Load settings...");
        actions->addStretch();
        actions->addWidget(load);
        actions->addWidget(save);
        root->addLayout(actions);
        connect(save,&QPushButton::clicked,this,&MainWindow::saveSettings);
        connect(load,&QPushButton::clicked,this,&MainWindow::loadSettings);
        setCentralWidget(central);
        setStyleSheet("QMainWindow, QWidget { background:#181c25; color:#e5e7eb; }"
                      "QPushButton, QComboBox { background:#303849; border:1px solid #475569; padding:9px; border-radius:5px; }"
                      "QTabWidget::pane { border:1px solid #374151; }"
                      "QTabBar::tab { background:#252c39; padding:10px 18px; }"
                      "QTabBar::tab:selected { background:#334155; }");
    }
private:
    MappingSettings settings_;
    OutputController output_;
    QComboBox* firstScreen_ = nullptr;
    QComboBox* secondScreen_ = nullptr;
    QLabel* selectedFile_ = nullptr;
    QString videoPath_;
    MappingPreview* preview_ = nullptr;
    QSlider* blendSlider_ = nullptr;
    QSlider* brightnessSlider_ = nullptr;
    QSlider* gammaSlider_ = nullptr;
    QComboBox* colorProjector_ = nullptr;
    std::array<QSlider*,3> colorSliders_{};
    std::array<QLabel*,3> colorLabels_{};

    void saveSettings() {
        auto path = QFileDialog::getSaveFileName(this,"Save calibration", "mapping-settings.json","JSON (*.json)");
        if(path.isEmpty()) return;
        QJsonObject root;
        root["version"] = 2;
        root["blendPercent"] = settings_.blend;
        root["brightnessPercent"] = settings_.brightness;
        root["blendGammaTenths"] = settings_.blendGammaTenths;
        QJsonArray gains;
        for(const auto& channel:settings_.rgbGains) gains.append(QJsonArray{channel[0],channel[1],channel[2]});
        root["rgbGains"] = gains;
        QJsonArray projectors;
        for(const auto& quad : settings_.quads) {
            QJsonArray points;
            for(const auto& point : quad) points.append(QJsonArray{point.x(),point.y()});
            projectors.append(points);
        }
        root["projectors"] = projectors;
        QFile file(path);
        if(!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(root).toJson()) < 0)
            QMessageBox::warning(this,"Save failed","Cannot write settings file.");
    }
    void loadSettings() {
        auto path = QFileDialog::getOpenFileName(this,"Load calibration",QString(),"JSON (*.json)");
        if(path.isEmpty()) return;
        QFile file(path);
        if(!file.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this,"Load failed","Cannot read settings file."); return;
        }
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(file.readAll(),&error);
        if(error.error != QJsonParseError::NoError || !doc.isObject()) {
            QMessageBox::warning(this,"Load failed","Invalid JSON."); return;
        }
        auto root = doc.object();
        auto projectors = root.value("projectors").toArray();
        if((root.value("version").toInt()!=1 && root.value("version").toInt()!=2) || projectors.size()!=2) {
            QMessageBox::warning(this,"Load failed","Unsupported settings format."); return;
        }
        MappingSettings loaded;
        for(int n=0;n<2;++n) {
            auto quad = projectors[n].toArray();
            if(quad.size()!=4) { QMessageBox::warning(this,"Load failed","Invalid corner count."); return; }
            loaded.quads[n].clear();
            for(const auto& value:quad) {
                auto coords = value.toArray();
                if(coords.size()!=2 || !coords[0].isDouble() || !coords[1].isDouble()) {
                    QMessageBox::warning(this,"Load failed","Invalid coordinates."); return;
                }
                double x=coords[0].toDouble(), y=coords[1].toDouble();
                if(!std::isfinite(x)||!std::isfinite(y)||x<0||x>1||y<0||y>1) {
                    QMessageBox::warning(this,"Load failed","Coordinates out of range."); return;
                }
                loaded.quads[n] << QPointF(x,y);
            }
        }
        loaded.blend = std::clamp(root.value("blendPercent").toInt(15),0,40);
        loaded.brightness = std::clamp(root.value("brightnessPercent").toInt(100),0,100);
        if(root.value("version").toInt()==2) {
            loaded.blendGammaTenths=std::clamp(root.value("blendGammaTenths").toInt(22),10,35);
            const auto gains=root.value("rgbGains").toArray();
            if(gains.size()!=2) { QMessageBox::warning(this,"Load failed","Invalid RGB settings."); return; }
            for(int n=0;n<2;++n) {
                const auto channel=gains[n].toArray();
                if(channel.size()!=3) { QMessageBox::warning(this,"Load failed","Invalid RGB channel count."); return; }
                for(int c=0;c<3;++c) {
                    if(!channel[c].isDouble()) { QMessageBox::warning(this,"Load failed","Invalid RGB value."); return; }
                    loaded.rgbGains[n][c]=std::clamp(channel[c].toInt(100),0,100);
                }
            }
        }
        settings_ = loaded;
        for(int i=0;i<2;++i) output_.setWarpQuad(i,settings_.quads[i]);
        blendSlider_->setValue(settings_.blend);
        brightnessSlider_->setValue(settings_.brightness);
        gammaSlider_->setValue(settings_.blendGammaTenths);
        output_.setBlendGamma(settings_.blendGammaTenths/10.0);
        for(int n=0;n<2;++n) {
            const auto& gain=settings_.rgbGains[n];
            output_.setRgbGain(n,gain[0],gain[1],gain[2]);
        }
        const int active=colorProjector_->currentIndex();
        if(active>=0 && active<2)
            for(int c=0;c<3;++c) {
                QSignalBlocker blocker(colorSliders_[c]);
                const int gain=settings_.rgbGains[active][c];
                colorSliders_[c]->setValue(gain);
                colorLabels_[c]->setText(QString("%1: %2%").arg(QStringList{"Red","Green","Blue"}[c]).arg(gain));
            }
        preview_->update();
    }
};

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    MainWindow window;
    window.show();
    return app.exec();
}
