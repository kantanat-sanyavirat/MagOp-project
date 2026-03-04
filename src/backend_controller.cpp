#include "backend_controller.h"
#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <iostream>
#include <QDir>

BackendController::BackendController(QObject *parent) : QObject(parent) {
    aiThread = new QThread();
    aiProcessor = new AI_Processing();
    aiProcessor->moveToThread(aiThread);
    aiThread->start();

    camera = new CameraHandler();

    connect(camera, &CameraHandler::frameReady, this, &BackendController::processCameraFrame);
    connect(aiProcessor, &AI_Processing::resultReady, this, &BackendController::handleAiResult);
    
    qDebug() << "Backend: Initialized.";
}

// *** ต้องมี Destructor ให้ตรงกับที่ประกาศไว้ ***
BackendController::~BackendController() {
    camera->stopCamera();
    if(aiThread->isRunning()) {
        aiThread->quit();
        aiThread->wait();
    }
    delete aiProcessor;
    delete aiThread;
    delete camera;
    qDebug() << "Backend: Destroyed.";
}

// *** เพิ่มฟังก์ชัน start() ที่หายไป ***
void BackendController::start() {
    qDebug() << "Backend: Starting camera...";
    camera->startCamera(0); 
}

void BackendController::processCameraFrame(cv::Mat frame) {
    if (frame.empty()) return;

    // ถ้าได้ภาพมาแล้ว ให้ลบข้อความ Error (ถ้ามี)
    static bool lastStateWasEmpty = false;
    if (lastStateWasEmpty) {
        emit statusMessage("Camera Reconnected");
        lastStateWasEmpty = false;
    }

    currentLiveFrame = frame.clone();
    emit frameReady(matToQImage(frame));
}

void BackendController::loadSavedImage(const QString &fileName) {
    QString filePath = QDir::homePath() + "/MagOp-project/ai_output/" + fileName;
    
    cv::Mat loadedMat = cv::imread(filePath.toStdString());
    
    if (!loadedMat.empty()) {
        m_lastSavedFile = fileName; // <--- เพิ่มบรรทัดนี้ เพื่อให้ลบรูปที่โหลดมาจาก History ได้
        
        currentEditedImage = loadedMat.clone();
        originalCvImage = loadedMat.clone();
        currentText = fileName;
        
        emit reviewReady(matToQImage(currentEditedImage), currentText);
        emit statusMessage("Viewing: " + fileName);
    } else {
        emit statusMessage("Load Failed!");
    }
}

void BackendController::exportToUsb(const QString &fileName) {
    QString sourcePath = QDir::homePath() + "/MagOp-project/ai_output/" + fileName;
    QString userName = qEnvironmentVariable("USER");
    if (userName.isEmpty()) userName = "kantanat"; 

    QDir mediaDir("/media/" + userName);
    QStringList drives = mediaDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    if (!drives.isEmpty()) {
        QString usbTargetPath = mediaDir.absoluteFilePath(drives.first()) + "/" + fileName;
        if (QFile::copy(sourcePath, usbTargetPath)) {
            emit statusMessage("Export to USB Success!");
        } else {
            emit statusMessage("USB Error or File Exists!");
        }
    } else {
        emit statusMessage("No USB Found!");
    }
}

void BackendController::capture() {
    if (!currentLiveFrame.empty()) {
        emit statusMessage("Processing AI...");
        aiProcessor->addFrameToQueue(currentLiveFrame);
    } else {
        emit statusMessage("Camera not ready!");
    }
}

void BackendController::handleAiResult(FrameResult result) {
    originalCvImage = result.originalImage.clone(); 
    currentEditedImage = result.originalImage.clone(); 
    currentTimestamp = result.timestamp;
    
    if (result.detections.empty()) {
        currentText = "No Object";
        currentX = 50; // กำหนดค่าเริ่มต้นถ้าไม่เจอวัตถุ
        currentY = 50;
    } else {
        currentText = result.detections[0].label;
        // --- เพิ่ม 2 บรรทัดนี้ ---
        currentX = result.detections[0].boundingBox.x;
        currentY = result.detections[0].boundingBox.y;
    }
    emit reviewReady(matToQImage(currentEditedImage), currentText);
}

void BackendController::adjustImage(int brightnessStep, bool denoise) {
    if (originalCvImage.empty()) return;
    cv::Mat temp;
    originalCvImage.convertTo(temp, -1, 1.0, brightnessStep);
    if (denoise) cv::GaussianBlur(temp, temp, cv::Size(3, 3), 0);
    currentEditedImage = temp;
    emit reviewReady(matToQImage(currentEditedImage), currentText); 
}

// *** เพิ่มฟังก์ชัน save(QString) ให้ตรงกับ Slot ใน Header ***
void BackendController::save(QString userText) {
    currentText = userText;
    saveToDiskAndUsb();
    emit statusMessage("Saved Successfully");
    qDebug() << "Backend: Data saved.";
}

