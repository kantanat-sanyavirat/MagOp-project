#include "backend_controller.h"
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QDebug>
#include <iostream>

BackendController::BackendController(QObject *parent) : QObject(parent) {
    // Setup AI Thread
    aiThread = new QThread();
    aiProcessor = new AI_Processing();
    aiProcessor->moveToThread(aiThread);
    aiThread->start();

    camera = new CameraHandler();

    // เชื่อมต่อสัญญาณภายใน
    connect(camera, &CameraHandler::frameReady, this, &BackendController::processCameraFrame);
    connect(aiProcessor, &AI_Processing::resultReady, this, &BackendController::handleAiResult);
}

BackendController::~BackendController() {
    camera->stopCamera();
    aiThread->quit();
    aiThread->wait();
    delete aiProcessor;
    delete aiThread;
    delete camera;
}

void BackendController::startSystem() {
    camera->startCamera(0);
}

// --- Image Processing Helper ---
QImage BackendController::matToQImage(const cv::Mat &mat) {
    if(mat.empty()) return QImage();
    // แปลงสี BGR (OpenCV) -> RGB (Qt)
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage((const unsigned char*)(rgb.data), rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
}

// 1. รับภาพสดจากกล้อง
void BackendController::processCameraFrame(cv::Mat frame) {
    // ส่งไปโชว์หน้าจอ UI
    emit liveFrameReady(matToQImage(frame));
}

// 2. สั่งถ่ายภาพ (UI กดปุ่ม Scan)
void BackendController::captureImage() {
    cv::Mat frame = camera->getCurrentFrame(); // ต้องเพิ่ม func นี้ใน camera_handler หรือดึงจาก loop ล่าสุด
    if (!frame.empty()) {
        emit statusMessage("Processing...");
        aiProcessor->addFrameToQueue(frame);
    }
}

// 3. AI ส่งผลกลับมา -> เตรียมเข้าโหมด Review
void BackendController::handleAiResult(FrameResult result) {
    currentCvImage = result.originalImage.clone();
    currentEditedImage = result.originalImage.clone(); // เริ่มต้นรูปแต่ง = รูปเดิม
    currentTimestamp = result.timestamp; // (หรือจะ Gen ใหม่ตอนเซฟก็ได้)
    
    if (result.detections.empty()) {
        currentText = "";
        currentX = 50; currentY = 50;
    } else {
        currentText = result.detections[0].label;
        currentX = result.detections[0].boundingBox.x;
        currentY = result.detections[0].boundingBox.y - 10;
    }

    // ส่งสัญญาณบอก UI ให้เด้งหน้า Review ขึ้นมา
    emit reviewReady(matToQImage(currentEditedImage), currentText);
}

// 4. ปรับแต่งภาพ (Slider เลื่อนปุ๊บ เรียกฟังก์ชันนี้)
void BackendController::adjustImage(double brightness, double contrast, bool denoise) {
    if (currentCvImage.empty()) return;

    cv::Mat temp;
    // ปรับ Brightness/Contrast (alpha=contrast, beta=brightness)
    // brightness รับค่า -100 ถึง 100, contrast 0.5 ถึง 2.0
    currentCvImage.convertTo(temp, -1, contrast, brightness);

    if (denoise) {
        cv::GaussianBlur(temp, temp, cv::Size(3, 3), 0);
    }

    currentEditedImage = temp;
    // อัปเดตหน้าจอทันที
    emit reviewReady(matToQImage(currentEditedImage), currentText);
}

// 5. แก้ไขข้อความ
void BackendController::updateText(const QString &newText) {
    currentText = newText;
}

// 6. ยืนยันการบันทึก (Save)
void BackendController::confirmSave() {
    saveToDiskAndUsb();
    emit statusMessage("Saved Successfully ✅");
    // อาจจะส่ง signal ให้ปิดหน้า Review กลับไปหน้า Live
}

void BackendController::discardImage() {
    emit statusMessage("Discarded ❌");
    // UI ควรปิดหน้า Review
}

// --- Logic การบันทึก ---
void BackendController::saveToDiskAndUsb() {
    QString homePath = QDir::homePath();
    QString localFolder = homePath + "/MagOp-project/ai_output";
    QString usbFolder = homePath + "/MagOp-project/Fake_FlashDrive";
    QDir().mkpath(localFolder);
    QDir().mkpath(usbFolder);

    // Timestamp ใหม่ตอนเซฟเพื่อไม่ให้ซ้ำ
    QString fileTimestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString imgName = QString("RESULT_%1.jpg").arg(fileTimestamp);
    QString jsonName = QString("RESULT_%1.json").arg(fileTimestamp);

    // Save Image
    QString localImgPath = localFolder + "/" + imgName;
    cv::imwrite(localImgPath.toStdString(), currentEditedImage);

    // Save JSON
    QJsonObject root;
    root["image_file"] = imgName;
    root["timestamp"] = currentTimestamp; // เวลาดั้งเดิมที่ AI จับได้
    
    QJsonObject overlay;
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
    QFile::copy(localImgPath, usbFolder + "/" + imgName);
    QFile::copy(localFolder + "/" + jsonName, usbFolder + "/" + jsonName);
    
    // อัปเดต List ไฟล์ใหม่ทันที
    refreshFileList();
}

// --- ส่วนจัดการไฟล์ (Delete One by One) ---

// เรียกดูรายการไฟล์ทั้งหมด
void BackendController::refreshFileList() {
    QString folderPath = QDir::homePath() + "/MagOp-project/ai_output";
    QDir dir(folderPath);
    QStringList filters;
    filters << "*.jpg"; // เอาแค่ชื่อไฟล์รูป
    dir.setNameFilters(filters);
    dir.setSorting(QDir::Time | QDir::Reversed);
    
    QStringList fileList = dir.entryList();
    emit fileListUpdated(fileList); // ส่งกลับไปให้ UI แสดงผล
}

// ลบไฟล์ทีละอัน (ตามชื่อไฟล์ที่ UI ส่งมา)
void BackendController::deleteFile(const QString &fileName) {
    QString folderPath = QDir::homePath() + "/MagOp-project/ai_output";
    QString usbPath = QDir::homePath() + "/MagOp-project/Fake_FlashDrive";
    
    // ลบไฟล์รูป
    QFile::remove(folderPath + "/" + fileName);
    QFile::remove(usbPath + "/" + fileName);

    // ลบไฟล์ JSON คู่กัน (เปลี่ยน .jpg เป็น .json)
    QString jsonName = fileName;
    jsonName.replace(".jpg", ".json");
    QFile::remove(folderPath + "/" + jsonName);
    QFile::remove(usbPath + "/" + jsonName);

    emit statusMessage("Deleted: " + fileName);
    refreshFileList(); // อัปเดตรายการใหม่หลังลบ
}