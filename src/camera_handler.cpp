#include "camera_handler.h"
#include <QDebug>
#include <QDir>
#include <QDateTime>
#include <iostream>

CameraHandler::CameraHandler(QObject *parent) : QObject(parent) {
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &CameraHandler::loop);
    m_camIndex = 0;
}

CameraHandler::~CameraHandler() {
    stopCamera();
}

void CameraHandler::startCamera(int camIndex) {
    m_camIndex = camIndex;

    // Try to open camera on startup
    if (!cap.isOpened()) {
        qDebug() << "Camera: Opening index" << camIndex;
        cap.open(m_camIndex, cv::CAP_V4L2);
        if (!cap.isOpened()) cap.open(m_camIndex);
    }

    if (cap.isOpened()) {
        qDebug() << "Camera: Connected.";
        cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    } else {
        qDebug() << "Camera: Not found. Waiting to reconnect...";
    }

    // Always start the timer — loop() will keep retrying if no camera
    if (!timer->isActive()) {
        timer->start(33); // ~30 FPS
    }
}

void CameraHandler::stopCamera() {
    timer->stop();
    if (cap.isOpened()) cap.release();
}

void CameraHandler::loop() {
    // ── No camera connected ──────────────────────────────────
    if (!cap.isOpened()) {
        static int reconnectCounter = 0;
        reconnectCounter++;

        // Emit empty frame so backend can disable the SCAN button
        emit frameReady(cv::Mat());

        // Retry every ~2 seconds (60 timer ticks at 33ms each)
        if (reconnectCounter >= 60) {
            qDebug() << "Camera: Retrying connection...";
            cap.open(m_camIndex, cv::CAP_V4L2);
            if (!cap.isOpened()) cap.open(m_camIndex);

            if (cap.isOpened()) {
                qDebug() << "Camera: Reconnected. Setting resolution...";
                cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
                cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
            }
            reconnectCounter = 0;
        }
        return;
    }

    // ── Camera connected — grab frame ────────────────────────
    cap >> currentFrame;

    if (currentFrame.empty()) {
        qDebug() << "Camera: Disconnected or empty frame.";
        cap.release(); // Release so the reconnect logic above kicks in next tick
        return;
    }

    emit frameReady(currentFrame.clone());
}

void CameraHandler::saveCapturedImage(const cv::Mat& frame) {
    if (frame.empty()) return;

    QString folderName = "../captured_images";
    if (!QDir(folderName).exists()) QDir().mkdir(folderName);

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename  = QString("%1/img_%2.jpg").arg(folderName).arg(timestamp);

    if (cv::imwrite(filename.toStdString(), frame)) {
        std::cout << "[CAMERA] Raw image saved: " << filename.toStdString() << std::endl;
    }
}