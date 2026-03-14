#include <QApplication>
#include "backend_controller.h"
#include "ui/mainwindow.h"
#include "ai_processing.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // ลงทะเบียน Custom Type เพื่อให้ Qt ส่งข้อมูลข้าม Thread ผ่าน Queued Connection ได้
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<FrameResult>("FrameResult");
    qDebug() << "System: MetaTypes registered.";

    BackendController backend;
    MainWindow window;

    // ──────────────────────────────────────────
    // Backend → UI  (แสดงผลที่หน้าจอ)
    // ──────────────────────────────────────────

    // ภาพสดจากกล้อง → แสดงที่หน้า Capture
    QObject::connect(&backend, &BackendController::frameReady,
                     &window,  &MainWindow::updateLiveView);

    // สถานะกล้อง → enable/disable ปุ่ม SCAN
    QObject::connect(&backend, &BackendController::cameraReady,
                     &window,  &MainWindow::setCameraReady);

    // ผลลัพธ์ AI เสร็จ → เปิดหน้า Review
    // [แก้ไข] ลบ reviewReady ออก เหลือ signal เดียวเพื่อป้องกัน showReviewMode ถูกเรียก 2 ครั้ง
    QObject::connect(&backend, &BackendController::resultReady,
                     &window,  &MainWindow::showReviewMode);

    // ข้อความสถานะ → แสดงที่ StatusBar
    QObject::connect(&backend, &BackendController::statusMessage,
                     &window,  &MainWindow::showMessage);

    // รายชื่อไฟล์ใหม่หลัง Save/Delete → อัปเดต History List ทันที
    QObject::connect(&backend, &BackendController::fileListUpdated,
                     &window,  &MainWindow::updateHistoryList);

    // ──────────────────────────────────────────
    // UI → Backend  (รับคำสั่งจากผู้ใช้)
    // ──────────────────────────────────────────

    // ปุ่ม SCAN → ถ่ายภาพและส่งให้ AI ประมวลผล
    QObject::connect(&window, &MainWindow::reqCapture,
                     &backend, &BackendController::capture);

    // ปุ่ม SAVE → บันทึกภาพพร้อม Label และส่งออก USB อัตโนมัติ
    QObject::connect(&window, &MainWindow::reqSave,
                     &backend, &BackendController::save);

    // ปุ่ม DELETE → ยกเลิกและลบไฟล์ชั่วคราว
    QObject::connect(&window, &MainWindow::reqDiscard,
                     &backend, &BackendController::discard);

    // [แก้ไข] ลบการ connect reqAdjust ออก เพราะ signature ไม่ตรงกับ adjustImage
    // (MainWindow ส่ง int,int / BackendController รับ int,bool)
    // → ต้องตัดสินใจ signature ที่ถูกต้องก่อนแล้วค่อย connect กลับ

    // ปุ่ม EXPORT → คัดลอกไฟล์ไปยัง USB
    QObject::connect(&window,  &MainWindow::reqExportToUsb,
                     &backend, &BackendController::exportToUsb);

    window.show();
    backend.start(); // เปิดกล้องและเริ่มระบบ

    return app.exec();
}