#include "mainwindow.h"
#include <QHBoxLayout>
#include <QDebug>
#include <QPixmap>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
    qDebug() << "UI: MainWindow initialized.";
}

MainWindow::~MainWindow() {}

void MainWindow::setupUi() {
    stackedWidget = new QStackedWidget(this);
    setCentralWidget(stackedWidget);

    // ===========================
    // PAGE 1: LIVE VIEW
    // ===========================
    livePage = new QWidget();
    QVBoxLayout *liveLayout = new QVBoxLayout(livePage);

    liveImageLabel = new QLabel("Initializing Camera...");
    liveImageLabel->setAlignment(Qt::AlignCenter);
    liveImageLabel->setStyleSheet("background-color: black; color: white; border: 2px solid #333;");
    liveImageLabel->setMinimumSize(640, 480);

    btnScan = new QPushButton("SCAN OBJECT");
    btnScan->setMinimumHeight(60);
    btnScan->setStyleSheet("font-size: 18px; font-weight: bold; background-color: #4CAF50; color: white;");
    
    statusLabel = new QLabel("System Ready");
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->setStyleSheet("font-weight: bold; color: #555;");

    liveLayout->addWidget(liveImageLabel);
    liveLayout->addWidget(statusLabel);
    liveLayout->addWidget(btnScan);

    connect(btnScan, &QPushButton::clicked, this, &MainWindow::reqCapture);
    stackedWidget->addWidget(livePage);

    // ===========================
    // PAGE 2: REVIEW MODE
    // ===========================
    reviewPage = new QWidget();
    QVBoxLayout *reviewLayout = new QVBoxLayout(reviewPage);

    reviewImageLabel = new QLabel();
    reviewImageLabel->setAlignment(Qt::AlignCenter);
    reviewImageLabel->setStyleSheet("background-color: #1a1a1a;");
    reviewImageLabel->setMinimumSize(640, 400);

    // Text Edit Area
    QHBoxLayout *textLayout = new QHBoxLayout();
    QLabel *lblText = new QLabel("Detected Label:");
    textEdit = new QLineEdit();
    textEdit->setStyleSheet("font-size: 16px; padding: 8px; border: 1px solid #ccc;");
    textLayout->addWidget(lblText);
    textLayout->addWidget(textEdit);

    // Tools Area (Brightness / Denoise)
    QHBoxLayout *toolsLayout = new QHBoxLayout();
    btnLightUp = new QPushButton("Brightness +");
    btnLightDown = new QPushButton("Brightness -");
    btnDenoise = new QPushButton("Toggle Denoise");
    
    toolsLayout->addWidget(btnLightDown);
    toolsLayout->addWidget(btnLightUp);
    toolsLayout->addWidget(btnDenoise);

    // Action Buttons
    QHBoxLayout *actionLayout = new QHBoxLayout();
    btnSave = new QPushButton("CONFIRM & SAVE");
    btnSave->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; height: 40px;");
    btnDiscard = new QPushButton("CANCEL");
    btnDiscard->setStyleSheet("background-color: #f44336; color: white; font-weight: bold; height: 40px;");

    actionLayout->addWidget(btnDiscard);
    actionLayout->addWidget(btnSave);

    reviewLayout->addWidget(reviewImageLabel);
    reviewLayout->addLayout(textLayout);
    reviewLayout->addLayout(toolsLayout);
    reviewLayout->addLayout(actionLayout);

    stackedWidget->addWidget(reviewPage);

    // --- Signals Connection ---
    connect(btnDiscard, &QPushButton::clicked, [=](){
        currentBrightness = 0; isDenoise = false; // Reset settings
        emit reqDiscard();
        stackedWidget->setCurrentIndex(0); 
    });

    connect(btnSave, &QPushButton::clicked, [=](){
        emit reqSave(textEdit->text());
        stackedWidget->setCurrentIndex(0); 
    });

    connect(btnLightUp, &QPushButton::clicked, [=](){
        currentBrightness += 20;
        emit reqAdjust(currentBrightness, isDenoise);
    });

    connect(btnLightDown, &QPushButton::clicked, [=](){
        currentBrightness -= 20;
        emit reqAdjust(currentBrightness, isDenoise);
    });

    connect(btnDenoise, &QPushButton::clicked, [=](){
        isDenoise = !isDenoise;
        emit reqAdjust(currentBrightness, isDenoise);
    });
}

void MainWindow::updateLiveView(QImage img) {
    if(stackedWidget->currentIndex() != 0) return;
    if(img.isNull()) return;

    QSize labelSize = liveImageLabel->size();
    // ป้องกันการ scale เป็น 0 ถ้าหน้าต่างยังไม่ถูกวาดเสร็จ
    if (labelSize.width() < 100) {
        liveImageLabel->setPixmap(QPixmap::fromImage(img));
    } else {
        liveImageLabel->setPixmap(QPixmap::fromImage(img).scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

void MainWindow::showReviewMode(QImage img, QString text) {
    stackedWidget->setCurrentIndex(1);
    
    if(!img.isNull()) {
        reviewImageLabel->setPixmap(QPixmap::fromImage(img).scaled(reviewImageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    
    // อัปเดตข้อความเฉพาะตอนที่ส่งมาจาก AI ครั้งแรก (ถ้า text ว่าง แสดงว่าเป็นการปรับแต่งภาพ)
    if(!text.isEmpty()) {
        textEdit->setText(text);
    }
}

void MainWindow::showMessage(QString msg) {
    statusLabel->setText(msg);
    qDebug() << "UI Status Message:" << msg;
}