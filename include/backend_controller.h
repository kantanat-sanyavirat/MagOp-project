#ifndef BACKEND_CONTROLLER_H
#define BACKEND_CONTROLLER_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QDir>
#include <opencv2/opencv.hpp>

// Include ไฟล์ลูกน้อง
#include "camera_handler.h"
#include "ai_processing.h"

class BackendController : public QObject
{
    Q_OBJECT
public:
    explicit BackendController(QObject *parent = nullptr);
    ~BackendController();
    
    // ฟังก์ชันเริ่มระบบ
    void start();

public slots:
    // --- รับคำสั่งจากหน้าจอ (Frontend) ---
    void capture();                     // กดปุ่ม Scan
    void save(QString userText);        // กดปุ่ม Save (รับข้อความที่แก้แล้ว)
    void discard();                     // กดปุ่ม Discard
    
    // ปรับแต่งภาพ (รับค่า Brightness และปุ่ม Denoise)
    void adjustImage(int brightnessStep, bool denoise); 
    
    // จัดการไฟล์
    void deleteFile(const QString &fileName);
    void refreshFileList();

signals:
    // --- ส่งข้อมูลไปหน้าจอ (Frontend) ---
    void frameReady(QImage img);                 // ส่งภาพสด (Live View)
    void reviewReady(QImage img, QString text);  // ส่งภาพนิ่ง (Review Mode)
    void fileListUpdated(QStringList files);     // ส่งรายชื่อไฟล์ (Gallery)
    void statusMessage(QString msg);             // ส่งข้อความแจ้งเตือน

private slots:
    // Slots ภายในสำหรับคุยกับลูกน้อง
    void processCameraFrame(cv::Mat frame);
    void handleAiResult(FrameResult result);

private:
    // ลูกน้องทั้งสอง
    CameraHandler *camera;
    QThread *aiThread;
    AI_Processing *aiProcessor;

    // ตัวแปรเก็บสถานะภาพ
    cv::Mat currentLiveFrame;   // ภาพล่าสุดจากกล้อง (รอถูก capture)
    cv::Mat originalCvImage;    // ภาพต้นฉบับที่ถ่ายได้
    cv::Mat currentEditedImage; // ภาพที่กำลังแต่งอยู่
    
    // ข้อมูลประกอบ
    QString currentTimestamp;
    QString currentText;
    int currentX, currentY;

    // ฟังก์ชันช่วยแปลงภาพ OpenCV -> Qt
    QImage matToQImage(const cv::Mat &mat);
    void saveToDiskAndUsb();
};

#endif // BACKEND_CONTROLLER_H