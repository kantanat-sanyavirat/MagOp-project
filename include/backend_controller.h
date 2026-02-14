#ifndef BACKEND_CONTROLLER_H
#define BACKEND_CONTROLLER_H

#include <QObject>
#include <QImage>
#include <QDir>
#include <QJsonObject>
#include <opencv2/opencv.hpp>
#include "ai_processing.h"
#include "camera_handler.h"

// Class นี้จะเป็น "สมอง" ของระบบที่ Frontend จะมาคุยด้วย
class BackendController : public QObject
{
    Q_OBJECT
public:
    explicit BackendController(QObject *parent = nullptr);
    ~BackendController();

    // เริ่มระบบ
    void startSystem();

public slots:
    // --- คำสั่งที่รับจากปุ่มบนหน้าจอ (Frontend -> Backend) ---
    void captureImage();                    // ปุ่ม Scan
    void confirmSave();                     // ปุ่ม Save
    void discardImage();                    // ปุ่ม Discard
    
    // ปุ่มปรับแต่งภาพ
    void adjustImage(double brightness, double contrast, bool denoise);
    
    // แก้ไขข้อความ
    void updateText(const QString &newText);

    // --- ส่วนจัดการไฟล์ (Delete One by One) ---
    void refreshFileList();                 // ขอรายชื่อไฟล์ทั้งหมด
    void deleteFile(const QString &fileName); // สั่งลบไฟล์ระบุชื่อ (เช่น "RESULT_2025...jpg")

signals:
    // --- สัญญาณส่งกลับไปบอกหน้าจอ (Backend -> Frontend) ---
    void liveFrameReady(QImage img);        // ส่งภาพสดไปโชว์
    void reviewReady(QImage img, QString text); // เข้าโหมด Review พร้อมรูปและข้อความ
    void fileListUpdated(QStringList files); // ส่งรายชื่อไฟล์กลับไปให้ List View
    void statusMessage(QString msg);        // ส่งข้อความแจ้งเตือน (เช่น "Saved!", "Deleted!")

private slots:
    void processCameraFrame(cv::Mat frame);
    void handleAiResult(FrameResult result);

private:
    CameraHandler *camera;
    QThread *aiThread;
    AI_Processing *aiProcessor;

    // เก็บสถานะปัจจุบันระหว่างรอ User ยืนยัน
    cv::Mat currentCvImage;     // รูปต้นฉบับ (OpenCV)
    cv::Mat currentEditedImage; // รูปที่แต่งแล้ว
    QString currentText;        // ข้อความ
    QString currentTimestamp;   // เวลา
    int currentX, currentY;     // พิกัด
    
    // Helper แปลง OpenCV -> QImage เพื่อโชว์บน Qt
    QImage matToQImage(const cv::Mat &mat);
    void saveToDiskAndUsb();
};

#endif // BACKEND_CONTROLLER_H