void BackendController::discard() {
    if (m_lastSavedFile.isEmpty()) {
        emit statusMessage("No file to discard.");
        return;
    }

    QString filePath = QDir::homePath() + "/MagOp-project/ai_output/" + m_lastSavedFile;

    if (QFile::exists(filePath)) {
        if (QFile::remove(filePath)) {
            emit statusMessage("Deleted: " + m_lastSavedFile);
            m_lastSavedFile = ""; 
        } else {
            emit statusMessage("Delete Failed!");
        }
    }
}

void BackendController::refreshFileList() {
    QString folderPath = QDir::homePath() + "/MagOp-project/ai_output";
    QDir dir(folderPath);
    QStringList filters; filters << "*.jpg";
    dir.setNameFilters(filters);
    
    // เรียงลำดับตามเวลา: ไฟล์ใหม่สุดอยู่บน ไฟล์เก่าสุดอยู่ท้าย
    dir.setSorting(QDir::Time); 
    QStringList files = dir.entryList();

    // --- ระบบ Auto-Cleanup: จำกัดไว้ที่ 50 รูป ---
    int maxFiles = 20; 
    while (files.size() > maxFiles) {
        // ลบไฟล์ที่เก่าที่สุด (ไฟล์สุดท้ายในรายการที่เรียงตามเวลา)
        QString oldFile = files.takeLast(); 
        if (QFile::exists(folderPath + "/" + oldFile)) {
            QFile::remove(folderPath + "/" + oldFile);
        }
        qDebug() << "Auto-cleanup: Deleted oldest image:" << oldFile;
    }

    emit fileListUpdated(files);
}

void BackendController::deleteFile(const QString &fileName) {
    QString folderPath = QDir::homePath() + "/MagOp-project/ai_output";
    
    // ลบไฟล์ภาพต้นทาง
    if (QFile::remove(folderPath + "/" + fileName)) {
        qDebug() << "Backend: Deleted file" << fileName;
    }
    
    refreshFileList(); // อัปเดตรายการหลังลบ
}

void BackendController::saveToDiskAndUsb() {
    QString homePath = QDir::homePath();
    QString localFolder = homePath + "/MagOp-project/ai_output";
    QDir().mkpath(localFolder);

    QString fileTimestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString imgName = QString("RESULT_%1.jpg").arg(fileTimestamp);
    QString localImgPath = localFolder + "/" + imgName;
    m_lastSavedFile = imgName;

    // --- ส่วนการวาด Overlay ---
    cv::Mat overlayImage = currentEditedImage.clone();
    cv::Scalar color(0, 255, 0);
    cv::rectangle(overlayImage, cv::Rect(currentX, currentY, 200, 150), color, 2);
    cv::putText(overlayImage, currentText.toStdString(), 
                cv::Point(currentX, currentY - 10), 
                cv::FONT_HERSHEY_SIMPLEX, 0.8, color, 2);

    // เซฟลงเครื่องก่อน
    if (!cv::imwrite(localImgPath.toStdString(), overlayImage)) {
        qDebug() << "Failed to save local image!";
        return;
    }

    // --- ส่วนการหา USB ของจริง (ปรับปรุงใหม่) ---
    QString userName = qEnvironmentVariable("USER"); 
    if (userName.isEmpty()) userName = qEnvironmentVariable("USERNAME"); // เผื่อเคสอื่นๆ

    QDir mediaDir("/media/" + userName);
    // ดึงรายชื่อ Directory ทั้งหมดใน /media/user/
    QStringList drives = mediaDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    bool usbSaved = false;
    QString foundDriveName = "";

    for (const QString &drive : drives) {
        QString usbMountPath = mediaDir.absoluteFilePath(drive);
        
        // เช็กว่า Directory นี้ "เขียนได้จริง" หรือไม่ (เพื่อตัดพวก System Folder ออก)
        QFileInfo driveInfo(usbMountPath);
        if (driveInfo.isWritable()) {
            QString targetPath = usbMountPath + "/" + imgName;
            
            // ถ้าคัดลอกสำเร็จให้หยุดลูป
            if (QFile::copy(localImgPath, targetPath)) {
                usbSaved = true;
                foundDriveName = drive;
                qDebug() << "Success: Copied to USB at" << targetPath;
                break; 
            }
        }
    }

    if (usbSaved) {
        emit statusMessage("Saved to Local & USB: " + foundDriveName);
    } else {
        if (drives.isEmpty()) {
            emit statusMessage("Local Saved. (No USB found)");
            qDebug() << "USB Error: No drive detected in /media/" << userName;
        } else {
            emit statusMessage("Local Saved, but USB Write Denied!");
            qDebug() << "USB Error: Found drives but none are writable or copy failed.";
        }
    }

    refreshFileList();
}

QImage BackendController::matToQImage(const cv::Mat &mat) {
    if(mat.empty()) return QImage();
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage((const unsigned char*)(rgb.data), rgb.cols, rgb.rows, rgb.step, QImage::Format_RGB888).copy();
}