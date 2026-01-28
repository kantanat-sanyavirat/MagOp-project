#include <QApplication>
#include <QThread>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <iostream>
#include <string>

#include "camera_handler.h"
#include "ai_processing.h"

// =========================================================
// [MODULE] Image Processing Tools (เครื่องมือแต่งภาพ)
// =========================================================
namespace ImageUtils {
    // ปรับความสว่าง (beta) และ contrast (alpha)
    cv::Mat adjustBrightness(const cv::Mat& source, double alpha, int beta) {
        cv::Mat newImage;
        source.convertTo(newImage, -1, alpha, beta);
        return newImage;
    }

    // ลด Noise
    cv::Mat denoise(const cv::Mat& source) {
        cv::Mat newImage;
        cv::GaussianBlur(source, newImage, cv::Size(3, 3), 0);
        return newImage;
    }
}

// =========================================================
// [MODULE] Export Manager (จัดการการบันทึกและส่งออก)
// =========================================================
void saveAndExport(const cv::Mat& finalImage, const QString& finalText, const QString& timestamp) {
    
    QString homePath = QDir::homePath();
    
    // 1. กำหนด Path ภายในเครื่อง (Local Storage)
    QString localFolder = homePath + "/MagOp-project/ai_output";
    QDir().mkpath(localFolder);

    // 2. กำหนด Path ของ Flash Drive (จำลอง)
    // ในเครื่องจริงจะเป็น "/media/pi/YOUR_USB_NAME"
    QString usbFolder = homePath + "/MagOp-project/Fake_FlashDrive"; 
    QDir().mkpath(usbFolder); 

    // --- ตั้งชื่อไฟล์ ---
    QString imgName = QString("IMG_%1.jpg").arg(timestamp);
    QString jsonName = QString("DATA_%1.json").arg(timestamp);

    // --- A. บันทึกรูปลงเครื่อง ---
    QString localImgPath = localFolder + "/" + imgName;
    cv::imwrite(localImgPath.toStdString(), finalImage);

    // --- B. บันทึก JSON ลงเครื่อง ---
    QString localJsonPath = localFolder + "/" + jsonName;
    QJsonObject root;
    root["image_file"] = imgName;
    root["text_content"] = finalText;
    root["timestamp"] = timestamp;

    QFile jsonFile(localJsonPath);
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(QJsonDocument(root).toJson());
        jsonFile.close();
    }

    // --- C. ส่งออกไป Flash Drive (Copy Files) ---
    std::cout << ">> [EXPORT] Copying to Flash Drive..." << std::endl;
    
    QFile::copy(localImgPath, usbFolder + "/" + imgName);
    QFile::copy(localJsonPath, usbFolder + "/" + jsonName);

    std::cout << "   -> Local: " << localFolder.toStdString() << std::endl;
    std::cout << "   -> USB:   " << usbFolder.toStdString() << std::endl;
    std::cout << "   -> Status: EXPORT COMPLETE ✅" << std::endl;
}

// =========================================================
// [MAIN]
// =========================================================
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    qRegisterMetaType<FrameResult>("FrameResult");

    std::cout << "--- MagOp Interactive System ---" << std::endl;
    std::cout << "[s] Scan | [q] Quit" << std::endl;

    QThread* aiThread = new QThread();
    AI_Processing* aiProcessor = new AI_Processing();
    aiProcessor->moveToThread(aiThread);
    aiThread->start();

    CameraHandler camera;

    // Connect Camera
    QObject::connect(&camera, &CameraHandler::frameReady, [&](cv::Mat frame){
        cv::imshow("Live View", frame);
        char key = (char)cv::waitKey(1);
        if (key == 'q') app.quit();
        else if (key == 's') {
            std::cout << "\n>> [ACTION] Capturing & Processing..." << std::endl;
            aiProcessor->addFrameToQueue(frame);
        }
    });

    // Connect AI Result -> เข้าสู่โหมด Review
    QObject::connect(aiProcessor, &AI_Processing::resultReady, aiProcessor, 
        [&](const FrameResult& resultData){
        
        // 1. เตรียมข้อมูลเริ่มต้น (Draft)
        cv::Mat currentImage = resultData.originalImage.clone();
        QString currentText = resultData.detections.empty() ? "" : resultData.detections[0].label;
        
        bool confirmed = false;

        // 2. เข้าลูป "หน้าจอ UI Review" (จำลองด้วย while loop ใน console)
        while (!confirmed) {
            std::cout << "\n---------------------------------" << std::endl;
            std::cout << "      REVIEW RESULT (UI)         " << std::endl;
            std::cout << "---------------------------------" << std::endl;
            std::cout << "Text from AI: [" << currentText.toStdString() << "]" << std::endl;
            std::cout << "---------------------------------" << std::endl;
            std::cout << " [1] OK -> Save & Export to USB" << std::endl;
            std::cout << " [2] Edit: Brightness (+Light)" << std::endl;
            std::cout << " [3] Edit: Denoise (Fix Grain)" << std::endl;
            std::cout << " [4] Edit: Manual Text Input" << std::endl;
            std::cout << " [0] Discard (Cancel)" << std::endl;
            std::cout << "Select option: ";

            int choice;
            std::cin >> choice; // รอ User พิมพ์เลือก (จำลองการกดปุ่ม)

            if (choice == 1) {
                // ยืนยัน -> บันทึก
                saveAndExport(currentImage, currentText, resultData.timestamp);
                confirmed = true;
            }
            else if (choice == 2) {
                // ปรับแสง
                std::cout << ">> Adjusting brightness..." << std::endl;
                currentImage = ImageUtils::adjustBrightness(currentImage, 1.2, 30); // เพิ่มแสง
                cv::imshow("Preview Edit", currentImage); // โชว์รูปที่แก้แล้ว
                cv::waitKey(500);
            }
            else if (choice == 3) {
                // ลด Noise
                std::cout << ">> Reducing noise..." << std::endl;
                currentImage = ImageUtils::denoise(currentImage);
                cv::imshow("Preview Edit", currentImage);
                cv::waitKey(500);
            }
            else if (choice == 4) {
                // พิมพ์เอง
                std::cout << "Enter new text: ";
                std::string input;
                std::cin >> input;
                currentText = QString::fromStdString(input);
            }
            else if (choice == 0) {
                std::cout << ">> Discarded." << std::endl;
                confirmed = true; // ออกจากลูปโดยไม่บันทึก
            }
        }
        
        cv::destroyWindow("Preview Edit"); // ปิดหน้าต่างพรีวิว (ถ้ามี)
        std::cout << "\n>> Ready for next scan..." << std::endl;
    });

    camera.startCamera(0);
    int ret = app.exec();

    aiThread->quit();
    aiThread->wait();
    delete aiProcessor;
    delete aiThread;
    
    return ret;
}