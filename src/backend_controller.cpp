#include "backend_controller.h"
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QPainter>
#include <QDebug>
#include <cstdio>
#include <opencv2/freetype.hpp>

static const std::string THAI_FONT = "/usr/share/fonts/truetype/tlwg/Garuda.ttf";

// ─────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────

BackendController::BackendController(QObject *parent) : QObject(parent) {
    // ① ตั้งค่า AI Processor ให้ทำงานบน Thread แยก เพื่อไม่ให้ UI กระตุก
    aiThread    = new QThread();
    aiProcessor = new AI_Processing();
    aiProcessor->moveToThread(aiThread);
    aiThread->start();

    // ② ตั้งค่ากล้อง
    camera = new CameraHandler();

    // ③ เชื่อม Signal ภายใน
    connect(camera,      &CameraHandler::frameReady,     this, &BackendController::processCameraFrame);
    connect(aiProcessor, &AI_Processing::resultReady,    this, &BackendController::handleAiResult);

    // สร้างโฟลเดอร์เก็บรูปถ้ายังไม่มี
    QDir().mkpath(SAVE_PATH);
}

BackendController::~BackendController() {
    camera->stopCamera();
    aiThread->quit();
    aiThread->wait();
    delete aiProcessor;
    delete aiThread;
    delete camera;
}

// ─────────────────────────────────────────────────────────────
// Public Methods
// ─────────────────────────────────────────────────────────────

void BackendController::start() {
    camera->startCamera(0); // เปิดกล้อง index 0
}

// ─────────────────────────────────────────────────────────────
// Private Slots — จัดการข้อมูลภายใน
// ─────────────────────────────────────────────────────────────

// รับ Frame จากกล้อง → เก็บไว้รอ Capture → ส่งต่อให้ UI แสดง
void BackendController::processCameraFrame(const cv::Mat &frame) {
    if (frame.empty()) {
        // กล้องหลุด — แจ้ง UI ให้ disable ปุ่ม SCAN
        if (cameraConnected) {
            cameraConnected = false;
            emit cameraReady(false);
        }
        return;
    }
    // Frame แรกที่ได้ — แจ้ง UI ว่าเจอกล้องแล้ว
    if (!cameraConnected) {
        cameraConnected = true;
        emit cameraReady(true);
    }
    currentLiveFrame = frame.clone();
    emit frameReady(matToQImage(frame));
}

// รับผลลัพธ์จาก AI → วาดกรอบ → ส่งไปหน้า Review
void BackendController::handleAiResult(const FrameResult &result) {
    currentDetections = result.detections;
    currentAiLabel    = result.detections.empty() ? "" : result.detections[0].label;
    currentX          = result.detections.empty() ? 0  : result.detections[0].boundingBox.x;
    currentY          = result.detections.empty() ? 0  : result.detections[0].boundingBox.y;

    float score         = result.anomalyScore;
    bool  isAnomaly     = score > 0.5f;
    bool  hasDetections = !result.detections.empty();

    cv::Mat previewMat = result.originalImage.clone();

    // ① Draw bounding boxes only — no label text
    for (const auto& det : result.detections) {
        cv::rectangle(previewMat, det.boundingBox, cv::Scalar(0, 255, 0), 3);
    }

    // ② Draw anomaly status (top-right) using FreeType — supports Unicode
    {
        auto ft2 = cv::freetype::createFreeType2();
        ft2->loadFontData(THAI_FONT, 0);

        std::string anomalyStr = isAnomaly ? "Defect detected" : "No defect";
        cv::Scalar anomalyColor = isAnomaly ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 200, 0);

        // Background rect
        int base = 0;
        cv::Size textSz = ft2->getTextSize(anomalyStr, 26, -1, &base);
        int tx = previewMat.cols - textSz.width - 10;
        int ty = 10;
        cv::rectangle(previewMat,
                      cv::Point(tx - 4, ty),
                      cv::Point(tx + textSz.width + 4, ty + textSz.height + 4),
                      cv::Scalar(0, 0, 0), cv::FILLED);
        ft2->putText(previewMat, anomalyStr,
                     cv::Point(tx, ty + textSz.height),
                     26, anomalyColor, -1, cv::LINE_AA, true);

        // ③ OCR text at bottom-left
        if (!result.ocrText.isEmpty()) {
            ft2->putText(previewMat,
                         "OCR: " + result.ocrText.toStdString(),
                         cv::Point(10, previewMat.rows - 12),
                         24, cv::Scalar(255, 200, 0), -1, cv::LINE_AA, true);
        }
    }

    // ④ Status bar message
    if (!hasDetections && isAnomaly) {
        emit statusMessage("⚠ Anomaly detected but no serial number found — enter manually");
    } else if (!hasDetections) {
        emit statusMessage("No serial number detected — enter manually if needed");
    } else {
        emit statusMessage(isAnomaly
            ? QString("⚠ Anomaly score: %1").arg(score, 0, 'f', 2)
            : QString("✓ Normal  score: %1").arg(score, 0, 'f', 2));
    }

    currentFileName = "SCAN_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".jpg";
    emit resultReady(matToQImage(previewMat), currentFileName, result.ocrText);
}

