#include <QApplication>
#include "backend_controller.h"
#include "mainwindow.h" // อย่าลืมไฟล์ MainWindow ที่ให้ไปรอบก่อนนะครับ

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // 1. สร้างส่วนประกอบ (Model & View)
    BackendController backend;
    MainWindow window;

    // 2. เชื่อมสายสัญญาณ (Controller Wiring)
    
    // --- Backend ส่งข้อมูล -> ไปโชว์ที่ Frontend ---
    QObject::connect(&backend, &BackendController::frameReady,    &window, &MainWindow::updateLiveView);
    QObject::connect(&backend, &BackendController::reviewReady,   &window, &MainWindow::showReviewMode);
    QObject::connect(&backend, &BackendController::statusMessage, &window, &MainWindow::showMessage);

    // --- Frontend กดปุ่ม -> ไปสั่งงาน Backend ---
    QObject::connect(&window, &MainWindow::reqCapture, &backend, &BackendController::capture);
    QObject::connect(&window, &MainWindow::reqSave,    &backend, &BackendController::save);
    QObject::connect(&window, &MainWindow::reqDiscard, &backend, &BackendController::discard);
    QObject::connect(&window, &MainWindow::reqAdjust,  &backend, &BackendController::adjustImage);

    // 3. เริ่มระบบ
    window.resize(800, 600); // กำหนดขนาดหน้าจอเริ่มต้น
    window.show();           // แสดงหน้าต่าง
    backend.start();         // สั่งเปิดกล้อง

    return app.exec(); // เข้าสู่ Main Loop ของ Qt
}