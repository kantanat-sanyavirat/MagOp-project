#ifndef AIPROCESSING_H
#define AIPROCESSING_H

#include <QObject>
#include <QMutex>
#include <queue>
#include <vector>
#include <opencv2/opencv.hpp>
#include <QString>

// struct ลูก
struct DetectedObject {
    cv::Rect boundingBox;
    QString label;
    float confidence;
};

// struct แม่ (ที่เราจะส่ง)
struct FrameResult {
    cv::Mat originalImage;
    std::vector<DetectedObject> detections; // เก็บหลายตัว
    QString timestamp;
};
Q_DECLARE_METATYPE(FrameResult) // <--- ต้องประกาศตัวนี้

class AI_Processing : public QObject
{
    Q_OBJECT
public:
    explicit AI_Processing(QObject *parent = nullptr);
    void addFrameToQueue(const cv::Mat &frame);

signals:
    void resultReady(FrameResult data); // <--- ส่ง FrameResult

private slots:
    void processNextFrame();

private:
    std::queue<cv::Mat> frameQueue;
    QMutex mutex;
    bool isBusy;
};

#endif // AIPROCESSING_H