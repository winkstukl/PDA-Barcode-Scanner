#include "camera.h"
#include <unistd.h>
#include <QDir>
#include <QDateTime>
#include <QDebug>

#if __arm__
#include "opencv4/opencv2/core/core.hpp"
#include "opencv4/opencv2/imgproc/imgproc.hpp"
#include "opencv4/opencv2/highgui/highgui.hpp"
#include "opencv4/opencv2/imgcodecs/imgcodecs.hpp"
#include "opencv4/opencv2/videoio/videoio.hpp"
#else
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/videoio.hpp"
#endif

Camera::Camera(QObject *parent)
    : QObject(parent)
{
    m_capture = new cv::VideoCapture();
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Camera::onTimeout);
}

Camera::~Camera()
{
    stopPreview();
    closeCamera();
    delete m_capture;
}

bool Camera::openCamera(int index)
{
    closeCamera();

    if (!m_capture->open(index, cv::CAP_V4L2)) {
        emit errorOccurred(QString("打开摄像头失败：/dev/video%1").arg(index));
        return false;
    }

    m_capture->set(cv::CAP_PROP_FRAME_WIDTH, 320);
    m_capture->set(cv::CAP_PROP_FRAME_HEIGHT, 240);
    m_capture->set(cv::CAP_PROP_FPS, 5);

#ifdef CAP_PROP_BUFFERSIZE
    m_capture->set(cv::CAP_PROP_BUFFERSIZE, 1);
#endif

    usleep(200000);

    cv::Mat testFrame;
    if (!m_capture->read(testFrame) || testFrame.empty()) {
        closeCamera();
        emit errorOccurred("摄像头节点已打开，但首帧获取失败");
        return false;
    }

    m_lastFrame = testFrame.clone();

    qDebug() << "camera opened";
    return true;
}

void Camera::closeCamera()
{
    if (m_capture && m_capture->isOpened()) {
        m_capture->release();
    }
}

bool Camera::startPreview(int intervalMs)
{
    if (!isOpened()) {
        emit errorOccurred("摄像头未打开，无法启动预览");
        return false;
    }

    if (!m_timer->isActive()) {
        m_timer->start(intervalMs);
    }

    return true;
}

void Camera::stopPreview()
{
    if (m_timer->isActive()) {
        m_timer->stop();
    }
}

bool Camera::isOpened() const
{
    return m_capture && m_capture->isOpened();
}

QString Camera::lastPhotoPath() const
{
    return m_lastPhotoPath;
}

cv::Mat Camera::captureMat()
{
    cv::Mat frame;

    if (!isOpened()) {
        emit errorOccurred("摄像头未打开，无法拍照");
        return frame;
    }

    for (int i = 0; i < 2; ++i) {
        m_capture->grab();
    }

    if (!m_capture->read(frame) || frame.empty()) {
        emit errorOccurred("拍照失败，获取到空帧");
        return cv::Mat();
    }

    m_lastFrame = frame.clone();
    return frame;
}

QImage Camera::saveFrame(const cv::Mat &frame, const QString &saveDir, QString *savedPath)
{
    if (frame.empty()) {
        emit errorOccurred("保存照片失败：图像为空");
        return QImage();
    }

    QDir dir(saveDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        emit errorOccurred("无法创建照片保存目录：" + saveDir);
        return QImage();
    }

    QString fileName = QString("%1/barcode_%2.jpg")
            .arg(saveDir)
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss_zzz"));

    std::vector<int> params;
    params.push_back(cv::IMWRITE_JPEG_QUALITY);
    params.push_back(85);

    if (!cv::imwrite(fileName.toStdString(), frame, params)) {
        emit errorOccurred("照片保存失败：" + fileName);
        return QImage();
    }

    m_lastPhotoPath = fileName;

    if (savedPath) {
        *savedPath = fileName;
    }

    return matToQImage(frame);
}

QImage Camera::capturePhoto(const QString &saveDir)
{
    cv::Mat frame = captureMat();
    if (frame.empty()) {
        return QImage();
    }

    return saveFrame(frame, saveDir);
}

void Camera::onTimeout()
{
    if (!isOpened()) {
        return;
    }

    cv::Mat frame;
    if (!m_capture->read(frame) || frame.empty()) {
        return;
    }

    m_lastFrame = frame.clone();
    emit readyImage(matToQImage(frame));
}

QImage Camera::matToQImage(const cv::Mat &mat) const
{
    if (mat.empty()) {
        return QImage();
    }

    if (mat.type() == CV_8UC3) {
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return img.rgbSwapped().copy();
    }

    if (mat.type() == CV_8UC1) {
        QImage img(mat.data, mat.cols, mat.rows, mat.step, QImage::Format_Grayscale8);
        return img.copy();
    }

    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    QImage img(rgb.data, rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888);
    return img.copy();
}