// ─────────────────────────────────────────────────────────────
// Public Slots — รับคำสั่งจาก UI
// ─────────────────────────────────────────────────────────────

void BackendController::capture() {
    if (currentLiveFrame.empty()) {
        emit statusMessage("Camera not ready!");
        return;
    }

    emit statusMessage("Processing AI...");
    lastCapturedFrame = currentLiveFrame.clone();

    // Clear any pending frames in queue before adding new one
    // Prevents stale results if user taps SCAN rapidly
    aiProcessor->clearQueue();
    aiProcessor->addFrameToQueue(lastCapturedFrame);
}

void BackendController::save(const QString &userText, const QString &originalOcrText) {
    if (lastCapturedFrame.empty()) {
        emit statusMessage("No image to save");
        return;
    }

    cv::Mat saveMat = lastCapturedFrame.clone();

    // Draw bbox only if AI found detections AND user did NOT edit the text
    bool userEdited   = userText.trimmed() != originalOcrText.trimmed();
    bool hasDetection = !currentDetections.empty();

    if (hasDetection && !userEdited) {
        for (const auto& det : currentDetections)
            cv::rectangle(saveMat, det.boundingBox, cv::Scalar(0, 255, 0), 2);
    }

    // Put user's text at bottom-left (QPainter supports Thai)
    QImage img = matToQImage(saveMat);
    QPainter painter(&img);
    painter.setFont(QFont("Sans", 22, QFont::Bold));
    int tx = 12, ty = img.height() - 16;
    painter.setPen(Qt::black);
    painter.drawText(tx + 2, ty + 2, userText);  // shadow
    painter.setPen(Qt::yellow);
    painter.drawText(tx, ty, userText);
    painter.end();

    // Save to disk
    if (img.save(SAVE_PATH + "/" + currentFileName, "JPG")) {
        // บันทึก label ลงไฟล์ .txt คู่กัน
        QString labelFileName = currentFileName;
        labelFileName.replace(".jpg", ".txt", Qt::CaseInsensitive);
        QFile labelFile(SAVE_PATH + "/" + labelFileName);
        if (labelFile.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&labelFile) << userText;

        emit statusMessage("Saved: " + currentFileName);
        refreshFileList();

        // ส่งออกไป USB ทันทีหลังบันทึก (ถ้าไม่มี USB ก็แค่ขึ้นแจ้งเตือน ไม่ error)
        exportToUsb(currentFileName);
    } else {
        emit statusMessage("Save failed — check disk space");
    }
}

void BackendController::discard(const QString& fileName) {
    // ใช้ fileName ที่ส่งมา ถ้าไม่มีใช้ currentFileName
    QString target = fileName.isEmpty() ? currentFileName : fileName;

    if (!target.isEmpty()) {
        const QString fullPath = SAVE_PATH + "/" + target;
        if (QFile::exists(fullPath)) {
            QFile::remove(fullPath);
            QString labelPath = fullPath;
            labelPath.replace(".jpg", ".txt", Qt::CaseInsensitive);
            QFile::remove(labelPath);
            emit statusMessage("Deleted: " + target);
            refreshFileList();
        }
    }

    lastCapturedFrame.release();
    currentDetections.clear();
    currentFileName.clear();
}

void BackendController::exportToUsb(const QString &fileName) {
    // ค้นหา USB Drive ที่ mount อยู่ใน /media/<username>/
    QDir mediaDir("/media/" + qgetenv("USER"));
    const QStringList drives = mediaDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    if (drives.isEmpty()) {
        emit statusMessage("No USB drive found!");
        return;
    }

    // คัดลอกไปยัง Drive แรกที่พบ
    const QString dest = mediaDir.absoluteFilePath(drives.first()) + "/" + fileName;
    if (QFile::copy(SAVE_PATH + "/" + fileName, dest)) {
        // [แก้ไข] ใช้ startDetached แทน execute เพื่อไม่บล็อก UI Thread
        QProcess::startDetached("sync", {});
        emit statusMessage("Exported to USB successfully");
    } else {
        emit statusMessage("Export failed — check USB space");
    }
}

void BackendController::refreshFileList() {
    QDir dir(SAVE_PATH);
    const QStringList files = dir.entryList(
        QStringList() << "*.jpg",
        QDir::Files,
        QDir::Time); // เรียงใหม่สุดก่อน
    emit fileListUpdated(files);
}

void BackendController::adjustImage(int brightnessStep, bool denoise) {
    // TODO: implement การปรับความสว่างและ Denoise
    Q_UNUSED(brightnessStep)
    Q_UNUSED(denoise)
    emit statusMessage("adjustImage: not implemented");
}

// ─────────────────────────────────────────────────────────────
// Helper
// ─────────────────────────────────────────────────────────────

// แปลง OpenCV Mat (BGR) → Qt QImage (RGB)
QImage BackendController::matToQImage(const cv::Mat &mat) {
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage(reinterpret_cast<const unsigned char*>(rgb.data),
                  rgb.cols, rgb.rows,
                  static_cast<int>(rgb.step),
                  QImage::Format_RGB888).copy();
}