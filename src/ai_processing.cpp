#include "ai_processing.h"
#include <QDateTime>
#include <QThread>
#include <QDebug>

AI_Processing::AI_Processing(QObject *parent) : QObject(parent) {
    isBusy = false;
    // ลงทะเบียนชนิดข้อมูล (กันเหนียว)
    qRegisterMetaType<FrameResult>("FrameResult");
}

AI_Processing::~AI_Processing() {
    // ล้างคิว
    QMutexLocker locker(&mutex);
    frameQueue.clear();
}

void AI_Processing::addFrameToQueue(cv::Mat frame) {
    QMutexLocker locker(&mutex);
    if (!frame.empty()) {
        frameQueue.push_back(frame.clone()); // ก็อปปี้รูปเก็บเข้าคิว
        
        // ถ้าว่างอยู่ ให้เริ่มทำเลย
        if (!isBusy) {
            // ใช้ QMetaObject::invokeMethod เพื่อเรียก processNextFrame ในรอบถัดไป (กัน Stack Overflow)
            QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
        }
    }
}

// ฟังก์ชันนี้แหละครับที่ Error เมื่อกี้ ตอนนี้หายแน่นอน
void AI_Processing::processNextFrame() {
    cv::Mat frame;
    
    // 1. ดึงงานออกจากคิว
    {
        QMutexLocker locker(&mutex);
        if (frameQueue.empty()) {
            isBusy = false;
            return;
        }
        frame = frameQueue.front();
        frameQueue.pop_front();
        isBusy = true;
    }

    // 2. เริ่มประมวลผล (จำลอง AI)
    // ตรงนี้ถ้ามีโค้ด ONNX จริงๆ ให้ใส่แทน runFakeAI
    FrameResult result = runFakeAI(frame);

    // 3. ส่งผลลัพธ์กลับไป UI
    emit resultReady(result);

    // 4. ทำงานถัดไป (Recursive แบบปลอดภัย)
    QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
}

FrameResult AI_Processing::runFakeAI(cv::Mat frame) {
    // จำลองการทำงาน AI (หน่วงเวลา + สร้างกรอบมั่วๆ)
    QThread::msleep(500); // แกล้งทำเป็นคิด 0.5 วิ

    FrameResult result;
    result.originalImage = frame.clone();
    result.timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    // จำลองผลลัพธ์ (Detection)
    Detection det;
    det.id = 1;
    det.label = "Screw M4";
    det.confidence = 0.95;
    det.boundingBox = cv::Rect(100, 100, 200, 150); // x, y, w, h
    
    result.detections.push_back(det);
    
    qDebug() << "AI Processed Frame: " << result.timestamp;
    return result;
}