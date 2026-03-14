#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QStatusBar>
#include <QLineEdit>
#include <QMessageBox>
#include <QRegularExpression>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    // รับภาพสดจากกล้องมาแสดงที่หน้า Capture
    void updateLiveView(const QImage &image);

    // เปิดหน้า Review พร้อมรูปและชื่อไฟล์ชั่วคราว
    // ใช้ร่วมกันทั้งโหมด "แสกนใหม่" และ "ดูรูปเก่าจาก History"
    void showReviewMode(const QImage &image, const QString &fileName);

    // แสดงข้อความแจ้งเตือนที่ StatusBar ด้านล่าง (หายเองใน 3 วินาที)
    void showMessage(const QString &msg);

    // รับรายชื่อไฟล์ล่าสุดจาก Backend มาอัปเดต History List ทันที
    void updateHistoryList(const QStringList &files);

    // อัปเดตสถานะปุ่ม SCAN ตามการเชื่อมต่อกล้อง
    void setCameraReady(bool ready);

signals:
    // ร้องขอให้ Backend ถ่ายภาพจาก Frame ปัจจุบัน
    void reqCapture();

    // ร้องขอให้ Backend บันทึกภาพพร้อมข้อความ Label
    void reqSave(const QString &text);

    // ร้องขอให้ Backend ยกเลิกและลบภาพที่แสกน
    void reqDiscard();

    // [หมายเหตุ] reqAdjust ถูก comment ออกชั่วคราว
    // เนื่องจาก signature ไม่ตรงกับ BackendController::adjustImage(int, bool)
    // → กำหนด signature ให้ตรงกันก่อนแล้วค่อย uncomment
    // void reqAdjust(int brightness, int contrast);

    // ร้องขอให้ Backend ส่งไฟล์ไปยัง USB Drive
    void reqExportToUsb(const QString &fileName);

private:
    // ── Widget หลัก ──────────────────────────────
    QStackedWidget *stackedWidget; // ควบคุมการสลับระหว่าง 3 หน้า

    // ── หน้า 0: Capture (กล้องสด) ───────────────
    QWidget    *pageCapture;
    QLabel     *liveViewLabel; // พื้นที่แสดงภาพกล้องสด
    QPushButton *btnScan;      // ปุ่มแสกน
    QPushButton *btnGoHistory; // ปุ่มไปหน้าประวัติ
    QPushButton *btnShutdown;  // ปุ่มปิดเครื่อง Raspberry Pi

    // ── หน้า 1: Review (ตรวจทานผลลัพธ์) ─────────
    QWidget     *pageReview;
    QLabel      *previewLabel;     // พื้นที่แสดงรูปที่แสกน/เลือกจาก History
    QLineEdit   *txtUserInput;     // ช่องพิมพ์ Label (ใช้กับคีย์บอร์ดจริงได้)
    QLabel      *lblManual;        // หัวข้อ "MANUAL LABEL" (ซ่อนในโหมด History)
    QPushButton *btnSave;          // SAVE (โหมดแสกน) หรือ EXPORT (โหมด History)
    QPushButton *btnReviewDelete;  // ลบรูปปัจจุบัน
    QPushButton *btnReviewBack;    // กลับหน้า History (ซ่อนอยู่ในโหมดแสกน)
    QString      currentFileName;  // ชื่อไฟล์ที่กำลังจัดการอยู่

    // ── หน้า 2: History (ประวัติการแสกน) ─────────
    QWidget     *pageHistory;
    QListWidget *historyList; // รายการไฟล์รูปที่บันทึกไว้
    QPushButton *btnBack;     // กลับหน้า Capture

    // ── Helper Functions ─────────────────────────
    void setupUI();                              // สร้าง Widget และเชื่อม Signal/Slot
    void refreshHistoryList();                   // โหลดรายชื่อไฟล์ใหม่จาก Disk
    void loadSavedImage(const QString &fileName); // โหลดรูปเก่าจาก Disk มาแสดง
};

#endif // MAINWINDOW_H