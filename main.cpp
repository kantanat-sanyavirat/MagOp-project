#include <QApplication>
#include <QThread>
#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfoList>
#include <iostream>
#include <string>

#include "camera_handler.h"
#include "ai_processing.h"

// สถานะของแอพพลิเคชัน
enum class AppMode {
    LIVE,       // โหมดกล้องสด
    PROCESSING, // กำลังคิด...
    REVIEW      // โหมดตรวจสอบรูป
};

// =========================================================
// [HELPER] Visual Utils
// =========================================================
namespace VisualUtils {
    cv::Mat drawOverlay(const cv::Mat& source, const QString& text, int x, int y) {
        // ถ้ารูปใหญ่ไป ย่อลงมาแสดงผล (Performance)
        cv::Mat display;
        float scale = 1.0f;
        if (source.cols > 800) {
            float ratio = 800.0f / source.cols;
            cv::resize(source, display, cv::Size(), ratio, ratio);
            scale = ratio;
            x = (int)(x * ratio);
            y = (int)(y * ratio);
        } else {
            display = source.clone();
        }

        if (text.isEmpty()) return display;

        cv::Size textSize = cv::getTextSize(text.toStdString(), cv::FONT_HERSHEY_SIMPLEX, 0.7, 2, 0);
        
        // วาดกรอบข้อความ
        cv::rectangle(display, 
                      cv::Point(x, y - textSize.height - 10), 
                      cv::Point(x + textSize.width, y), 
                      cv::Scalar(0, 255, 0), cv::FILLED);

        // วาดตัวหนังสือ
        cv::putText(display, text.toStdString(), 
                    cv::Point(x, y - 5), 
                    cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 0, 0), 2);
        
        // วาดเมนูช่วยเหลือด้านล่าง
        cv::putText(display, "[1]Save [2]Light+ [3]Dark- [4]Denoise [0]Discard", 
                    cv::Point(10, display.rows - 20), 
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);

        return display;
    }
}

// =========================================================
// [MODULE] Image Utils
// =========================================================
namespace ImageUtils {
    cv::Mat adjustBrightness(const cv::Mat& source, double alpha, int beta) {
        cv::Mat newImage;
        source.convertTo(newImage, -1, alpha, beta);
        return newImage;
    }
    cv::Mat denoise(const cv::Mat& source) {
        cv::Mat newImage;
        cv::GaussianBlur(source, newImage, cv::Size(3, 3), 0);
        return newImage;
    }
}

// =========================================================
// [MODULE] File Manager
// =========================================================
namespace FileManager {
    QString getLocalPath() { return QDir::homePath() + "/MagOp-project/ai_output"; }
    QString getUsbPath()   { return QDir::homePath() + "/MagOp-project/Fake_FlashDrive"; }

    void deleteSpecificFile(const QString& filename) {
        QString local = getLocalPath();
        QString usb = getUsbPath();
        QFile::remove(local + "/" + filename);
        QFile::remove(usb + "/" + filename);
        QString jsonName = filename;
        jsonName.replace(".jpg", ".json");
        QFile::remove(local + "/" + jsonName);
        QFile::remove(usb + "/" + jsonName);
        std::cout << "   [DELETED] " << filename.toStdString() << std::endl;
    }

    void clearAllFiles() {
        QString paths[] = { getLocalPath(), getUsbPath() };
        for (const QString& path : paths) {
            QDir dir(path);
            if (!dir.exists()) continue;
            dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
            for (const QFileInfo& file : dir.entryInfoList()) {
                dir.remove(file.fileName());
            }
        }
        std::cout << "   -> Reset Done." << std::endl;
    }

