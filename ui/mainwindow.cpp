#include "mainwindow.h"
#include <QDir>
#include <QPixmap>
#include <QDateTime>
#include <QRegularExpression>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QShortcut>
#include <QKeySequence>
#include <QApplication>

// forward declaration — นิยามจริงอยู่ด้านล่างก่อน refreshHistoryList
static QString formatFileName(const QString &fileName);

// ─────────────────────────────────────────────────────────────
// Theme: Dark Industrial
//
// Color Palette:
//   --bg-base    : #0F1117  (พื้นหลังหลัก — ดำอมน้ำเงินเข้ม)
//   --bg-panel   : #1A1D27  (แผง sidebar)
//   --bg-surface : #22263A  (พื้นผิว widget เช่น input, list)
//   --border     : #2E3347  (เส้นขอบ)
//   --accent     : #00D4FF  (ฟ้า Cyan — สีหลักของ action)
//   --accent-dim : #0099BB  (ฟ้า Cyan เข้มขึ้น — hover / pressed)
//   --success    : #00C896  (เขียว — SAVE)
//   --danger     : #FF4560  (แดง — DELETE)
//   --text-main  : #E8EAF0  (ข้อความหลัก)
//   --text-dim   : #7A8099  (ข้อความรอง / label หัวข้อ)
// ─────────────────────────────────────────────────────────────

// Global stylesheet ใช้กับทั้งแอป — กำหนดที่ Constructor
static const QString APP_STYLE = R"(

/* ── ฐานของหน้าจอทั้งหมด ── */
QMainWindow, QWidget {
    background-color: #0F1117;
    color: #E8EAF0;
    font-family: "Noto Sans", "Segoe UI", sans-serif;
    font-size: 13px;
}

/* ── StatusBar ด้านล่าง ── */
QStatusBar {
    background-color: #0A0C12;
    color: #7A8099;
    border-top: 1px solid #2E3347;
    padding: 2px 8px;
    font-size: 11px;
}

/* ══════════════════════════════
   ปุ่มทั่วไป (ค่าเริ่มต้น)
══════════════════════════════ */
QPushButton {
    background-color: #22263A;
    color: #E8EAF0;
    border: 1px solid #2E3347;
    border-radius: 6px;
    padding: 10px 8px;
    font-size: 12px;
    font-weight: 600;
    letter-spacing: 0.5px;
    min-height: 36px;
}
QPushButton:hover {
    background-color: #2E3347;
    border-color: #00D4FF;
    color: #00D4FF;
}
QPushButton:pressed {
    background-color: #1A1D27;
    border-color: #0099BB;
}
QPushButton:disabled {
    background-color: #181B26;
    color: #3A3F55;
    border-color: #1E2130;
}

/* ══════════════════════════════
   ปุ่ม SCAN — Accent สีฟ้า (ปุ่มหลัก)
══════════════════════════════ */
QPushButton#btnScan {
    background-color: #00D4FF;
    color: #0F1117;
    border: none;
    border-radius: 6px;
    font-size: 14px;
    font-weight: 700;
    letter-spacing: 1.5px;
    min-height: 48px;
}
QPushButton#btnScan:hover {
    background-color: #33DDFF;
    color: #0F1117;
}
QPushButton#btnScan:pressed {
    background-color: #0099BB;
}

/* ══════════════════════════════
   ปุ่ม SAVE — สีเขียว
══════════════════════════════ */
QPushButton#btnSave {
    background-color: #00C896;
    color: #0F1117;
    border: none;
    font-weight: 700;
    min-height: 40px;
}
QPushButton#btnSave:hover   { background-color: #00E6AC; }
QPushButton#btnSave:pressed { background-color: #009970; }

/* ══════════════════════════════
   ปุ่ม DELETE — สีแดง (outline)
══════════════════════════════ */
QPushButton#btnDelete {
    background-color: transparent;
    color: #FF4560;
    border: 1px solid #FF4560;
    font-weight: 700;
    min-height: 40px;
}
QPushButton#btnDelete:hover {
    background-color: #FF4560;
    color: #0F1117;
}
QPushButton#btnDelete:pressed { background-color: #CC2040; }

