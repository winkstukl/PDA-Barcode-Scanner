#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include <QImage>
#include <QTimer>
#include <QString>

#if __arm__
#include "opencv4/opencv2/core/core.hpp"
#else
#include "opencv2/core.hpp"
#endif

namespace cv {
class VideoCapture;
}

class Camera : public QObject
{
    Q_OBJECT

public:
    explicit Camera(QObject *parent = nullptr);
    ~Camera();

    bool openCamera(int index = 0);
    void closeCamera();

    bool startPreview(int intervalMs = 120);   // 约 8~9 fps
    void stopPreview();

    bool isOpened() const;
    QString lastPhotoPath() const;

    cv::Mat captureMat();
    QImage capturePhoto(const QString &saveDir = "/mnt/sdcard/0330");
    QImage saveFrame(const cv::Mat &frame,
                     const QString &saveDir = "/mnt/sdcard/0330",
                     QString *savedPath = nullptr);
    QImage matToQImage(const cv::Mat &mat) const;

signals:
    void readyImage(const QImage &img);
    void errorOccurred(const QString &msg);

private slots:
    void onTimeout();

private:
    cv::VideoCapture *m_capture;
    QTimer *m_timer;
    QString m_lastPhotoPath;
    cv::Mat m_lastFrame;
};

#endif // CAMERA_H