    void autoCleanup(int maxFiles) {
        QDir dir(getLocalPath());
        dir.setFilter(QDir::Files | QDir::NoDotAndDotDot);
        dir.setSorting(QDir::Time | QDir::Reversed);
        QFileInfoList list = dir.entryInfoList();
        int jpgCount = 0;
        for(auto& f : list) if(f.fileName().endsWith(".jpg")) jpgCount++;
        if (jpgCount > maxFiles) {
            for (const QFileInfo& file : list) {
                if (file.fileName().endsWith(".jpg")) {
                    deleteSpecificFile(file.fileName());
                    jpgCount--;
                    if (jpgCount <= maxFiles) break;
                }
            }
        }
    }

    void listFiles() {
        QDir dir(getLocalPath());
        QStringList filters; filters << "*.jpg";
        dir.setNameFilters(filters);
        dir.setSorting(QDir::Time | QDir::Reversed);
        std::cout << "\n--- GALLERY ---" << std::endl;
        QFileInfoList list = dir.entryInfoList();
        if (list.empty()) std::cout << "(Empty)" << std::endl;
        else {
            for (const QFileInfo& file : list) {
                std::cout << " [FILE] " << file.fileName().toStdString() << std::endl;
            }
        }
    }
}

// =========================================================
// [CORE] Save System
// =========================================================
void saveAndExport(const cv::Mat& finalImage, const QString& finalText, const QString& timestampOriginal, int x, int y) {
    QString localFolder = FileManager::getLocalPath();
    QString usbFolder = FileManager::getUsbPath();
    QDir().mkpath(localFolder);
    QDir().mkpath(usbFolder); 
    
    FileManager::autoCleanup(50); 

    QString fileTimestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString imgName = QString("RESULT_%1.jpg").arg(fileTimestamp);
    QString jsonName = QString("RESULT_%1.json").arg(fileTimestamp);

    QString localImgPath = localFolder + "/" + imgName;
    cv::imwrite(localImgPath.toStdString(), finalImage);

    QString localJsonPath = localFolder + "/" + jsonName;
    QJsonObject root;
    root["image_file"] = imgName;
    root["timestamp"] = timestampOriginal;

    QJsonObject overlay;
    overlay["text_content"] = finalText;
    overlay["x"] = x;
    overlay["y"] = y;
    root["overlay"] = overlay;

    QFile jsonFile(localJsonPath);
    if (jsonFile.open(QIODevice::WriteOnly)) {
        jsonFile.write(QJsonDocument(root).toJson());
        jsonFile.close();
    }

    QFile::copy(localImgPath, usbFolder + "/" + imgName);
    QFile::copy(localJsonPath, usbFolder + "/" + jsonName);
    std::cout << "   -> Saved: " << imgName.toStdString() << std::endl;
}

