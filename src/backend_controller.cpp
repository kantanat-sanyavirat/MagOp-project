#include "backend_controller.h"
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QPainter>
#include <QDebug>

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
    // ดึงข้อมูล Detection ตัวแรก (ถ้าไม่มีให้ใช้ค่า default)
    currentAiLabel = result.detections.empty() ? "Unknown"                       : result.detections[0].label;
    currentX       = result.detections.empty() ? 50                              : result.detections[0].boundingBox.x;
    currentY       = result.detections.empty() ? 50                              : result.detections[0].boundingBox.y;

    // วาด Bounding Box บนภาพต้นฉบับ
    cv::Mat previewMat = result.originalImage.clone();
    cv::rectangle(previewMat,
                  cv::Rect(currentX, currentY, 250, 200),
                  cv::Scalar(0, 255, 0), 3);

    // ตั้งชื่อไฟล์ตาม Timestamp ป้องกันชื่อซ้ำ
    currentFileName = "SCAN_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + ".jpg";

    // [แก้ไข] emit เพียง signal เดียว (ลบ reviewReady ออกแล้ว)
    emit resultReady(matToQImage(previewMat), currentFileName);
}

// ─────────────────────────────────────────────────────────────
// Public Slots — รับคำสั่งจาก UI
// ─────────────────────────────────────────────────────────────

void BackendController::capture() {
    if (currentLiveFrame.empty()) {
        emit statusMessage("กล้องยังไม่พร้อม!");
        return;
    }

    emit statusMessage("กำลังประมวลผล AI...");
    lastCapturedFrame = currentLiveFrame.clone(); // เก็บ Frame นิ่งไว้ใช้ตอน Save

    // ── Mockup AI Result ─────────────────────────────────────
    // TODO: แทนที่ block นี้ด้วย aiProcessor->process(lastCapturedFrame) เมื่อพร้อม
    FrameResult mock;
    mock.originalImage = lastCapturedFrame;
    mock.timestamp     = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    Detection d;
    d.label       = "Product_A";
    d.boundingBox = cv::Rect(100, 100, 250, 200);
    mock.detections.push_back(d);

    handleAiResult(mock);
    // ─────────────────────────────────────────────────────────
}

void BackendController::save(const QString &userText) {
    if (lastCapturedFrame.empty()) {
        emit statusMessage("ไม่มีภาพให้บันทึก");
        return;
    }

    // ① วาด Bounding Box บนภาพต้นฉบับ
    cv::Mat saveMat = lastCapturedFrame.clone();
    cv::rectangle(saveMat,
                  cv::Rect(currentX, currentY, 250, 200),
                  cv::Scalar(0, 255, 0), 3);

    // ② วาด Label ด้วย QPainter (รองรับภาษาไทย ต่างจาก cv::putText)
    QImage img = matToQImage(saveMat);
    QPainter painter(&img);
    painter.setFont(QFont("Sans", 30, QFont::Bold));
    painter.setPen(Qt::green);
    painter.drawText(currentX, currentY - 10, userText);
    painter.end();

    // ③ บันทึกลง Disk
    if (img.save(SAVE_PATH + "/" + currentFileName, "JPG")) {
        // บันทึก label ลงไฟล์ .txt คู่กัน
        QString labelFileName = currentFileName;
        labelFileName.replace(".jpg", ".txt", Qt::CaseInsensitive);
        QFile labelFile(SAVE_PATH + "/" + labelFileName);
        if (labelFile.open(QIODevice::WriteOnly | QIODevice::Text))
            QTextStream(&labelFile) << userText;

        emit statusMessage("บันทึกสำเร็จ: " + currentFileName);
        refreshFileList();

        // ส่งออกไป USB ทันทีหลังบันทึก (ถ้าไม่มี USB ก็แค่ขึ้นแจ้งเตือน ไม่ error)
        exportToUsb(currentFileName);
    } else {
        emit statusMessage("บันทึกล้มเหลว ตรวจสอบพื้นที่ว่าง");
    }
}

void BackendController::discard() {
    if (currentFileName.isEmpty()) return;

    // [แก้ไข] เช็คว่าไฟล์มีอยู่จริงก่อนลบ ป้องกัน error กรณีกด DELETE ก่อน SAVE
    const QString fullPath = SAVE_PATH + "/" + currentFileName;
    if (QFile::exists(fullPath)) {
        QFile::remove(fullPath);

        // ลบไฟล์ .txt label คู่กันด้วย
        QString labelPath = fullPath;
        labelPath.replace(".jpg", ".txt", Qt::CaseInsensitive);
        QFile::remove(labelPath);

        emit statusMessage("ลบไฟล์สำเร็จ: " + currentFileName);
        refreshFileList();
    } else {
        emit statusMessage("ยกเลิกการบันทึก");
    }
}

void BackendController::exportToUsb(const QString &fileName) {
    // ค้นหา USB Drive ที่ mount อยู่ใน /media/<username>/
    QDir mediaDir("/media/" + qgetenv("USER"));
    const QStringList drives = mediaDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    if (drives.isEmpty()) {
        emit statusMessage("ไม่พบ USB Drive!");
        return;
    }

    // คัดลอกไปยัง Drive แรกที่พบ
    const QString dest = mediaDir.absoluteFilePath(drives.first()) + "/" + fileName;
    if (QFile::copy(SAVE_PATH + "/" + fileName, dest)) {
        // [แก้ไข] ใช้ startDetached แทน execute เพื่อไม่บล็อก UI Thread
        QProcess::startDetached("sync", {});
        emit statusMessage("ส่งออกไป USB สำเร็จ");
    } else {
        emit statusMessage("ส่งออกล้มเหลว ตรวจสอบพื้นที่ว่าง");
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
    emit statusMessage("adjustImage: ยังไม่ implement");
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