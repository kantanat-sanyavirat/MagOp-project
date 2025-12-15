#include "camera_handler.h"
#include <iostream> // เอาไว้ cout

CameraHandler::CameraHandler(QObject *parent) : QObject(parent)
{
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CameraHandler::loop);
}

CameraHandler::~CameraHandler()
{
    stopCamera();
}

void CameraHandler::startCamera(int camIndex)
{
    if (!cap.isOpened()) {
        cap.open(camIndex, cv::CAP_V4L2); // ลองใส่ V4L2 เพื่อความชัวร์บน Pi
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    }

    if (cap.isOpened()) {
        qDebug() << "Camera started successfully.";
        timer->start(30);
    } else {
        qDebug() << "Error: Could not open camera index" << camIndex;
    }
}

void CameraHandler::stopCamera()
{
    timer->stop();
    if (cap.isOpened()) {
        cap.release();
        qDebug() << "Camera stopped.";
    }
}

void CameraHandler::loop()
{
    if (!cap.isOpened()) return;
    cap >> currentFrame;
    if (currentFrame.empty()) return;
    emit frameReady(currentFrame.clone());
}

cv::Mat CameraHandler::getCurrentFrame() const
{
    return currentFrame.clone();
}

// --- ฟังก์ชันบันทึกภาพที่เพิ่มเข้ามา ---
void CameraHandler::saveCapturedImage(const cv::Mat& frame)
{
    if (frame.empty()) return;

    // 1. ตั้งชื่อโฟลเดอร์เก็บรูป
    QString folderName = "captured_images";

    // 2. ถ้ายังไม่มีโฟลเดอร์นี้ ให้สร้างใหม่
    if (!QDir(folderName).exists()) {
        QDir().mkdir(folderName);
    }

    // 3. ตั้งชื่อไฟล์ตามเวลา (เช่น img_20251216_123045.jpg)
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QString("%1/img_%2.jpg").arg(folderName).arg(timestamp);

    // 4. บันทึกไฟล์
    bool success = cv::imwrite(filename.toStdString(), frame);

    if (success) {
        std::cout << "Saved: " << filename.toStdString() << std::endl;
    } else {
        std::cerr << "Error: Failed to save image!" << std::endl;
    }
}