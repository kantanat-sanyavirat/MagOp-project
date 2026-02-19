#ifndef AI_PROCESSING_H
#define AI_PROCESSING_H

#include <QObject>
#include <QMetaType>  // [สำคัญ] ต้องมี
#include <QImage>
#include <QMutex>
#include <opencv2/opencv.hpp>
#include <vector>
#include <deque>

// ---------------------------------------------------------
// 1. ประกาศ Struct ก่อน (ต้องอยู่บนสุด!)
// ---------------------------------------------------------

struct Detection {
    int id;
    QString label;
    float confidence;
    cv::Rect boundingBox;
};

struct FrameResult {
    cv::Mat originalImage;
    std::vector<Detection> detections;
    QString timestamp;

    // Constructor ที่จำเป็น
    FrameResult() {}
    FrameResult(const FrameResult& other) 
        : originalImage(other.originalImage.clone()),
          detections(other.detections),
          timestamp(other.timestamp) {}
    ~FrameResult() {}
};

// ---------------------------------------------------------
// 2. สั่ง Register MetaType (ต้องอยู่หลัง Struct เสมอ!)
// ---------------------------------------------------------
Q_DECLARE_METATYPE(FrameResult)

// ---------------------------------------------------------
// 3. ประกาศ Class AI_Processing (อยู่ล่างสุด)
// ---------------------------------------------------------
class AI_Processing : public QObject
{
    Q_OBJECT
public:
    explicit AI_Processing(QObject *parent = nullptr);
    ~AI_Processing();

    void addFrameToQueue(cv::Mat frame);

signals:
    void resultReady(FrameResult result);

private slots:
    void processNextFrame();

private:
    FrameResult runFakeAI(cv::Mat frame);

    std::deque<cv::Mat> frameQueue;
    bool isBusy;
    QMutex mutex;
};

#endif // AI_PROCESSING_H