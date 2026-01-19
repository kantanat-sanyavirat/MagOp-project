#include <QApplication>
#include <QThread>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>

// ไม่ต้องใส่ path ยาวๆ เพราะ CMake จัดการให้แล้ว
#include "camera_handler.h"
#include "ai_processing.h"

// -----------------------------------------------------------------
// [BACKEND] ฟังก์ชันบันทึกผลลัพธ์ (ใช้ Path แบบระบุเต็ม เพื่อความชัวร์)
// -----------------------------------------------------------------
void finalizePrintJob(const FrameResult& rawData, const QString& userEditedText) {
    
    // 1. ระบุตำแหน่งโฟลเดอร์ output โดยอ้างอิงจาก Home Directory ของ User
    // ผลลัพธ์จะได้ประมาณ: "/home/kantanat/MagOp-project/ai_output"
    QString homePath = QDir::homePath();
    QString folderName = homePath + "/MagOp-project/ai_output";
    
    // 2. สร้างโฟลเดอร์ถ้ายังไม่มี
    QDir dir(folderName);
    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            std::cout << ">> [INFO] Created output folder: " << folderName.toStdString() << std::endl;
        } else {
            std::cerr << ">> [ERROR] Could not create folder: " << folderName.toStdString() << std::endl;
            return; // หยุดทำงานถ้าสร้างโฟลเดอร์ไม่ได้
        }
    }

    // 3. ตั้งชื่อไฟล์ตามเวลา
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString imgName = QString("RESULT_%1.jpg").arg(timestamp);
    QString fullImgPath = QString("%1/%2").arg(folderName).arg(imgName);
    
    // 4. บันทึกรูปต้นฉบับ (Clean Image)
    if (cv::imwrite(fullImgPath.toStdString(), rawData.originalImage)) {
        
        // 5. บันทึกข้อมูล JSON (Overlay Data) คู่กัน
        QJsonObject root;
        root["image_file"] = imgName;
        root["timestamp"] = rawData.timestamp;

        QJsonObject overlay;
        overlay["text_content"] = userEditedText;
        
        // หาพิกัดที่จะวางข้อความ
        int x = 50, y = 50;
        if (!rawData.detections.empty()) {
            x = rawData.detections[0].boundingBox.x;
            y = rawData.detections[0].boundingBox.y - 10;
        }
        overlay["x"] = x;
        overlay["y"] = y;

        root["overlay"] = overlay;

        // เขียนไฟล์ .json
        QString jsonName = QString("RESULT_%1.json").arg(timestamp);
        QFile jsonFile(QString("%1/%2").arg(folderName).arg(jsonName));
        
        if (jsonFile.open(QIODevice::WriteOnly)) {
            jsonFile.write(QJsonDocument(root).toJson());
            jsonFile.close();
            
            // Log บอกตำแหน่งไฟล์ชัดเจน
            std::cout << ">> [SAVED] Success!" << std::endl;
            std::cout << "   IMG:  " << fullImgPath.toStdString() << std::endl;
        }
    } else {
        std::cerr << ">> [ERROR] Failed to save image!" << std::endl;
    }
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // ลงทะเบียน Struct เพื่อส่งข้าม Thread
    qRegisterMetaType<FrameResult>("FrameResult");

    std::cout << "--- MagOp System Ready ---" << std::endl;
    std::cout << "Output will be saved to: " << (QDir::homePath() + "/MagOp-project/ai_output").toStdString() << std::endl;
    std::cout << "[s] Scan & Save | [q] Quit" << std::endl;

    // --- Setup AI Thread ---
    QThread* aiThread = new QThread();
    AI_Processing* aiProcessor = new AI_Processing();
    aiProcessor->moveToThread(aiThread);
    aiThread->start();

    CameraHandler camera;

    // --- Connect Camera (Live View) ---
    QObject::connect(&camera, &CameraHandler::frameReady, [&](cv::Mat frame){
        cv::imshow("MagOp Live", frame);
        char key = (char)cv::waitKey(1);
        
        if (key == 'q') {
            app.quit();
        } 
        else if (key == 's') {
            std::cout << "\n>> [ACTION] Capture!" << std::endl;
            // ส่งเข้าคิว AI
            aiProcessor->addFrameToQueue(frame);
        }
    });

    // --- Connect AI Result (Backend Process) ---
    QObject::connect(aiProcessor, &AI_Processing::resultReady, aiProcessor, 
        [&](const FrameResult& data){
        
        QString rawText = data.detections.empty() ? "NONE" : data.detections[0].label;
        std::cout << "[AI Result] Found: " << rawText.toStdString() << std::endl;

        // จำลอง User แก้ไขข้อความ
        QString userText = rawText + "-VERIFIED";
        
        // บันทึกผลลัพธ์ลง Disk
        finalizePrintJob(data, userText);
    });

    camera.startCamera(0); // เปิดกล้อง

    int ret = app.exec();

    // Cleanup
    aiThread->quit();
    aiThread->wait();
    delete aiProcessor;
    delete aiThread;
    
    return ret;
}