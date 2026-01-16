#include "ai_processing.h"
#include <QThread>
#include <iostream>

AI_Processing::AI_Processing(QObject *parent) : QObject(parent)
{
    isBusy = false;
}

void AI_Processing::processNextFrame()
{
    // --- 1. รับงานและล็อค (เหมือนเดิม) ---
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
    // [ZONE AI] (Simulation from Time)
    // ==========================================
    std::cout << "[AI] Simulating objects from time..." << std::endl;
    QThread::msleep(500); // แกล้งทำเป็นคิดนาน

    FrameResult result;
    result.originalImage = inputFrame;
    
    // ดึงเวลาปัจจุบัน
    QDateTime now = QDateTime::currentDateTime();
    result.timestamp = now.toString("HH:mm:ss");

    // สร้างข้อมูลตามตัวเลขเวลา (เช่น "103045")
    QString simulateText = now.toString("HHmmss");
    
    int startX = 50;  
    int startY = 100; 
    int width = 40;   
    int height = 60;  
    int gap = 10;     

    for (int i = 0; i < simulateText.length(); i++) {
        DetectedObject obj;
        
        // Label คือตัวเลขแต่ละหลัก
        obj.label = QString(simulateText.at(i)); 

        // คำนวณพิกัด X ไม่ให้ทับกัน
        int currentX = startX + (i * (width + gap));
        obj.boundingBox = cv::Rect(currentX, startY, width, height);

        // สุ่มความมั่นใจ 0.80 - 0.99
        obj.confidence = 0.80f + (float)(rand() % 20) / 100.0f;

        result.detections.push_back(obj);
    }
    // ==========================================

    // --- 2. ส่งงาน  ---
    emit resultReady(result);

    // --- 3. เช็คงานถัดไป (อย่าลืมตรงนี้!) ---
    mutex.lock();
    isBusy = false;
    bool hasMore = !frameQueue.empty();
    mutex.unlock();

    if (hasMore) {
        QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
    }
}