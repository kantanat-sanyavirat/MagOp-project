#include "camera_handler.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <iostream>

CameraHandler::CameraHandler(QObject *parent) : QObject(parent) {
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CameraHandler::loop);
    m_camIndex = 0; // ค่าเริ่มต้น
}

CameraHandler::~CameraHandler() {
    stopCamera();
}

void CameraHandler::startCamera(int camIndex) {
    m_camIndex = camIndex; // เก็บ index ไว้สำหรับพยายามต่อใหม่ (reconnect)
    
    // พยายามเปิดกล้องครั้งแรก
    if (!cap.isOpened()) {
        qDebug() << "Camera: Initial attempt to open index" << camIndex;
        cap.open(m_camIndex, cv::CAP_V4L2);
        if(!cap.isOpened()) cap.open(m_camIndex); 
    }

    if (cap.isOpened()) {
        qDebug() << "Camera: Initial connection successful.";
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    } else {
        qDebug() << "Camera: Not found. Reconnection loop started...";
    }

    // เริ่ม Timer เสมอ (แม้จะยังไม่มีกล้อง) เพื่อให้ loop() ทำงานตรวจเช็กต่อไป
    if (!timer->isActive()) {
        timer->start(33); // ~30 FPS
    }
}

void CameraHandler::stopCamera() {
    timer->stop();
    if (cap.isOpened()) cap.release();
}

void CameraHandler::loop() {
    // --- ส่วนที่ 1: กรณีกล้องไม่ได้เชื่อมต่ออยู่ ---
    if (!cap.isOpened()) {
        static int reconnectCounter = 0;
        reconnectCounter++;

        // พยายามลองเปิดกล้องใหม่ทุกๆ ประมาณ 2 วินาที (60 รอบของ timer)
        if (reconnectCounter >= 60) { 
            qDebug() << "Camera: Trying to reconnect...";
            cap.open(m_camIndex, cv::CAP_V4L2);
            if(!cap.isOpened()) cap.open(m_camIndex);

            if (cap.isOpened()) {
                qDebug() << "Camera: Found! Setting up resolution...";
                cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
                cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
            }
            reconnectCounter = 0;
        }
        return; // ออกจาก loop ไปก่อนเพราะยังไม่มีกล้อง
    }

    // --- ส่วนที่ 2: กรณีกล้องเชื่อมต่ออยู่ปกติ ---
    cap >> currentFrame;

    if (currentFrame.empty()) {
        qDebug() << "Camera: Disconnected or empty frame received.";
        cap.release(); // สั่งปิดเพื่อให้เงื่อนไข !cap.isOpened() ด้านบนทำงานในรอบถัดไป
        return;
    }

    emit frameReady(currentFrame.clone());
}

void CameraHandler::saveCapturedImage(const cv::Mat& frame) {
    if (frame.empty()) return;
    
    QString folderName = "../captured_images";
    if (!QDir(folderName).exists()) QDir().mkdir(folderName);

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QString("%1/img_%2.jpg").arg(folderName).arg(timestamp);

    if (cv::imwrite(filename.toStdString(), frame)) {
        std::cout << "[CAMERA] Backup raw image saved: " << filename.toStdString() << std::endl;
    }
}