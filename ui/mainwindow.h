#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QLineEdit>
#include <QVBoxLayout>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

signals:
    // ส่งคำสั่งไป Backend
    void reqCapture();
    void reqSave(QString text);
    void reqDiscard();
    void reqAdjust(int bright, bool denoise);

public slots:
    void updateLiveView(QImage img);
    void showReviewMode(QImage img, QString text);
    void showMessage(QString msg);

private:
    QStackedWidget *stackedWidget;
    
    // --- Page 1: Live ---
    QWidget *livePage;
    QLabel *liveImageLabel;
    QPushButton *btnScan;
    QLabel *statusLabel;

    // --- Page 2: Review ---
    QWidget *reviewPage;
    QLabel *reviewImageLabel;
    QLineEdit *textEdit; // ช่องแก้ข้อความ
    QPushButton *btnLightUp;
    QPushButton *btnLightDown;
    QPushButton *btnDenoise;
    QPushButton *btnSave;
    QPushButton *btnDiscard;
    
    // State แต่งภาพ
    int currentBrightness = 0;
    bool isDenoise = false;

    void setupUi();
};

#endif // MAINWINDOW_H