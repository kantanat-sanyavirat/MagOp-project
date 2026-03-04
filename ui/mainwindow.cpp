#include "mainwindow.h"
#include <QDir>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    this->setMinimumSize(800, 480);
}

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    stackedWidget = new QStackedWidget();
    mainLayout->addWidget(stackedWidget);

    // --- PAGE 1: CAPTURE ---
    pageCapture = new QWidget();
    QHBoxLayout *layout1 = new QHBoxLayout(pageCapture);
    liveViewLabel = new QLabel("Waiting for Camera...");
    liveViewLabel->setStyleSheet("background-color: black; border: 2px solid #3E444C;");
    liveViewLabel->setAlignment(Qt::AlignCenter);

    QWidget *sideBar1 = new QWidget();
    sideBar1->setFixedWidth(160);
    QVBoxLayout *sideLayout1 = new QVBoxLayout(sideBar1);
    btnScan = new QPushButton("SCAN");
    btnScan->setObjectName("btnScan");
    btnGoHistory = new QPushButton("History");
    sideLayout1->addWidget(btnScan);
    sideLayout1->addStretch();
    sideLayout1->addWidget(btnGoHistory);

    layout1->addWidget(liveViewLabel, 1);
    layout1->addWidget(sideBar1, 0);
    stackedWidget->addWidget(pageCapture);

    // --- PAGE 2: REVIEW ---
    pageReview = new QWidget();
    QHBoxLayout *layout2 = new QHBoxLayout(pageReview);
    previewLabel = new QLabel();
    previewLabel->setStyleSheet("background-color: #222;");
    previewLabel->setAlignment(Qt::AlignCenter);

    QWidget *sideBar2 = new QWidget();
    sideBar2->setFixedWidth(160);
    QVBoxLayout *sideLayout2 = new QVBoxLayout(sideBar2);
    btnSave = new QPushButton("SAVE");
    btnSave->setObjectName("btnSave");
    btnReviewDelete = new QPushButton("DELETE"); 
    btnReviewDelete->setObjectName("btnDelete");
    btnReviewBack = new QPushButton("BACK");
    sideLayout2->addWidget(btnSave);
    sideLayout2->addWidget(btnReviewDelete);
    sideLayout2->addWidget(btnReviewBack);
    sideLayout2->addStretch();

    layout2->addWidget(previewLabel, 1);
    layout2->addWidget(sideBar2, 0);
    stackedWidget->addWidget(pageReview);

    // --- PAGE 3: HISTORY ---
    pageHistory = new QWidget();
    QHBoxLayout *layout3 = new QHBoxLayout(pageHistory);
    historyList = new QListWidget();
    btnBack = new QPushButton("Back");

    layout3->addWidget(historyList, 1);

    // สร้าง Layout แนวตั้งเล็กๆ เพื่อวางปุ่ม Back กับ Delete คู่กันด้านขวา
    QVBoxLayout *sideLayout3 = new QVBoxLayout();
    sideLayout3->addStretch();
    sideLayout3->addWidget(btnBack);

    layout3->addLayout(sideLayout3, 0); // ใส่ sideLayout เข้าไปใน layout แถวหลัก
    stackedWidget->addWidget(pageHistory);

    // --- STYLESHEET ---
    this->setStyleSheet(
        "QMainWindow { background-color: #545B64; }"
        "QWidget { color: white; }"
        "QPushButton { background-color: #3E444C; border-radius: 8px; padding: 12px; font-weight: bold; }"
        "QPushButton#btnScan { background-color: #00E676; color: black; font-size: 18px; }"
        "QListWidget { background-color: #3E444C; border-radius: 5px; }"
        "QListWidget::item { padding: 15px; border-bottom: 1px solid #707780; }"
        "QListWidget::item:selected { background-color: #00E676; color: black; }"

        /* 1. จัดการแถบเลื่อนแนวตั้ง */
        "QScrollBar:vertical {"
        "    border: none;"
        "    background: #3E444C;" // สีพื้นหลังของรางเลื่อน (เข้มกว่าพื้นหลังหลักนิดนึง)
        "    width: 25px;"         // เพิ่มความกว้างให้จิ้มง่ายขึ้น (จากปกติ 10-12px)
        "    margin: 0px 0px 0px 0px;"
        "}"

        /* 2. ตัวเลื่อน (Handle) ที่เราต้องเลื่อนขึ้นลง */
        "QScrollBar::handle:vertical {"
        "    background: #00E676;" // ใช้สีเขียวนีออนเหมือนปุ่ม SCAN เพื่อให้เด่น
        "    min-height: 30px;"
        "    border-radius: 12px;" // ทำขอบมนให้ดูทันสมัย
        "}"

        /* 3. เมื่อเอาเมาส์ไปวางหรือกดที่ตัวเลื่อน */
        "QScrollBar::handle:vertical:hover {"
        "    background: #00C853;" // สีเขียวเข้มขึ้นเวลาโดน
        "}"

        /* 4. ซ่อนปุ่มลูกศรบน-ล่าง เพื่อให้เหลือพื้นที่เลื่อนเยอะขึ้น */
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}"
        
        "QPushButton#btnReviewBack { background-color: #707780; color: white; }"
        "QPushButton#btnReviewBack:pressed { background-color: #545B64; }"
        "QPushButton#btnDelete { background-color: #FF5252; color: white; }"

        "QPushButton#btnSave {"
        "    background-color: #2ECC71;" // สีเขียว Emerald
        "    color: white;"
        "    font-weight: bold;"
        "    border-radius: 5px;"
        "    padding: 10px;"
        "}"
        "QPushButton#btnSave:pressed {"
        "    background-color: #27AE60;" // สีเขียวเข้มขึ้นเวลาโดนกด
        "}"
    );

    // --- CONNECT INTERNAL BUTTONS TO SIGNALS ---
    connect(btnScan, &QPushButton::clicked, this, &MainWindow::reqCapture);
    connect(btnSave, &QPushButton::clicked, this, [this](){
        if (btnSave->text() == "EXPORT") {
            // ถ้าปุ่มเป็นคำว่า EXPORT ให้สั่งส่งไฟล์ไป USB
            emit reqExportToUsb(currentFileName); 
            showMessage("Exporting " + currentFileName + " to USB...");
        } else {
            // ถ้าปุ่มเป็นคำว่า SAVE ให้เซฟลงเครื่องตามปกติ
            emit reqSave("result_text");
            stackedWidget->setCurrentIndex(0); // เซฟเสร็จกลับหน้ากล้อง
        }
    });
        
    // Switch Pages
    connect(btnGoHistory, &QPushButton::clicked, [this](){ 
        refreshHistoryList(); // อัพเดตรายการก่อนแสดง
        stackedWidget->setCurrentIndex(2); 
    });
    connect(btnBack, &QPushButton::clicked, [this](){ 
        stackedWidget->setCurrentIndex(0); // จากหน้า History กลับไปหน้า Camera (Index 0)
    });

    // คลิกที่รายชื่อ: จิ้มแล้วโหลดรูปไปหน้า Review
    connect(historyList, &QListWidget::itemClicked, [this](QListWidgetItem *item){
        QString fileName = item->text();
        if (fileName != "No history found.") {
            loadSavedImage(fileName);
            stackedWidget->setCurrentIndex(1); // แสดงหน้า Review เมื่อคลิกดูรูปเก่า
        }
    });

    // 1. ปุ่ม BACK ในหน้า REVIEW (Index 1) -> กลับไปหน้า HISTORY (Index 2)
    connect(btnReviewBack, &QPushButton::clicked, this, [this](){
        stackedWidget->setCurrentIndex(2); 
    });

    // 2. ปุ่ม BACK ในหน้า HISTORY (Index 2) -> กลับไปหน้า CAMERA (Index 0)
    connect(btnBack, &QPushButton::clicked, this, [this](){
        stackedWidget->setCurrentIndex(0); 
    });

    // 3. ปุ่ม DELETE ในหน้า REVIEW -> ลบแล้วกลับหน้า CAMERA (Index 0)
    connect(btnReviewDelete, &QPushButton::clicked, this, [this](){
        if (!currentFileName.isEmpty()) {
            emit reqDiscard(); // สั่ง Backend ลบ
            QString filePath = QDir::homePath() + "/MagOp-project/ai_output/" + currentFileName;
            if (QFile::remove(filePath)) {
                showMessage("Deleted: " + currentFileName);
            }
            currentFileName = ""; 
            refreshHistoryList();
        }
        stackedWidget->setCurrentIndex(0); 
    });

}