// =========================================================
// [MAIN LOOP]
// =========================================================
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    qRegisterMetaType<FrameResult>("FrameResult");

    std::cout << "==========================================" << std::endl;
    std::cout << "    MagOp System (Single Window Mode)     " << std::endl;
    std::cout << "==========================================" << std::endl;
    std::cout << " [s] SCAN | [f] File Manager | [q] Quit" << std::endl;

    QThread* aiThread = new QThread();
    AI_Processing* aiProcessor = new AI_Processing();
    aiProcessor->moveToThread(aiThread);
    aiThread->start();

    CameraHandler camera;

    // -- STATE VARIABLES (ตัวแปรเก็บสถานะ) --
    AppMode currentMode = AppMode::LIVE;
    
    // ตัวแปรสำหรับ Review Mode
    cv::Mat reviewImage;
    QString reviewText;
    QString reviewTimestamp;
    int reviewX = 0, reviewY = 0;

    // เชื่อมต่อ AI Result
    // เมื่อ AI เสร็จ -> เปลี่ยนโหมดเป็น REVIEW และเก็บค่าไว้
    QObject::connect(aiProcessor, &AI_Processing::resultReady, aiProcessor, 
        [&](const FrameResult& resultData){
            reviewImage = resultData.originalImage.clone();
            reviewText = resultData.detections.empty() ? "" : resultData.detections[0].label;
            reviewTimestamp = resultData.timestamp;
            
            reviewX = 50; reviewY = 50;
            if (!resultData.detections.empty()) {
                reviewX = resultData.detections[0].boundingBox.x;
                reviewY = resultData.detections[0].boundingBox.y - 10;
            }

            currentMode = AppMode::REVIEW; // **เปลี่ยนสถานะ**
            std::cout << ">> Review Mode: Press [1]Save [0]Discard" << std::endl;
    });

    // เชื่อมต่อกล้อง (Main Loop 30 FPS)
    QObject::connect(&camera, &CameraHandler::frameReady, [&](cv::Mat frame){
        
        cv::Mat displayFrame;

        // --- SWITCH LOGIC ตามสถานะ ---
        if (currentMode == AppMode::LIVE) {
            // [LIVE] แสดงภาพจากกล้อง
            if (frame.cols > 640) cv::resize(frame, displayFrame, cv::Size(640, 480));
            else displayFrame = frame;
            
            cv::putText(displayFrame, "LIVE MODE (Press 's')", cv::Point(10, 30), 
                        cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
        }
        else if (currentMode == AppMode::PROCESSING) {
            // [PROCESSING] แสดงภาพค้าง หรือข้อความ
            displayFrame = cv::Mat::zeros(cv::Size(640, 480), CV_8UC3);
            cv::putText(displayFrame, "Processing...", cv::Point(200, 240), 
                        cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
        }
        else if (currentMode == AppMode::REVIEW) {
            // [REVIEW] แสดงรูปที่ถ่ายไว้ (reviewImage)
            // ใช้ฟังก์ชันวาด Overlay ทับลงไป
            displayFrame = VisualUtils::drawOverlay(reviewImage, reviewText, reviewX, reviewY);
        }

        // แสดงผลหน้าต่างเดียว (Single Window)
        cv::imshow("MagOp System", displayFrame);
        
        // รับปุ่มกด (30ms delay)
        int key = cv::waitKey(30);

        // --- INPUT HANDLING ตามสถานะ ---
        if (key == 'q') {
            app.quit();
        }

        if (currentMode == AppMode::LIVE) {
            if (key == 's') { 
                currentMode = AppMode::PROCESSING; // เปลี่ยนสถานะเป็นรอ
                aiProcessor->addFrameToQueue(frame);
            }
            else if (key == 'f') {
                FileManager::listFiles();
            }
            else if (key == 'd') {
                std::cout << ">> Reset? (y/n): ";
                std::string c; std::cin >> c;
                if(c == "y") FileManager::clearAllFiles();
            }
        }
        else if (currentMode == AppMode::REVIEW) {
            if (key == '1') { // SAVE
                saveAndExport(reviewImage, reviewText, reviewTimestamp, reviewX, reviewY);
                currentMode = AppMode::LIVE; // กลับไป Live
                std::cout << ">> Saved. Back to Live." << std::endl;
            }
            else if (key == '0') { // DISCARD
                currentMode = AppMode::LIVE; // กลับไป Live ทันที
                std::cout << ">> Discarded. Back to Live." << std::endl;
            }
            else if (key == '2') { // Light +
                reviewImage = ImageUtils::adjustBrightness(reviewImage, 1.1, 20);
            }
            else if (key == '3') { // Dark -
                reviewImage = ImageUtils::adjustBrightness(reviewImage, 0.9, -20);
            }
            else if (key == '4') { // Denoise
                reviewImage = ImageUtils::denoise(reviewImage);
            }
            else if (key == '5') { // Edit Text
                std::cout << ">> Type text: ";
                std::cin.clear();
                std::string t; std::cin >> t;
                reviewText = QString::fromStdString(t);
            }
        }
    });

    camera.startCamera(0);
    int ret = app.exec();

    aiThread->quit(); aiThread->wait();
    delete aiProcessor; delete aiThread;
    
    return ret;
}