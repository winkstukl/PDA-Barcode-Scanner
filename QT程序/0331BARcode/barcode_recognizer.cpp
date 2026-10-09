#include "barcode_recognizer.h"
#include <zbar.h>

#if __arm__
#include "opencv4/opencv2/imgproc/imgproc.hpp"
#else
#include "opencv2/imgproc.hpp"
#endif

BarcodeRecognizer::BarcodeRecognizer()
{
}

cv::Mat BarcodeRecognizer::toGray(const cv::Mat &src) const
{
    if (src.empty()) {
        return cv::Mat();
    }

    if (src.channels() == 1) {
        return src.clone();
    }

    cv::Mat gray;
    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
    return gray;
}

cv::Mat BarcodeRecognizer::preprocess(const cv::Mat &src) const
{
    cv::Mat gray = toGray(src);
    if (gray.empty()) {
        return cv::Mat();
    }

    cv::Mat blurImg;
    cv::GaussianBlur(gray, blurImg, cv::Size(3, 3), 0);
    cv::equalizeHist(blurImg, blurImg);

    cv::Mat binImg;
    cv::adaptiveThreshold(
        blurImg,
        binImg,
        255,
        cv::ADAPTIVE_THRESH_GAUSSIAN_C,
        cv::THRESH_BINARY,
        31,
        10
    );

    return binImg;
}

cv::Mat BarcodeRecognizer::centerRoi(const cv::Mat &src) const
{
    if (src.empty()) {
        return cv::Mat();
    }

    int x = src.cols / 6;
    int y = src.rows / 3;
    int w = src.cols * 2 / 3;
    int h = src.rows / 3;

    if (x < 0 || y < 0 || w <= 0 || h <= 0) {
        return src.clone();
    }

    if (x + w > src.cols) {
        w = src.cols - x;
    }

    if (y + h > src.rows) {
        h = src.rows - y;
    }

    if (w <= 0 || h <= 0) {
        return src.clone();
    }

    return src(cv::Rect(x, y, w, h)).clone();
}

bool BarcodeRecognizer::scanImage(const cv::Mat &img, QString &code, QString &type)
{
    if (img.empty()) {
        return false;
    }

    cv::Mat scanMat = img;
    if (!scanMat.isContinuous()) {
        scanMat = scanMat.clone();
    }

    zbar::ImageScanner scanner;
    scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_ENABLE, 1);
    scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_X_DENSITY, 2);
    scanner.set_config(zbar::ZBAR_NONE, zbar::ZBAR_CFG_Y_DENSITY, 1);

    zbar::Image zimg(
        scanMat.cols,
        scanMat.rows,
        "Y800",
        scanMat.data,
        scanMat.cols * scanMat.rows
    );

    int n = scanner.scan(zimg);
    if (n <= 0) {
        return false;
    }

    for (zbar::Image::SymbolIterator symbol = zimg.symbol_begin();
         symbol != zimg.symbol_end();
         ++symbol) {
        code = QString::fromStdString(symbol->get_data());
        type = QString::fromStdString(symbol->get_type_name());
        return !code.isEmpty();
    }

    return false;
}

bool BarcodeRecognizer::recognize(const cv::Mat &frame, QString &code, QString &type)
{
    code.clear();
    type.clear();

    if (frame.empty()) {
        return false;
    }

    cv::Mat roi = centerRoi(frame);
    cv::Mat grayRoi = toGray(roi);
    cv::Mat binRoi = preprocess(roi);
    cv::Mat grayFull = toGray(frame);
    cv::Mat binFull = preprocess(frame);

    if (scanImage(grayRoi, code, type)) {
        return true;
    }

    if (scanImage(binRoi, code, type)) {
        return true;
    }

    if (scanImage(grayFull, code, type)) {
        return true;
    }

    if (scanImage(binFull, code, type)) {
        return true;
    }

    return false;
}
