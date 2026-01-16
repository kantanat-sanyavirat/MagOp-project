#ifndef AIPROCESSING_H
#define AIPROCESSING_H

#include <QObject>
#include <QMutex>
#include <queue>
#include <opencv2/opencv.hpp>
#include <QString>

//ชั่วคราว: โครงสร้างข้อมูลสำหรับส่งผลลัพธ์การตรวจจับ
struct DetectionResult {
    cv::Mat originalImage; // รูปต้นฉบับ (ยังไม่วาดอะไรทับ)
    cv::Rect boundingBox;  // พิกัดสี่เหลี่ยม (x, y, width, height)
    QString detectedText;  // ข้อความที่ AI อ่านได้
    float confidence;      // ความมั่นใจ (0.0 - 1.0)
};

class AI_Processing : public QObject
{
    Q_OBJECT

public:
    explicit AI_Processing(QObject *parent = nullptr);
    void addFrameToQueue(const cv::Mat &frame);

signals:
    // --- 2. แก้ Signal ให้ส่งกล่องข้อมูลแทน ---
    void resultReady(DetectionResult data); 

private slots:
    void processNextFrame();

private:
    std::queue<cv::Mat> frameQueue;
    QMutex mutex;
    bool isBusy;
};

#endif // AIPROCESSING_H