#ifndef AI_PROCESSING_H
#define AI_PROCESSING_H

#include <QObject>
#include <QMetaType>
#include <QImage>
#include <QMutex>
#include <opencv2/opencv.hpp>
#include <vector>
#include <deque>

// ─────────────────────────────────────────────────────────────
// Data Structures
// ─────────────────────────────────────────────────────────────

struct Detection {
    int     id;
    QString label;
    float   confidence;
    cv::Rect boundingBox;
};

struct FrameResult {
    cv::Mat              originalImage;
    std::vector<Detection> detections;
    QString              timestamp;

    FrameResult() {}
    FrameResult(const FrameResult& other)
        : originalImage(other.originalImage.clone()),
          detections(other.detections),
          timestamp(other.timestamp) {}
    ~FrameResult() {}
};

// Register FrameResult so it can be passed through Qt signals across threads
Q_DECLARE_METATYPE(FrameResult)

// ─────────────────────────────────────────────────────────────
// AI_Processing — runs inference on a background thread
// ─────────────────────────────────────────────────────────────

class AI_Processing : public QObject
{
    Q_OBJECT

public:
    explicit AI_Processing(QObject *parent = nullptr);
    ~AI_Processing();

    // Add a frame to the processing queue
    void addFrameToQueue(cv::Mat frame);

signals:
    // Emitted when a frame has been processed
    void resultReady(FrameResult result);

private slots:
    // Processes one frame from the queue, then schedules itself again if more remain
    void processNextFrame();

private:
    // Placeholder for real ONNX/YOLOv8 + PP-OCRv5 inference
    FrameResult runFakeAI(cv::Mat frame);

    std::deque<cv::Mat> frameQueue;
    bool   isBusy;
    QMutex mutex;
};

#endif // AI_PROCESSING_H