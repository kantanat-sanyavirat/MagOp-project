#include <QApplication>
#include <QThread>
#include <QDir>
#include <QDateTime>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include "camera_handler.h"
#include "ai_processing.h" 

// --- ฟังก์ชันช่วยบันทึกไฟล์ (เหมือนเดิม) ---
void saveResultToDisk(const cv::Mat& image, const QString& infoText) {
    QString folderName = "ai_output";
    QDir dir(folderName);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString filename = QString("%1/RESULT_%2.jpg").arg(folderName).arg(timestamp);

    bool success = cv::imwrite(filename.toStdString(), image);

    if (success) {
        std::cout << ">> [SAVED] " << filename.toStdString() 
                  << " | Info: " << infoText.toStdString() << std::endl;
    } else {
        std::cerr << ">> [ERROR] Failed to save image to disk!" << std::endl;
    }
}

// --- ฟังก์ชันเช็คระบบ (เหมือนเดิม) ---
int performSystemCheck() {
    // ... (ละไว้ฐานที่เข้าใจ) ...
    return 0;
}

// --- ฟังก์ชันหลัก ---
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // [แก้จุดที่ 1] ลงทะเบียน Struct ให้ Qt รู้จัก (สำคัญมากเวลาส่งข้าม Thread!)
    qRegisterMetaType<DetectionResult>("DetectionResult");

    std::cout << "System Ready! Press 's' to Scan, 'q' to Quit." << std::endl;

    // --- SETUP THREAD SYSTEM ---
    QThread* aiThread = new QThread();
    AI_Processing* aiProcessor = new AI_Processing();
    
    aiProcessor->moveToThread(aiThread);
    aiThread->start();
    // ---------------------------

    CameraHandler camera;

    // 1. รับภาพจากกล้อง
    QObject::connect(&camera, &CameraHandler::frameReady, [&](cv::Mat frame){
        cv::imshow("Live Camera", frame);
        char key = (char)cv::waitKey(1);

        if (key == 'q') {
            app.quit();
        } 
        else if (key == 's') {
            std::cout << ">> [QUEUE] Sending frame to AI..." << std::endl;
            aiProcessor->addFrameToQueue(frame);
        }
    });

    // 2. รับผลลัพธ์จาก AI (ต้องแก้ตรงนี้!)
    // [แก้จุดที่ 2] เปลี่ยนตัวรับให้เป็น DetectionResult
    QObject::connect(aiProcessor, &AI_Processing::resultReady, aiProcessor, [&](const DetectionResult& data){
        
        // เราได้กล่อง data มาแล้ว ก็แกะของข้างในออกมาใช้งาน
        std::cout << ">> Received Data: " << data.detectedText.toStdString() 
                  << " [Conf: " << data.confidence << "]" << std::endl;

        // เรียกใช้ฟังก์ชัน Save โดยส่ง "รูปต้นฉบับ" และ "ข้อความ" เข้าไป
        saveResultToDisk(data.originalImage, data.detectedText);

        // (แถม) ถ้าอยากเห็นว่ามันเจอตรงไหน ลองปรินท์พิกัดออกมาดูเล่นๆ
        std::cout << "   Bounding Box: " << data.boundingBox.x << "," << data.boundingBox.y 
                  << " (" << data.boundingBox.width << "x" << data.boundingBox.height << ")" << std::endl;
    });

    camera.startCamera(0);

    int ret = app.exec();

    // Cleanup
    aiThread->quit();
    aiThread->wait();
    delete aiProcessor;
    delete aiThread;
    cv::destroyAllWindows();
    
    return ret;
}