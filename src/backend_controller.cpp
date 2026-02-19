#include "backend_controller.h"
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <iostream>
#include <QDir>

BackendController::BackendController(QObject *parent) : QObject(parent) {
    aiThread = new QThread();
    aiProcessor = new AI_Processing();
    aiProcessor->moveToThread(aiThread);
    aiThread->start();

    camera = new CameraHandler();

    connect(camera, &CameraHandler::frameReady, this, &BackendController::processCameraFrame);
    connect(aiProcessor, &AI_Processing::resultReady, this, &BackendController::handleAiResult);
    
    qDebug() << "Backend: Initialized.";
}

// *** ต้องมี Destructor ให้ตรงกับที่ประกาศไว้ ***
BackendController::~BackendController() {
    camera->stopCamera();
    if(aiThread->isRunning()) {
        aiThread->quit();
        aiThread->wait();
    }
    delete aiProcessor;
    delete aiThread;
    delete camera;
    qDebug() << "Backend: Destroyed.";
}

// *** เพิ่มฟังก์ชัน start() ที่หายไป ***
void BackendController::start() {
    qDebug() << "Backend: Starting camera...";
    camera->startCamera(0); 
}

void BackendController::processCameraFrame(cv::Mat frame) {
    if (!frame.empty()) {
        currentLiveFrame = frame.clone();
        emit frameReady(matToQImage(frame));
    }
}

void BackendController::capture() {
    if (!currentLiveFrame.empty()) {
        emit statusMessage("Processing AI...");
        aiProcessor->addFrameToQueue(currentLiveFrame);
    } else {
        emit statusMessage("Camera not ready!");
    }
}

void BackendController::handleAiResult(FrameResult result) {
    originalCvImage = result.originalImage.clone(); 
    currentEditedImage = result.originalImage.clone(); 
    currentTimestamp = result.timestamp;
    
    if (result.detections.empty()) {
        currentText = "No Object";
    } else {
        currentText = result.detections[0].label;
    }
    emit reviewReady(matToQImage(currentEditedImage), currentText);
}

void BackendController::adjustImage(int brightnessStep, bool denoise) {
    if (originalCvImage.empty()) return;
    cv::Mat temp;
    originalCvImage.convertTo(temp, -1, 1.0, brightnessStep);
    if (denoise) cv::GaussianBlur(temp, temp, cv::Size(3, 3), 0);
    currentEditedImage = temp;
    emit reviewReady(matToQImage(currentEditedImage), currentText); 
}

// *** เพิ่มฟังก์ชัน save(QString) ให้ตรงกับ Slot ใน Header ***
void BackendController::save(QString userText) {
    currentText = userText;
    saveToDiskAndUsb();
    emit statusMessage("Saved Successfully ✅");
    qDebug() << "Backend: Data saved.";
}

// *** เพิ่มฟังก์ชัน discard() ***
void BackendController::discard() {
    emit statusMessage("Discarded ❌");
    qDebug() << "Backend: Process discarded.";
}

// *** เพิ่มฟังก์ชันจัดการไฟล์ที่ Linker หาไม่เจอ ***
void BackendController::refreshFileList() {
    QString folderPath = QDir::homePath() + "/MagOp-project/ai_output";
    QDir dir(folderPath);
    QStringList filters; filters << "*.jpg";
    dir.setNameFilters(filters);
    emit fileListUpdated(dir.entryList());
}

void BackendController::deleteFile(const QString &fileName) {
    QString folderPath = QDir::homePath() + "/MagOp-project/ai_output";
    QFile::remove(folderPath + "/" + fileName);
    refreshFileList();
}

void BackendController::saveToDiskAndUsb() {
    QString homePath = QDir::homePath();
    QString localFolder = homePath + "/MagOp-project/ai_output";
    QDir().mkpath(localFolder);

    QString fileTimestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString imgName = QString("RESULT_%1.jpg").arg(fileTimestamp);

    cv::imwrite((localFolder + "/" + imgName).toStdString(), currentEditedImage);
    refreshFileList();
}

QImage BackendController::matToQImage(const cv::Mat &mat) {
    if(mat.empty()) return QImage();
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage((const unsigned char*)(rgb.data), rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
}