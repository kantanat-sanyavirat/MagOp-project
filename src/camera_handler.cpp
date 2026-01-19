#include "camera_handler.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <iostream>

CameraHandler::CameraHandler(QObject *parent) : QObject(parent) {
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CameraHandler::loop);
}

CameraHandler::~CameraHandler() {
    stopCamera();
}

void CameraHandler::startCamera(int camIndex) {
    if (!cap.isOpened()) {
        // ใช้ V4L2 เพื่อความเสถียรบน Linux/RPi
        cap.open(camIndex, cv::CAP_V4L2);
        if(!cap.isOpened()) cap.open(camIndex); // Fallback
        
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    }
    if (cap.isOpened()) {
        qDebug() << "Camera started successfully.";
        timer->start(30); // 30ms ~ 33 FPS
    }
}

void CameraHandler::stopCamera() {
    timer->stop();
    if (cap.isOpened()) cap.release();
}

void CameraHandler::loop() {
    if (!cap.isOpened()) return;
    cap >> currentFrame;
    if (currentFrame.empty()) return;
    emit frameReady(currentFrame.clone());
}

void CameraHandler::saveCapturedImage(const cv::Mat& frame) {
    if (frame.empty()) return;
    
    // เซฟลง folder captured_images ที่หน้าโปรเจค
    QString folderName = "../captured_images";
    if (!QDir(folderName).exists()) QDir().mkdir(folderName);

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QString("%1/img_%2.jpg").arg(folderName).arg(timestamp);

    if (cv::imwrite(filename.toStdString(), frame)) {
        std::cout << "[CAMERA] Backup raw image saved." << std::endl;
    }
}