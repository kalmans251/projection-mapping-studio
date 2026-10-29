#include <QtWidgets>
#include <array>
#include <cmath>
#include <algorithm>
#include "OutputController.h"

struct MappingSettings {
    std::array<QPolygonF, 2> quads;
    int blend = 15;
    int brightness = 100;

    MappingSettings() {
        quads[0] << QPointF(.05,.15) << QPointF(.56,.10) << QPointF(.53,.88) << QPointF(.07,.84);
        quads[1] << QPointF(.45,.10) << QPointF(.95,.15) << QPointF(.93,.84) << QPointF(.47,.88);
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
private:
    MappingSettings& settings_;
    int selected_ = 0;
    int dragging_ = -1;

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
        update();
    }
    void mouseReleaseEvent(QMouseEvent*) override { dragging_ = -1; }
};

class MainWindow final : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle("Projection Mapping Studio — v0.1 Preview");
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
        auto* notice = new QLabel("Calibration UI prototype · HDMI playback and edge blending are not yet implemented");
        notice->setStyleSheet("color:#9ca3af");
        root->addWidget(notice);

        auto* tabs = new QTabWidget;
        root->addWidget(tabs,1);
        auto* calibration = new QWidget;
        auto* calibrationLayout = new QHBoxLayout(calibration);
        preview_ = new MappingPreview(settings_);
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
        brightnessSlider_ = addSlider("Brightness (preview value)",100,settings_.brightness,[this](int v){settings_.brightness=v;});
        auto* reset = new QPushButton("Reset calibration");
        side->addSpacing(12);
        side->addWidget(reset);
        connect(reset,&QPushButton::clicked,this,[this]{
            settings_ = MappingSettings{};
            blendSlider_->setValue(settings_.blend);
            brightnessSlider_->setValue(settings_.brightness);
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
        auto* showTest = new QPushButton("Open test patterns on selected displays");
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
        mediaLayout->addWidget(new QLabel("Early preview: synchronized 2-way crop, no perspective warp or brightness blending yet."));
        mediaLayout->addStretch();
        tabs->addTab(media,"Media");

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
            QString error;
            if(!output_.showOutputs(firstScreen_->currentIndex(),secondScreen_->currentIndex(),error)) {
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

    void saveSettings() {
        auto path = QFileDialog::getSaveFileName(this,"Save calibration", "mapping-settings.json","JSON (*.json)");
        if(path.isEmpty()) return;
        QJsonObject root;
        root["version"] = 1;
        root["blendPercent"] = settings_.blend;
        root["brightnessPercent"] = settings_.brightness;
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
        if(root.value("version").toInt()!=1 || projectors.size()!=2) {
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
        settings_ = loaded;
        blendSlider_->setValue(settings_.blend);
        brightnessSlider_->setValue(settings_.brightness);
        preview_->update();
    }
};

int main(int argc,char** argv) {
    QApplication app(argc,argv);
    MainWindow window;
    window.show();
    return app.exec();
}
