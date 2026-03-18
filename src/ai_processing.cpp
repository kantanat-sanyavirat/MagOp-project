#include "ai_processing.h"
#include <QDateTime>
#include <QThread>
#include <QDebug>

AI_Processing::AI_Processing(QObject *parent) : QObject(parent) {
    isBusy = false;
    qRegisterMetaType<FrameResult>("FrameResult");
}

AI_Processing::~AI_Processing() {
    QMutexLocker locker(&mutex);
    frameQueue.clear();
}

void AI_Processing::addFrameToQueue(cv::Mat frame) {
    QMutexLocker locker(&mutex);
    if (frame.empty()) return;

    frameQueue.push_back(frame.clone());

    // If not already processing, kick off the queue
    if (!isBusy) {
        QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
    }
}

void AI_Processing::processNextFrame() {
    cv::Mat frame;

    // Pull one frame from the queue
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

    // Run inference (replace runFakeAI with real model call when ready)
    FrameResult result = runFakeAI(frame);

    emit resultReady(result);

    // Schedule next frame — safe recursive call via event loop
    QMetaObject::invokeMethod(this, "processNextFrame", Qt::QueuedConnection);
}

FrameResult AI_Processing::runFakeAI(cv::Mat frame) {
    // TODO: Replace with real YOLOv8 (object detection) + PP-OCRv5 (OCR) inference
    QThread::msleep(500); // Simulate processing delay

    FrameResult result;
    result.originalImage = frame.clone();
    result.timestamp     = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    Detection det;
    det.id          = 1;
    det.label       = "Serial Number";
    det.confidence  = 0.95f;
    det.boundingBox = cv::Rect(100, 100, 200, 150);

    result.detections.push_back(det);

    qDebug() << "[AI] Frame processed at" << result.timestamp;
    return result;
}