#include "backend_controller.h"
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <iostream>

BackendController::BackendController(QObject *parent) : QObject(parent) {
    // 1. Setup AI (สร้าง Thread แยกทำงาน)
    aiThread = new QThread();
    aiProcessor = new AI_Processing();
    aiProcessor->moveToThread(aiThread);
    aiThread->start();

    // 2. Setup Camera
    camera = new CameraHandler();

    // 3. เชื่อมสายสัญญาณภายใน (Internal Connections)
    // - กล้องส่งภาพมา -> เรียก processCameraFrame
    connect(camera, &CameraHandler::frameReady, this, &BackendController::processCameraFrame);
    
    // - AI คิดเสร็จ -> เรียก handleAiResult
    connect(aiProcessor, &AI_Processing::resultReady, this, &BackendController::handleAiResult);
}

BackendController::~BackendController() {
    // เคลียร์เมมโมรี่เมื่อปิดโปรแกรม
    camera->stopCamera();
    aiThread->quit();
    aiThread->wait();
    delete aiProcessor;
    delete aiThread;
    delete camera;
}

void BackendController::start() {
    camera->startCamera(0); // เริ่มเปิดกล้องเบอร์ 0
}

// ฟังก์ชันแปลงร่าง OpenCV(BGR) -> Qt(RGB)
QImage BackendController::matToQImage(const cv::Mat &mat) {
    if(mat.empty()) return QImage();
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage((const unsigned char*)(rgb.data), rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
}

// ---------------------------------------------------------
// การทำงานหลัก
// ---------------------------------------------------------

// 1. รับภาพสดจากกล้อง (ทำงานตลอดเวลา 30 FPS)
void BackendController::processCameraFrame(cv::Mat frame) {
    if (!frame.empty()) {
        currentLiveFrame = frame.clone(); // จำภาพล่าสุดไว้ (เผื่อกดถ่าย)
        
        // ส่งภาพไปโชว์หน้าจอ Live View
        emit frameReady(matToQImage(frame));
    }
}

// 2. สั่งถ่ายภาพ (เมื่อกดปุ่ม SCAN)
void BackendController::capture() {
    if (!currentLiveFrame.empty()) {
        emit statusMessage("Processing AI...");
        // ส่งภาพล่าสุดไปให้ AI ทำงาน (ในอีก Thread)
        aiProcessor->addFrameToQueue(currentLiveFrame);
    } else {
        emit statusMessage("Camera not ready!");
    }
}

// 3. เมื่อ AI ทำงานเสร็จ
void BackendController::handleAiResult(FrameResult result) {
    // เก็บข้อมูลลงตัวแปร
    originalCvImage = result.originalImage.clone(); 
    currentEditedImage = result.originalImage.clone(); 
    currentTimestamp = result.timestamp;
    
    if (result.detections.empty()) {
        currentText = "";
        currentX = 50; currentY = 50;
    } else {
        currentText = result.detections[0].label;
        currentX = result.detections[0].boundingBox.x;
        currentY = result.detections[0].boundingBox.y - 10;
    }

    // ส่งสัญญาณบอกหน้าจอให้สลับไปโหมด Review พร้อมรูปและข้อความ
    emit reviewReady(matToQImage(currentEditedImage), currentText);
}

// 4. ปรับแต่งภาพ (Brightness / Denoise)
void BackendController::adjustImage(int brightnessStep, bool denoise) {
    if (originalCvImage.empty()) return;

    cv::Mat temp;
    // ปรับ Brightness (ใช้ original เสมอเพื่อไม่ให้ภาพเละ)
    // Contrast = 1.0 (คงเดิม), Brightness = brightnessStep (+/-)
    originalCvImage.convertTo(temp, -1, 1.0, brightnessStep);

    if (denoise) {
        cv::GaussianBlur(temp, temp, cv::Size(3, 3), 0);
    }

    currentEditedImage = temp;
    
    // อัปเดตหน้าจอ Review (ส่ง text ว่างไป เพราะ UI มี Text อยู่แล้ว)
    emit reviewReady(matToQImage(currentEditedImage), ""); 
}

// 5. บันทึก (SAVE)
void BackendController::save(QString userText) {
    currentText = userText; // รับข้อความที่ User อาจจะแก้มา
    saveToDiskAndUsb();
    emit statusMessage("Saved Successfully ✅");
}

// 6. ยกเลิก (DISCARD)
void BackendController::discard() {
    emit statusMessage("Discarded ❌");
    // Frontend จะรับรู้เองว่าต้องกลับไปหน้า Live
}

// ---------------------------------------------------------
// ระบบจัดการไฟล์
// ---------------------------------------------------------
void BackendController::saveToDiskAndUsb() {
    QString homePath = QDir::homePath();
    QString localFolder = homePath + "/MagOp-project/ai_output";
    QString usbFolder = homePath + "/MagOp-project/Fake_FlashDrive";
    QDir().mkpath(localFolder);
    QDir().mkpath(usbFolder);

    // สร้างชื่อไฟล์
    QString fileTimestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString imgName = QString("RESULT_%1.jpg").arg(fileTimestamp);
    QString jsonName = QString("RESULT_%1.json").arg(fileTimestamp);

    // Save Image
    cv::imwrite((localFolder + "/" + imgName).toStdString(), currentEditedImage);

    // Save JSON
    QJsonObject root, overlay;
    root["image_file"] = imgName;
    root["timestamp"] = currentTimestamp;
    
    overlay["text_content"] = currentText;
    overlay["x"] = currentX;
    overlay["y"] = currentY;
    root["overlay"] = overlay;

    QFile jsonFile(localFolder + "/" + jsonName);
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(QJsonDocument(root).toJson());
        jsonFile.close();
    }

    // Copy to USB
    QFile::copy(localFolder + "/" + imgName, usbFolder + "/" + imgName);
    QFile::copy(localFolder + "/" + jsonName, usbFolder + "/" + jsonName);
    
    refreshFileList(); // อัปเดตรายการไฟล์
}

void BackendController::refreshFileList() {
    QString folderPath = QDir::homePath() + "/MagOp-project/ai_output";
    QDir dir(folderPath);
    QStringList filters; filters << "*.jpg";
    dir.setNameFilters(filters);
    dir.setSorting(QDir::Time | QDir::Reversed);
    emit fileListUpdated(dir.entryList());
}

void BackendController::deleteFile(const QString &fileName) {
    QString folderPath = QDir::homePath() + "/MagOp-project/ai_output";
    QString usbPath = QDir::homePath() + "/MagOp-project/Fake_FlashDrive";
    
    QFile::remove(folderPath + "/" + fileName);
    QFile::remove(usbPath + "/" + fileName);

    QString jsonName = fileName;
    jsonName.replace(".jpg", ".json");
    QFile::remove(folderPath + "/" + jsonName);
    QFile::remove(usbPath + "/" + jsonName);

    emit statusMessage("Deleted: " + fileName);
    refreshFileList();
}