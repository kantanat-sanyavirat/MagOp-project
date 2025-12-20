#include <QApplication>
#include <QThread>      
#include <QDir>         
#include <QDateTime>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include "camera_handler.h"
#include "ai_processing.h"

void saveResultToDisk(const cv::Mat& image, const QString& infoText) {
    // 1. ตั้งชื่อโฟลเดอร์
    QString folderName = "ai_output";

    // 2. สร้างโฟลเดอร์ (ถ้ายังไม่มี)
    QDir dir(folderName);
    if (!dir.exists()) {
        dir.mkpath("."); // mkpath ดีกว่า mkdir ตรงที่สร้างโฟลเดอร์ซ้อนชั้นได้
    }

    // 3. ตั้งชื่อไฟล์: ai_output/RESULT_YYYYMMDD_HHmmss_zzz.jpg
    // ใช้ Millisecond (zzz) เพื่อกันชื่อซ้ำเวลารัวชัตเตอร์
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString filename = QString("%1/RESULT_%2.jpg").arg(folderName).arg(timestamp);

    // 4. สั่งบันทึก
    bool success = cv::imwrite(filename.toStdString(), image);

    if (success) {
        std::cout << ">> [SAVED] " << filename.toStdString() 
                  << " | Info: " << infoText.toStdString() << std::endl;
    } else {
        std::cerr << ">> [ERROR] Failed to save image to disk!" << std::endl;
    }
}

// --- ฟังก์ชันเช็คระบบ ---
int performSystemCheck() {
    std::cout << "----------------------------------------" << std::endl;
    std::cout << "MagOp System: Starting Initialization..." << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    bool success = true;

    // 1. เช็ค OpenCV
    std::cout << "[Check 1] OpenCV Version: " << CV_VERSION << " ... ";
    try {
        cv::Mat test = cv::Mat::zeros(10, 10, CV_8UC1);
        std::cout << "PASSED" << std::endl;
    } catch (...) {
        std::cerr << "FAILED" << std::endl;
        success = false;
    }

    // 2. เช็ค ONNX Runtime
    std::cout << "[Check 2] ONNX Runtime ... ";
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_ERROR, "MagOp_Check");
        Ort::SessionOptions options;
        std::cout << "PASSED" << std::endl;
    } catch (std::exception &e) {
        std::cerr << "FAILED (" << e.what() << ")" << std::endl;
        success = false;
    }

    std::cout << "----------------------------------------" << std::endl;
    return success ? 0 : -1;
}

// --- ฟังก์ชันหลัก ---
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    std::cout << "System Ready! Press 's' to Scan, 'q' to Quit." << std::endl;

    // --- SETUP THREAD SYSTEM ---
    QThread* aiThread = new QThread();
    AI_Processing* aiProcessor = new AI_Processing();
    
    aiProcessor->moveToThread(aiThread);
    aiThread->start();
    // ---------------------------

    CameraHandler camera;

    // 1. รับภาพจากกล้อง (Main Thread)
    QObject::connect(&camera, &CameraHandler::frameReady, [&](cv::Mat frame){
        cv::imshow("Live Camera", frame);
        
        // ใช้ waitKey(1) เพื่อให้หน้าต่างอัปเดตและรับปุ่มกด
        char key = (char)cv::waitKey(1);

        if (key == 'q') {
            app.quit();
        } 
        else if (key == 's') {
            std::cout << ">> [QUEUE] Sending frame to AI..." << std::endl;
            // ส่งเข้าคิวประมวลผล (AI จะไปทำงานอยู่เบื้องหลัง)
            aiProcessor->addFrameToQueue(frame);
        }
    });

    // 2. รับผลลัพธ์จาก AI (Main Thread) -> แล้วสั่ง Save
    // ใช้ aiProcessor เป็น Context object (ตัวที่ 3) เพื่อความปลอดภัย
    QObject::connect(aiProcessor, &AI_Processing::resultReady, aiProcessor, [&](cv::Mat resultImg, QString text){
        
        // เรียกใช้ฟังก์ชัน Save ที่เราเขียนไว้ข้างบน
        saveResultToDisk(resultImg, text);
    });

    camera.startCamera(0);

    int ret = app.exec();

    // --- Cleanup (เก็บกวาดให้เรียบร้อย) ---
    aiThread->quit();
    aiThread->wait();
    delete aiProcessor;
    delete aiThread;
    cv::destroyAllWindows();
    
    return ret;
}