/* ══════════════════════════════
   ปุ่ม Shutdown — เทาเข้ม ขอบแดงจาง
══════════════════════════════ */
QPushButton#btnShutdown {
    background-color: transparent;
    color: #7A8099;
    border: 1px solid #3A3F55;
    font-size: 11px;
    min-height: 32px;
}
QPushButton#btnShutdown:hover {
    background-color: #FF4560;
    color: #0F1117;
    border-color: #FF4560;
}
QPushButton#btnShutdown:pressed { background-color: #CC2040; }

/* ══════════════════════════════
   พื้นที่ภาพ (กล้องสด / Preview)
══════════════════════════════ */
QLabel#liveView, QLabel#previewView {
    background-color: #080A10;
    border: 1px solid #2E3347;
    border-radius: 4px;
    color: #3A3F55;
    font-size: 14px;
}

/* ══════════════════════════════
   Sidebar ด้านขวา
══════════════════════════════ */
QWidget#sideBar {
    background-color: #1A1D27;
    border-left: 1px solid #2E3347;
}

/* ══════════════════════════════
   Label หัวข้อใน Sidebar (SMALL CAPS)
══════════════════════════════ */
QLabel#sideLabel {
    color: #7A8099;
    font-size: 10px;
    font-weight: 600;
    letter-spacing: 1px;
}

/* ══════════════════════════════
   ช่อง Input (Label)
══════════════════════════════ */
QLineEdit {
    background-color: #22263A;
    color: #E8EAF0;
    border: 1px solid #2E3347;
    border-radius: 5px;
    padding: 8px 10px;
    font-size: 13px;
    selection-background-color: #00D4FF;
    selection-color: #0F1117;
}
QLineEdit:focus {
    border-color: #00D4FF;
    background-color: #272B3E;
}

/* ══════════════════════════════
   History List
══════════════════════════════ */
QListWidget {
    background-color: #12151E;
    border: none;
    color: #E8EAF0;
    font-size: 12px;
    outline: none;
}
QListWidget::item {
    padding: 10px 14px;
    border-bottom: 1px solid #1A1D27;
}
QListWidget::item:hover {
    background-color: #1E2235;
    color: #00D4FF;
}
QListWidget::item:selected {
    background-color: #00D4FF22;
    color: #00D4FF;
    border-left: 3px solid #00D4FF;
}

/* ══════════════════════════════
   Scrollbar
══════════════════════════════ */
QScrollBar:vertical {
    background: #12151E;
    width: 6px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background: #2E3347;
    border-radius: 3px;
    min-height: 20px;
}
QScrollBar::handle:vertical:hover { background: #00D4FF; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }

)";

// ─────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    // ใส่ Global Stylesheet ก่อน setupUI เพื่อให้ทุก Widget ได้รับสไตล์ทันที
    this->setStyleSheet(APP_STYLE);
    setupUI();
    this->setMinimumSize(800, 480);
    this->setWindowTitle("MagOp_app");
    this->showFullScreen(); // เปิดเต็มจอทันที ไม่มี taskbar / title bar
}

MainWindow::~MainWindow() {}

// ─────────────────────────────────────────────────────────────
// setupUI — สร้าง Widget ทั้งหมดและเชื่อม Signal/Slot
// ─────────────────────────────────────────────────────────────

