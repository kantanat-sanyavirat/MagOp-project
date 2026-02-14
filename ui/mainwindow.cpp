#include "mainwindow.h" // โปรแกรมจะหาเจอเพราะเราจะแก้ CMake ให้มันมองเห็น
#include <QHBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi();
}

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
    liveImageLabel->setStyleSheet("background-color: black; color: white;");
    liveImageLabel->setMinimumSize(640, 480);

    btnScan = new QPushButton("SCAN OBJECT");
    btnScan->setMinimumHeight(60);
    btnScan->setStyleSheet("font-size: 20px; font-weight: bold; background-color: #4CAF50; color: white;");
    
    statusLabel = new QLabel("Ready");
    statusLabel->setAlignment(Qt::AlignCenter);

    liveLayout->addWidget(liveImageLabel);
    liveLayout->addWidget(statusLabel);
    liveLayout->addWidget(btnScan);

    // Connect Scan Button
    connect(btnScan, &QPushButton::clicked, this, &MainWindow::reqCapture);

    stackedWidget->addWidget(livePage);

    // ===========================
    // PAGE 2: REVIEW MODE
    // ===========================
    reviewPage = new QWidget();
    QVBoxLayout *reviewLayout = new QVBoxLayout(reviewPage);

    reviewImageLabel = new QLabel();
    reviewImageLabel->setAlignment(Qt::AlignCenter);
    reviewImageLabel->setStyleSheet("background-color: #222;");

    // Text Edit Area
    QHBoxLayout *textLayout = new QHBoxLayout();
    QLabel *lblText = new QLabel("Detected Text:");
    textEdit = new QLineEdit();
    textEdit->setStyleSheet("font-size: 16px; padding: 5px;");
    textLayout->addWidget(lblText);
    textLayout->addWidget(textEdit);

    // Tools Area
    QHBoxLayout *toolsLayout = new QHBoxLayout();
    btnLightUp = new QPushButton("Light +");
    btnLightDown = new QPushButton("Light -");
    btnDenoise = new QPushButton("Denoise");
    
    toolsLayout->addWidget(btnLightDown);
    toolsLayout->addWidget(btnLightUp);
    toolsLayout->addWidget(btnDenoise);

    // Action Area
    QHBoxLayout *actionLayout = new QHBoxLayout();
    btnSave = new QPushButton("SAVE");
    btnSave->setStyleSheet("background-color: #2196F3; color: white; font-weight: bold; padding: 10px;");
    btnDiscard = new QPushButton("DISCARD");
    btnDiscard->setStyleSheet("background-color: #f44336; color: white; font-weight: bold; padding: 10px;");

    actionLayout->addWidget(btnDiscard);
    actionLayout->addWidget(btnSave);

    // Add everything to layout
    reviewLayout->addWidget(reviewImageLabel);
    reviewLayout->addLayout(textLayout);
    reviewLayout->addLayout(toolsLayout);
    reviewLayout->addLayout(actionLayout);

    stackedWidget->addWidget(reviewPage);

    // --- Connect Signals ---
    connect(btnDiscard, &QPushButton::clicked, [=](){
        emit reqDiscard();
        stackedWidget->setCurrentIndex(0); // กลับไปหน้า Live
        currentBrightness = 0; isDenoise = false; // Reset ค่า
    });

    connect(btnSave, &QPushButton::clicked, [=](){
        emit reqSave(textEdit->text());
        stackedWidget->setCurrentIndex(0); // กลับไปหน้า Live
    });

    // ปุ่มปรับแต่งภาพ
    connect(btnLightUp, &QPushButton::clicked, [=](){
        currentBrightness += 20;
        emit reqAdjust(currentBrightness, isDenoise);
    });
    connect(btnLightDown, &QPushButton::clicked, [=](){
        currentBrightness -= 20;
        emit reqAdjust(currentBrightness, isDenoise);
    });
    connect(btnDenoise, &QPushButton::clicked, [=](){
        isDenoise = !isDenoise; // Toggle
        emit reqAdjust(currentBrightness, isDenoise);
    });
}

void MainWindow::updateLiveView(QImage img) {
    if(stackedWidget->currentIndex() != 0) stackedWidget->setCurrentIndex(0);
    // Scale รูปให้พอดีจอ
    liveImageLabel->setPixmap(QPixmap::fromImage(img).scaled(liveImageLabel->size(), Qt::KeepAspectRatio));
}

void MainWindow::showReviewMode(QImage img, QString text) {
    stackedWidget->setCurrentIndex(1); // สลับไปหน้า Review
    reviewImageLabel->setPixmap(QPixmap::fromImage(img).scaled(reviewImageLabel->size(), Qt::KeepAspectRatio));
    if(!text.isEmpty()) textEdit->setText(text); // อัปเดตข้อความเฉพาะตอนแรก (หรือตอน AI ส่งมา)
}

void MainWindow::showMessage(QString msg) {
    statusLabel->setText(msg);
}