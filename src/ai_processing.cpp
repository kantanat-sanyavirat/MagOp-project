#include "ai_processing.h"
#include <QThread>
#include <iostream>

AI_Processing::AI_Processing(QObject *parent) : QObject(parent)
{
    isBusy = false;
}

void AI_Processing::addFrameToQueue(const cv::Mat &frame)
{
    QMutexLocker locker(&mutex);
    frameQueue.push(frame.clone()); // Clone รูปเพื่อความปลอดภัย
    
    // กระตุ้นให้เริ่มทำงาน
    QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
}

void AI_Processing::processNextFrame()
{
    mutex.lock();
    if (frameQueue.empty() || isBusy) {
        mutex.unlock();
        return;
    }

    cv::Mat inputFrame = frameQueue.front();
    frameQueue.pop();
    isBusy = true;
    mutex.unlock();

    // ==========================================
    // [ZONE AI] (Simulation)
    // ==========================================
    std::cout << "[AI Processing] Analyzing... Queue left: " << frameQueue.size() << std::endl;
    
    // จำลองเวลาประมวลผล 0.5 วินาที
    QThread::msleep(500); 

    // วาดผลลัพธ์สมมติ
    cv::putText(inputFrame, "ID: testsubject01", cv::Point(50, 50), 
                cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 0, 0), 2); // สีน้ำเงิน
    // ==========================================
    // ==========================================

    // ส่งงานกลับ
    emit resultReady(inputFrame, "Detected: 998877");

    mutex.lock();
    isBusy = false;
    bool hasMore = !frameQueue.empty();
    mutex.unlock();

    if (hasMore) {
        QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
    }
}