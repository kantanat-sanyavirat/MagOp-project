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

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void updateLiveView(const QImage &image);
    void showReviewMode(const QImage &image, const QString &text);
    void showMessage(const QString &msg);

signals:
    void reqCapture();
    void reqSave(const QString &text);
    void reqDiscard();
    void reqAdjust(int brightness, int contrast);
    void reqExportToUsb(QString fileName);

private:
    QStackedWidget *stackedWidget;

    // Page 1: Capture
    QWidget *pageCapture;
    QLabel *liveViewLabel;
    QPushButton *btnScan;
    QPushButton *btnGoHistory;

    // Page 2: Review
    QWidget *pageReview;
    QLabel *previewLabel;
    QPushButton *btnSave;
    QString currentFileName; // ตัวแปรสำหรับจำว่าตอนนี้เปิดไฟล์ไหนอยู่
    QPushButton *btnReviewDelete;
    QPushButton *btnReviewBack;

    // Page 3: History
    QWidget *pageHistory;
    QListWidget *historyList;
    QPushButton *btnBack;

    void setupUI(); 
    void refreshHistoryList();
    void loadSavedImage(const QString &fileName); // ฟังก์ชันสำหรับโหลดรูปเก่ามาโชว์
};
#endif