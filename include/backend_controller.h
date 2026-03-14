#ifndef BACKEND_CONTROLLER_H
#define BACKEND_CONTROLLER_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QDir>
#include <opencv2/opencv.hpp>
#include "camera_handler.h"
#include "ai_processing.h"

class BackendController : public QObject {
    Q_OBJECT

public:
    explicit BackendController(QObject *parent = nullptr);
    ~BackendController();

    // เริ่มต้นระบบ (เปิดกล้อง)
    void start();

public slots:
    // ── รับคำสั่งจาก UI ──────────────────────────────────────

    // ถ่ายภาพ Frame ปัจจุบันแล้วส่งให้ AI ประมวลผล
    void capture();

    // วาดกรอบและ Label ลงบนภาพ แล้วบันทึกลง Disk (และส่งออก USB อัตโนมัติถ้ามี)
    void save(const QString &userText);

    // ลบไฟล์ที่กำลังจัดการอยู่ (ถ้ามีอยู่จริง)
    void discard();

    // ส่งออกไฟล์ที่บันทึกแล้วไปยัง USB Drive
    void exportToUsb(const QString &fileName);

    // รีเฟรชรายชื่อไฟล์ล่าสุดในโฟลเดอร์
    void refreshFileList();

    // [หมายเหตุ] adjustImage ยังไม่ implement เต็มรูปแบบ
    // signature: (int brightnessStep, bool denoise)
    // ต้องแก้ MainWindow::reqAdjust ให้ตรงก่อนจึงจะ connect ได้
    void adjustImage(int brightnessStep, bool denoise);

signals:
    // ── ส่งข้อมูลกลับไปที่ UI ────────────────────────────────

    // ภาพสดจากกล้อง → แสดงที่หน้า Capture
    void frameReady(const QImage &img);

    // สถานะกล้อง → true = เจอกล้องแล้ว, false = ไม่พบกล้อง
    void cameraReady(bool ready);

    // ผลลัพธ์ AI เสร็จแล้ว → เปิดหน้า Review
    // [แก้ไข] รวม reviewReady เข้ามาที่นี่ signal เดียว ป้องกัน showReviewMode ถูกเรียก 2 ครั้ง
    void resultReady(const QImage &image, const QString &fileName);

    // รายชื่อไฟล์ล่าสุด → อัปเดต History List
    void fileListUpdated(const QStringList &files);

    // ข้อความสถานะ → แสดงที่ StatusBar
    void statusMessage(const QString &msg);

private slots:
    // จัดการ Frame ที่ได้จากกล้อง (เก็บ + ส่งต่อให้ UI)
    void processCameraFrame(const cv::Mat &frame);

    // จัดการผลลัพธ์ที่ได้จาก AI Thread
    void handleAiResult(const FrameResult &result);

private:
    CameraHandler  *camera;      // ตัวจัดการกล้อง
    QThread        *aiThread;    // Thread แยกสำหรับประมวลผล AI
    AI_Processing  *aiProcessor; // ตัวประมวลผล AI

    // ── ตัวแปรสถานะภาพ ───────────────────────────────────────
    cv::Mat currentLiveFrame;  // Frame สดล่าสุดจากกล้อง
    cv::Mat lastCapturedFrame; // Frame ที่กด Capture ไว้ (ใช้ตอน Save)
    QString currentFileName;   // ชื่อไฟล์ที่กำลังจัดการอยู่
    bool    cameraConnected = false; // ติดตามสถานะกล้อง — ป้องกัน emit ซ้ำ

    // ── ข้อมูลจาก AI ─────────────────────────────────────────
    QString currentAiLabel;        // Label ที่ AI ตรวจจับได้
    int     currentX = 50;         // พิกัด X ของ Bounding Box
    int     currentY = 50;         // พิกัด Y ของ Bounding Box

    // โฟลเดอร์เก็บรูปภาพ (สร้างอัตโนมัติตอน Constructor)
    const QString SAVE_PATH = QDir::homePath() + "/MagOp-project/ai_output";

    // แปลง OpenCV Mat (BGR) เป็น Qt QImage (RGB)
    QImage matToQImage(const cv::Mat &mat);
};

#endif // BACKEND_CONTROLLER_H