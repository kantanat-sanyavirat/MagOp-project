#include <QApplication>
#include "backend_controller.h"
#include "ui/mainwindow.h"
#include "ai_processing.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // [สำคัญมาก] ลงทะเบียนให้ Qt รู้จักการส่งข้อมูลข้าม Thread
    qRegisterMetaType<cv::Mat>("cv::Mat");
    qRegisterMetaType<FrameResult>("FrameResult");
    qDebug() << "System: MetaTypes registered.";

    BackendController backend;
    MainWindow window;

    // เชื่อมต่อ Signals/Slots (ตรวจสอบชื่อให้ตรงกับใน Class)
    QObject::connect(&backend, &BackendController::frameReady,    &window, &MainWindow::updateLiveView);
    QObject::connect(&backend, &BackendController::reviewReady,   &window, &MainWindow::showReviewMode);
    QObject::connect(&backend, &BackendController::statusMessage, &window, &MainWindow::showMessage);

    QObject::connect(&window, &MainWindow::reqCapture, &backend, &BackendController::capture);
    QObject::connect(&window, &MainWindow::reqSave,    &backend, &BackendController::save);
    QObject::connect(&window, &MainWindow::reqDiscard, &backend, &BackendController::discard);
    QObject::connect(&window, &MainWindow::reqAdjust,  &backend, &BackendController::adjustImage);
    QObject::connect(&window, &MainWindow::reqExportToUsb, &backend, &BackendController::exportToUsb);

    window.show();
    backend.start(); 

    return app.exec();
}