void MainWindow::setupUI() {
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // ลบ margin รอบนอกออกให้ชิดขอบหน้าจอ
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    stackedWidget = new QStackedWidget();
    mainLayout->addWidget(stackedWidget);

    // ── Maintenance Shortcut ──────────────────────────────────
    // Ctrl+Alt+Q → ออกจากโปรแกรมสำหรับช่างเทคนิค (ผู้ใช้ทั่วไปไม่รู้)
    QShortcut *exitShortcut = new QShortcut(QKeySequence("Ctrl+Alt+Q"), this);
    connect(exitShortcut, &QShortcut::activated, this, []() {
        QApplication::quit();
    });

    // ════════════════════════════════════════
    // หน้า 0: Capture — แสดงภาพกล้องสด
    // ════════════════════════════════════════
    pageCapture = new QWidget();
    QHBoxLayout *layout1 = new QHBoxLayout(pageCapture);
    layout1->setContentsMargins(0, 0, 0, 0);
    layout1->setSpacing(0);

    // พื้นที่ภาพกล้อง
    liveViewLabel = new QLabel("Waiting for Camera...");
    liveViewLabel->setObjectName("liveView");
    liveViewLabel->setAlignment(Qt::AlignCenter);

    // Sidebar ขวา
    QWidget *sideBar1 = new QWidget();
    sideBar1->setObjectName("sideBar");
    sideBar1->setFixedWidth(160);
    QVBoxLayout *sideLayout1 = new QVBoxLayout(sideBar1);
    sideLayout1->setContentsMargins(12, 16, 12, 16);
    sideLayout1->setSpacing(8);

    btnScan = new QPushButton("No Camera");
    btnScan->setObjectName("btnScan");
    btnScan->setEnabled(false); // รอกล้อง — เปิดอัตโนมัติเมื่อ cameraReady(true)
    btnGoHistory = new QPushButton("History");

    // ปุ่ม Shutdown — ปิดเครื่อง Raspberry Pi อย่างปลอดภัย
    btnShutdown = new QPushButton("⏻  Shutdown");
    btnShutdown->setObjectName("btnShutdown");

    sideLayout1->addWidget(btnScan);
    sideLayout1->addStretch();
    sideLayout1->addWidget(btnGoHistory);
    sideLayout1->addSpacing(4);
    sideLayout1->addWidget(btnShutdown);

    layout1->addWidget(liveViewLabel, 1);
    layout1->addWidget(sideBar1, 0);
    stackedWidget->addWidget(pageCapture); // index 0

    // ════════════════════════════════════════
    // หน้า 1: Review — ตรวจทานและบันทึกผลลัพธ์
    // ════════════════════════════════════════
    pageReview = new QWidget();
    QHBoxLayout *layout2 = new QHBoxLayout(pageReview);
    layout2->setContentsMargins(0, 0, 0, 0);
    layout2->setSpacing(0);

    previewLabel = new QLabel();
    previewLabel->setObjectName("previewView");
    previewLabel->setAlignment(Qt::AlignCenter);

    QWidget *sideBar2 = new QWidget();
    sideBar2->setObjectName("sideBar");
    sideBar2->setFixedWidth(160);
    QVBoxLayout *sideLayout2 = new QVBoxLayout(sideBar2);
    sideLayout2->setContentsMargins(12, 16, 12, 16);
    sideLayout2->setSpacing(8);

    // หัวข้อ label และ input — ซ่อนในโหมด History
    lblManual = new QLabel("MANUAL LABEL");
    lblManual->setObjectName("sideLabel");

    txtUserInput = new QLineEdit();
    txtUserInput->setPlaceholderText("ชื่อสินค้า...");

    btnSave = new QPushButton("SAVE");
    btnSave->setObjectName("btnSave");
    btnSave->setEnabled(false);

    // กด Enter ใน input → trigger ปุ่ม SAVE ได้เลย ไม่ต้องเอื้อมไปคลิก
    connect(txtUserInput, &QLineEdit::returnPressed, btnSave, &QPushButton::click);

    // enable/disable ปุ่ม SAVE ตามว่า input ว่างหรือไม่
    connect(txtUserInput, &QLineEdit::textChanged, this, [this](const QString &text) {
        btnSave->setEnabled(!text.trimmed().isEmpty());
    });

    btnReviewDelete = new QPushButton("DELETE");
    btnReviewDelete->setObjectName("btnDelete");

    btnReviewBack = new QPushButton("◀  BACK");

    sideLayout2->addWidget(lblManual);
    sideLayout2->addWidget(txtUserInput);
    sideLayout2->addSpacing(4);
    sideLayout2->addWidget(btnSave);
    sideLayout2->addWidget(btnReviewDelete);
    sideLayout2->addStretch();
    sideLayout2->addWidget(btnReviewBack);

    layout2->addWidget(previewLabel, 1);
    layout2->addWidget(sideBar2, 0);
    stackedWidget->addWidget(pageReview); // index 1

    // ════════════════════════════════════════
    // หน้า 2: History — ประวัติการแสกน
    // ════════════════════════════════════════
    pageHistory = new QWidget();
    QHBoxLayout *layout3 = new QHBoxLayout(pageHistory);
    layout3->setContentsMargins(0, 0, 0, 0);
    layout3->setSpacing(0);

    historyList = new QListWidget();

    QWidget *sideBar3 = new QWidget();
    sideBar3->setObjectName("sideBar");
    sideBar3->setFixedWidth(160);
    QVBoxLayout *sideLayout3 = new QVBoxLayout(sideBar3);
    sideLayout3->setContentsMargins(12, 16, 12, 16);
    sideLayout3->setSpacing(8);

    QLabel *lblHistory = new QLabel("SCAN HISTORY");
    lblHistory->setObjectName("sideLabel");

    btnBack = new QPushButton("◀  BACK");

    sideLayout3->addWidget(lblHistory);
    sideLayout3->addStretch();
    sideLayout3->addWidget(btnBack);

    layout3->addWidget(historyList, 1);
    layout3->addWidget(sideBar3, 0);
    stackedWidget->addWidget(pageHistory); // index 2

    // ════════════════════════════════════════
    // เชื่อม Signal / Slot
    // ════════════════════════════════════════

    // ปุ่ม SCAN → disable ระหว่างรอ AI ป้องกันกดซ้ำ แล้วส่งสัญญาณให้ Backend
    connect(btnScan, &QPushButton::clicked, this, [this]() {
        btnScan->setEnabled(false);
        btnScan->setText("...");
        emit reqCapture();
    });

    // ปุ่ม Shutdown → ถามยืนยัน แล้วสั่งปิดเครื่อง Pi อย่างปลอดภัย
    connect(btnShutdown, &QPushButton::clicked, this, [this]() {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            this, "ปิดเครื่อง", "ต้องการปิดเครื่องหรือไม่?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;

        QProcess::startDetached("sudo", {"shutdown", "-h", "now"});
    });

    // ปุ่ม History → รีเฟรชรายการแล้วสลับไปหน้า History
    connect(btnGoHistory, &QPushButton::clicked, this, [this]() {
        refreshHistoryList();
        stackedWidget->setCurrentIndex(2);
    });

    // ปุ่ม Back (History) → กลับหน้า Capture
    connect(btnBack, &QPushButton::clicked, this, [this]() {
        stackedWidget->setCurrentIndex(0);
    });

    // เลือกไฟล์ใน History → โหลดชื่อไฟล์จริงจาก UserRole (ไม่ใช่ข้อความที่แสดง)
    connect(historyList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        const QString fileName = item->data(Qt::UserRole).toString();
        if (!fileName.isEmpty()) loadSavedImage(fileName);
    });

    // ปุ่ม SAVE → บันทึกลง Disk และส่งออก USB พร้อมกันเลย
    // โหมด History → ปุ่มนี้กลายเป็น EXPORT
    connect(btnSave, &QPushButton::clicked, this, [this]() {
        if (btnSave->text() == "SAVE") {
            emit reqSave(txtUserInput->text());
            stackedWidget->setCurrentIndex(0);
        } else {
            emit reqExportToUsb(currentFileName);
        }
    });

    // ปุ่ม DELETE → ถามยืนยันก่อนลบเสมอ ป้องกันกดพลาด
    // โหมดแสกนใหม่  (btnReviewBack ซ่อน) → กลับหน้า Capture (0)
    // โหมด History   (btnReviewBack แสดง) → กลับหน้า History (2)
    connect(btnReviewDelete, &QPushButton::clicked, this, [this]() {
        const QMessageBox::StandardButton reply = QMessageBox::question(
            this, "ยืนยันการลบ", "ต้องการลบรูปนี้หรือไม่?",
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;

        emit reqDiscard();
        const int returnPage = btnReviewBack->isVisible() ? 2 : 0;
        stackedWidget->setCurrentIndex(returnPage);
    });

    // ปุ่ม BACK (Review) → กลับหน้า History (แสดงเฉพาะโหมดดูรูปเก่า)
    connect(btnReviewBack, &QPushButton::clicked, this, [this]() {
        stackedWidget->setCurrentIndex(2);
    });
}