// --- IMPLEMENT SLOTS ---
void MainWindow::updateLiveView(const QImage &image) {
    liveViewLabel->setPixmap(QPixmap::fromImage(image).scaled(liveViewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

// --- ฟังก์ชันดึงรายชื่อไฟล์จากโฟลเดอร์ ---
void MainWindow::refreshHistoryList() {
    historyList->clear(); // ล้างรายการเก่าออกก่อน
    
    // ชี้ไปที่โฟลเดอร์เก็บรูป (แก้ Path ให้ตรงกับเครื่องคุณ)
    QString path = QDir::homePath() + "/MagOp-project/ai_output";
    QDir directory(path);
    
    // กรองเอาเฉพาะไฟล์รูปภาพ .jpg และ .png
    QStringList images = directory.entryList(QStringList() << "*.jpg" << "*.png", QDir::Files);
    
    // เรียงจากใหม่ไปเก่า (Optional)
    std::reverse(images.begin(), images.end());

    if (images.isEmpty()) {
        historyList->addItem("No history found.");
    } else {
        historyList->addItems(images);
    }
}

void MainWindow::showMessage(const QString &msg) {
    this->statusBar()->showMessage(msg, 3000);
}

void MainWindow::loadSavedImage(const QString &fileName) {
    currentFileName = fileName;
    QString filePath = QDir::homePath() + "/MagOp-project/ai_output/" + fileName;
    QImage img(filePath);

    if (!img.isNull()) {
        previewLabel->setPixmap(QPixmap::fromImage(img).scaled(previewLabel->size(), 
                                Qt::KeepAspectRatio, Qt::SmoothTransformation));

        // --- ปรับปุ่มสำหรับโหมดดูรูปเก่า (History Mode) ---
        btnSave->setText("EXPORT");   // <--- เปลี่ยนชื่อปุ่มชั่วคราว
        btnSave->show();              // แสดงปุ่มไว้สำหรับกด Export ไป USB
        btnReviewDelete->show();
        btnReviewBack->show();        // แสดงปุ่ม Back เพื่อกลับไปหน้า History List

        stackedWidget->setCurrentIndex(1);
    }
}

void MainWindow::showReviewMode(const QImage &image, const QString &fileName) {
    currentFileName = fileName;
    
    // อย่าลืมบรรทัดนี้! เพื่อแสดงรูปที่เพิ่งแสกน
    previewLabel->setPixmap(QPixmap::fromImage(image).scaled(previewLabel->size(), 
                            Qt::KeepAspectRatio, Qt::SmoothTransformation));

    btnSave->setText("SAVE");
    btnSave->show();
    btnReviewDelete->show();
    btnReviewBack->hide(); // ซ่อนปุ่ม Back ตอนแสกนเสร็จ

    stackedWidget->setCurrentIndex(1);
}
MainWindow::~MainWindow() {}