#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QImage>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    // ส่งคำสั่งไป Backend
    void reqCapture();
    void reqSave(QString text);
    void reqDiscard();
    void reqAdjust(int bright, bool denoise);

public slots:
    // รับข้อมูลจาก Backend มาแสดงผล
    void updateLiveView(QImage img);
    void showReviewMode(QImage img, QString text);
    void showMessage(QString msg);

private:
    void setupUi();

    QStackedWidget *stackedWidget;
    
    // --- Page 1: Live ---
    QWidget *livePage;
    QLabel *liveImageLabel;
    QPushButton *btnScan;
    QLabel *statusLabel;

    // --- Page 2: Review ---
    QWidget *reviewPage;
    QLabel *reviewImageLabel;
    QLineEdit *textEdit; 
    QPushButton *btnLightUp;
    QPushButton *btnLightDown;
    QPushButton *btnDenoise;
    QPushButton *btnSave;
    QPushButton *btnDiscard;
    
    // State สำหรับการแต่งภาพ
    int currentBrightness = 0;
    bool isDenoise = false;
};

#endif // MAINWINDOW_H