// ─────────────────────────────────────────────────────────────
// Slots — รับข้อมูลจาก Backend
// ─────────────────────────────────────────────────────────────

void MainWindow::updateLiveView(const QImage &image) {
    if (image.isNull()) return;
    liveViewLabel->setPixmap(
        QPixmap::fromImage(image).scaled(liveViewLabel->size(),
                                         Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
}

// เปิดหน้า Review — ใช้ได้ทั้งโหมดแสกนใหม่ และโหมดดูรูปเก่าจาก History
// โหมดแสกนใหม่ : btnSave = "SAVE" (เขียว),  btnReviewBack ซ่อน
// โหมด History  : btnSave = "EXPORT" (เทา),  btnReviewBack แสดง
void MainWindow::showReviewMode(const QImage &image, const QString &fileName) {
    currentFileName = fileName;

    previewLabel->setPixmap(
        QPixmap::fromImage(image).scaled(previewLabel->size(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));

    txtUserInput->setText("Product_A");
    txtUserInput->setFocus();
    txtUserInput->selectAll();

    // ตั้งค่าสำหรับโหมดแสกนใหม่ — ปุ่มสีเขียว
    btnSave->setText("SAVE");
    btnSave->setObjectName("btnSave");
    btnSave->style()->unpolish(btnSave); // บังคับ Qt refresh stylesheet
    btnSave->style()->polish(btnSave);

    // คืนสถานะปุ่ม SCAN (ถูก disable ตอนกด)
    btnScan->setEnabled(true);
    btnScan->setText("SCAN");

    // แสดง Manual Label และ input (ซ่อนอยู่ในโหมด History)
    lblManual->show();
    txtUserInput->show();

    btnReviewBack->hide();
    stackedWidget->setCurrentIndex(1);
}

void MainWindow::showMessage(const QString &msg) {
    statusBar()->showMessage(msg, 3000);
}

// อัปเดตสถานะปุ่ม SCAN ตามการเชื่อมต่อกล้อง
void MainWindow::setCameraReady(bool ready) {
    btnScan->setEnabled(ready);
    if (ready) {
        // กล้องเจอแล้ว — คืนสถานะปกติ
        btnScan->setText("SCAN");
        statusBar()->clearMessage();
    } else {
        // ยังไม่เจอกล้อง — แสดงข้อความแจ้งเตือน
        btnScan->setText("No Camera");
        statusBar()->showMessage("ไม่พบกล้อง — กรุณาเชื่อมต่ออุปกรณ์แล้วรอสักครู่", 0);
    }
}

// รับรายชื่อไฟล์ที่อัปเดตจาก Backend → ใส่ลง History List ทันทีโดยไม่ต้องออกแล้วเข้าใหม่
void MainWindow::updateHistoryList(const QStringList &files) {
    historyList->clear();
    if (files.isEmpty()) {
        historyList->addItem("— No history found —");
        return;
    }
    for (const QString &fileName : files) {
        QListWidgetItem *item = new QListWidgetItem(formatFileName(fileName));
        item->setData(Qt::UserRole, fileName);
        historyList->addItem(item);
    }
}

// ─────────────────────────────────────────────────────────────
// Helper Functions
// ─────────────────────────────────────────────────────────────

// โหลดรูปจาก Disk แล้วสลับเป็นโหมด History (EXPORT + ปุ่ม BACK)
void MainWindow::loadSavedImage(const QString &fileName) {
    currentFileName = fileName;
    const QString path = QDir::homePath() + "/MagOp-project/ai_output/" + fileName;
    QImage img(path);

    if (img.isNull()) {
        showMessage("โหลดรูปไม่สำเร็จ: " + fileName);
        return;
    }

    previewLabel->setPixmap(
        QPixmap::fromImage(img).scaled(previewLabel->size(),
                                        Qt::KeepAspectRatio,
                                        Qt::SmoothTransformation));
    // ค่าเริ่มต้นเป็นว่างไว้ก่อน — จะถูกแทนด้วย label จาก .txt ถ้ามี
    txtUserInput->clear();

    // อ่าน label จากไฟล์ .txt คู่กัน (ถ้ามี)
    QString labelFileName = fileName;
    labelFileName.replace(".jpg", ".txt", Qt::CaseInsensitive);
    labelFileName.replace(".png", ".txt", Qt::CaseInsensitive);
    QFile labelFile(QDir::homePath() + "/MagOp-project/ai_output/" + labelFileName);
    if (labelFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString label = QTextStream(&labelFile).readAll().trimmed();
        txtUserInput->setText(label);
    } else {
        // ไฟล์ .txt ไม่มี (รูปเก่าก่อนมี feature นี้) — แสดง placeholder แทน
        txtUserInput->setPlaceholderText("ไม่พบ label (รูปเก่า)");
    }

    // สลับเป็นโหมด History — ซ่อน Manual Label และ input
    lblManual->hide();
    txtUserInput->hide();

    btnSave->setText("EXPORT");
    btnSave->setObjectName("btnExport");
    btnSave->setEnabled(true); // โหมด History เปิด EXPORT ไว้เสมอ
    btnSave->style()->unpolish(btnSave);
    btnSave->style()->polish(btnSave);

    btnReviewBack->show();
    stackedWidget->setCurrentIndex(1);
}

// แปลงชื่อไฟล์ SCAN_yyyyMMdd_HHmmss.jpg → "DD/MM/YYYY  HH:MM" อ่านง่ายขึ้น
// ถ้า parse ไม่ได้ (ชื่อไฟล์ไม่ตรง format) ให้คืนชื่อไฟล์เดิม
static QString formatFileName(const QString &fileName) {
    // ตัด prefix "SCAN_" และ suffix ".jpg" ออก → "yyyyMMdd_HHmmss"
    QString mid = fileName;
    mid.remove(QRegularExpression("^SCAN_")).remove(QRegularExpression("\\.[^.]+$"));

    const QDateTime dt = QDateTime::fromString(mid, "yyyyMMdd_HHmmss");
    if (!dt.isValid()) return fileName; // fallback ถ้า parse ไม่ได้

    return dt.toString("dd/MM/yyyy  HH:mm");
}

// อ่านรายชื่อไฟล์ .jpg/.png จากโฟลเดอร์ เรียงใหม่สุดไว้บนสุด
// แสดงวันเวลาแทนชื่อไฟล์ดิบ แต่เก็บ data จริงไว้ใน UserRole เพื่อใช้โหลดรูป
void MainWindow::refreshHistoryList() {
    historyList->clear();
    QDir dir(QDir::homePath() + "/MagOp-project/ai_output");
    const QStringList files = dir.entryList(
        QStringList() << "*.jpg" << "*.png",
        QDir::Files,
        QDir::Time);

    if (files.isEmpty()) {
        historyList->addItem("— No history found —");
        return;
    }

    for (const QString &fileName : files) {
        QListWidgetItem *item = new QListWidgetItem(formatFileName(fileName));
        item->setData(Qt::UserRole, fileName); // เก็บชื่อไฟล์จริงไว้ใน data
        historyList->addItem(item);
    }
}