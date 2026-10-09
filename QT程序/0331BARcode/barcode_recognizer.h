#ifndef BARCODE_RECOGNIZER_H
#define BARCODE_RECOGNIZER_H

#include <QString>

#if __arm__
#include "opencv4/opencv2/core/core.hpp"
#else
#include "opencv2/core.hpp"
#endif

class BarcodeRecognizer
{
public:
    BarcodeRecognizer();

    bool recognize(const cv::Mat &frame, QString &code, QString &type);

private:
    bool scanImage(const cv::Mat &img, QString &code, QString &type);
    cv::Mat toGray(const cv::Mat &src) const;
    cv::Mat preprocess(const cv::Mat &src) const;
    cv::Mat centerRoi(const cv::Mat &src) const;
};

#endif // BARCODE_RECOGNIZER_H
