#ifndef AIPROCESSING_H
#define AIPROCESSING_H

#include <QObject>
#include <QMutex>
#include <queue>
#include <opencv2/opencv.hpp>

class AI_Processing : public QObject
{
    Q_OBJECT

public:
    explicit AI_Processing(QObject *parent = nullptr);
    
    // ฟังก์ชันรับงานเข้าคิว
    void addFrameToQueue(const cv::Mat &frame);

signals:
    // ส่งผลลัพธ์กลับ
    void resultReady(cv::Mat resultImage, QString textResult);

private slots:
    // ฟังก์ชันประมวลผลภายใน
    void processNextFrame();

private:
    std::queue<cv::Mat> frameQueue;
    QMutex mutex;
    bool isBusy;
};

#endif // AIPROCESSING_H