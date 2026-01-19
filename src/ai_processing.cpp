#include "ai_processing.h" // CMake รู้จัก include path แล้ว ใส่ชื่อไฟล์ได้เลย
#include <QThread>
#include <QDateTime>
#include <iostream>

AI_Processing::AI_Processing(QObject *parent) : QObject(parent) {
    isBusy = false;
}

void AI_Processing::addFrameToQueue(const cv::Mat &frame) {
    QMutexLocker locker(&mutex);
    frameQueue.push(frame.clone());
    // กระตุ้นให้ทำงานใน Thread ตัวเอง
    QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
}

void AI_Processing::processNextFrame() {
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
    // [ZONE AI SIMULATION]
    // ==========================================
    std::cout << "[AI] Analyzing frame..." << std::endl;
    QThread::msleep(500); // จำลองว่าคิดหนัก

    FrameResult result;
    result.originalImage = inputFrame;
    
    QDateTime now = QDateTime::currentDateTime();
    result.timestamp = now.toString("HH:mm:ss");

    // จำลองการเจอวัตถุตามตัวเลขเวลา (เช่น 134510 -> เจอ 6 ตัว)
    QString simulateText = now.toString("HHmmss");
    int startX = 50;

    for (int i = 0; i < simulateText.length(); i++) {
        DetectedObject obj;
        obj.label = QString(simulateText.at(i));
        
        // ขยับกล่องไปทางขวาเรื่อยๆ
        obj.boundingBox = cv::Rect(startX + (i * 50), 100, 40, 60);
        obj.confidence = 0.85f + (float)(rand() % 15) / 100.0f;
        
        result.detections.push_back(obj);
    }
    // ==========================================

    emit resultReady(result);

    // เช็คงานถัดไป
    mutex.lock();
    isBusy = false;
    bool hasMore = !frameQueue.empty();
    mutex.unlock();

    if (hasMore) {
        QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
    }
}