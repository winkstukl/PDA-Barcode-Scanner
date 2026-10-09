#include "mainwindow.h"
#include "camera.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <QTextEdit>
#include <QKeyEvent>
#include <QPixmap>
#include <QTextCursor>
#include <QSizePolicy>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_camera(nullptr),
      m_serialPort(nullptr),
      m_isBusy(false)
{
    setFocusPolicy(Qt::StrongFocus);

#if __arm__
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
#endif

    initUi();
    initSerial();
    initCamera();

#if __arm__
    showFullScreen();
#else
    resize(800, 480);
#endif
}

MainWindow::~MainWindow()
{
    if (m_camera) {
        m_camera->stopPreview();
        m_camera->closeCamera();
    }

    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
    }
}

void MainWindow::initUi()
{
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    QVBoxLayout *rootLayout = new QVBoxLayout(m_centralWidget);
    rootLayout->setContentsMargins(6, 6, 6, 6);
    rootLayout->setSpacing(4);

    // 顶部栏
    QHBoxLayout *topLayout = new QHBoxLayout();
    m_backBtn = new QPushButton("返回");
    m_backBtn->setFixedSize(90, 40);
    connect(m_backBtn, &QPushButton::clicked, this, &MainWindow::close);

    QLabel *titleLabel = new QLabel("0330BARcode");
    titleLabel->setStyleSheet("font-size:20px;font-weight:bold;");

    topLayout->addWidget(m_backBtn);
    topLayout->addStretch();
    topLayout->addWidget(titleLabel);
    topLayout->addStretch();

    rootLayout->addLayout(topLayout);

    m_tabWidget = new QTabWidget(this);
    rootLayout->addWidget(m_tabWidget);

    // ======================== 拍摄识别页 ========================
    QWidget *cameraPage = new QWidget(this);
    QHBoxLayout *cameraPageLayout = new QHBoxLayout(cameraPage);
    cameraPageLayout->setContentsMargins(8, 8, 8, 8);
    cameraPageLayout->setSpacing(10);

    // 左侧：大预览区
    QWidget *leftWidget = new QWidget(cameraPage);
    QVBoxLayout *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(6);

    QLabel *previewTitle = new QLabel("摄像头预览");
    previewTitle->setStyleSheet("font-size:16px;font-weight:bold;");

    m_cameraLabel = new QLabel("摄像头预览未启动");
    m_cameraLabel->setAlignment(Qt::AlignCenter);
    m_cameraLabel->setMinimumSize(500, 340);
    m_cameraLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_cameraLabel->setStyleSheet("border:1px solid #666;background:#111;color:#fff;");

    leftLayout->addWidget(previewTitle);
    leftLayout->addWidget(m_cameraLabel, 1);

    // 右侧：结果、缩略图、按钮
    QWidget *rightWidget = new QWidget(cameraPage);
    rightWidget->setFixedWidth(240);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(8);

    QGroupBox *resultBox = new QGroupBox("识别结果");
    QFormLayout *resultLayout = new QFormLayout(resultBox);

    m_barcodeValueLabel = new QLabel("--");
    m_barcodeTypeLabel = new QLabel("--");
    m_itemLabel = new QLabel("--");
    m_statusLabel = new QLabel("等待拍照...");
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("color:#2E8B57;font-size:14px;");

    resultLayout->addRow("条码值：", m_barcodeValueLabel);
    resultLayout->addRow("类型：", m_barcodeTypeLabel);
    resultLayout->addRow("标签：", m_itemLabel);
    resultLayout->addRow("状态：", m_statusLabel);

    QLabel *thumbTitle = new QLabel("拍照缩略图");
    thumbTitle->setStyleSheet("font-size:15px;font-weight:bold;");

    m_thumbLabel = new QLabel("暂无");
    m_thumbLabel->setAlignment(Qt::AlignCenter);
    m_thumbLabel->setFixedSize(220, 140);
    m_thumbLabel->setStyleSheet("border:1px solid #999;background:#fafafa;");

    m_captureBtn = new QPushButton("拍照并识别");
    m_captureBtn->setFixedHeight(52);
    m_captureBtn->setStyleSheet("font-size:18px;");
    m_captureBtn->setEnabled(false);
    connect(m_captureBtn, &QPushButton::clicked, this, &MainWindow::takePhotoAndRecognize);

    rightLayout->addWidget(resultBox);
    rightLayout->addWidget(thumbTitle);
    rightLayout->addWidget(m_thumbLabel, 0, Qt::AlignHCenter);
    rightLayout->addSpacing(4);
    rightLayout->addWidget(m_captureBtn);
    rightLayout->addStretch();

    cameraPageLayout->addWidget(leftWidget, 1);
    cameraPageLayout->addWidget(rightWidget, 0);

    m_tabWidget->addTab(cameraPage, "拍摄识别");

    // ======================== 串口调试页 ========================
    QWidget *serialPage = new QWidget(this);
    QVBoxLayout *serialLayout = new QVBoxLayout(serialPage);
    serialLayout->setContentsMargins(8, 8, 8, 8);
    serialLayout->setSpacing(6);

    QLabel *serialInfo = new QLabel("串口调试界面 | 波特率固定：921600");
    serialInfo->setStyleSheet("font-size:15px;font-weight:bold;");

    m_serialLog = new QTextEdit();
    m_serialLog->setReadOnly(true);
    m_serialLog->setPlaceholderText("这里显示串口收发日志...");
    m_serialLog->setStyleSheet("font-size:13px;");

    QPushButton *clearBtn = new QPushButton("清空日志");
    clearBtn->setFixedWidth(100);
    connect(clearBtn, &QPushButton::clicked, m_serialLog, &QTextEdit::clear);

    serialLayout->addWidget(serialInfo);
    serialLayout->addWidget(m_serialLog);
    serialLayout->addWidget(clearBtn, 0, Qt::AlignRight);

    m_tabWidget->addTab(serialPage, "串口调试");
}

void MainWindow::initSerial()
{
    m_serialPort = new QSerialPort(this);

#if __arm__
    m_serialPort->setPortName("/dev/ttySTM1");
#else
    m_serialPort->setPortName("COM3");
#endif

    m_serialPort->setBaudRate(115200);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (m_serialPort->open(QIODevice::ReadWrite)) {
        connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::serialPortReadyRead);
        appendSerialLog("[系统] 串口打开成功，115200");
    } else {
        appendSerialLog("[错误] 串口打开失败，请检查设备节点和权限");
    }
}

void MainWindow::initCamera()
{
    m_camera = new Camera(this);

    connect(m_camera, &Camera::readyImage, this, &MainWindow::showCameraImage);
    connect(m_camera, &Camera::errorOccurred, this, &MainWindow::onCameraError);

#if __arm__
    if (!m_camera->openCamera(0)) {
        m_statusLabel->setText("未检测到 /dev/video0");
        return;
    }
#else
    if (!m_camera->openCamera(0)) {
        m_statusLabel->setText("PC 摄像头打开失败");
        return;
    }
#endif

    if (m_camera->startPreview(120)) {
        m_captureBtn->setEnabled(true);
        m_statusLabel->setText("请对准条形码，点击按钮或按实体键拍照识别");
        appendSerialLog("[系统] 摄像头启动成功，预览已开始");
    }
}

void MainWindow::appendSerialLog(const QString &text)
{
    QString line = QString("[%1] %2")
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
            .arg(text);

    m_serialLog->append(line);

    QTextCursor cursor = m_serialLog->textCursor();
    cursor.movePosition(QTextCursor::End);
    m_serialLog->setTextCursor(cursor);
}

void MainWindow::showCameraImage(const QImage &img)
{
    if (img.isNull()) {
        return;
    }

    QPixmap pix = QPixmap::fromImage(img);
    m_cameraLabel->setPixmap(
        pix.scaled(m_cameraLabel->size(),
                   Qt::KeepAspectRatio,
                   Qt::SmoothTransformation)
    );
}

QString MainWindow::queryLabelByCode(const QString &code) const
{
    if (code == "6901234567890") return "矿泉水";
    if (code == "6923450657713") return "方便面";
    if (code == "6954767412345") return "洗发水";
    if (code == "6901028075760") return "伊利纯牛奶";
    if (code == "6902538006102") return "红牛";
    return "未知物品";
}

bool MainWindow::sendResultPacket(const QString &barcode, const QString &item)
{
    if (!m_serialPort || !m_serialPort->isOpen()) {
        appendSerialLog("[错误] 串口未打开，无法发送识别结果");
        return false;
    }

    QString wireItem = item;
    if (barcode == "NOT_FOUND") {
        wireItem = "NOT_FOUND";
    } else {
        wireItem = "OK";
    }

    QString msg = QString("RESULT:%1,%2\n").arg(barcode, wireItem);
    QByteArray payload = msg.toUtf8();

    qint64 written = m_serialPort->write(payload);
    if (written != payload.size()) {
        appendSerialLog("[错误] 串口写入失败或写入不完整");
        return false;
    }

    if (!m_serialPort->waitForBytesWritten(800)) {
        appendSerialLog("[错误] 串口发送超时");
        return false;
    }

    appendSerialLog("[TX] " + msg.trimmed());
    return true;
}

void MainWindow::takePhotoAndRecognize()
{
    if (m_isBusy) {
        appendSerialLog("[警告] 当前正在识别，忽略重复触发");
        return;
    }

    m_isBusy = true;
    m_captureBtn->setEnabled(false);
    m_statusLabel->setText("正在拍照并识别...");
    QApplication::processEvents();

    cv::Mat frame = m_camera->captureMat();
    if (frame.empty()) {
        m_statusLabel->setText("拍照失败");
        m_captureBtn->setEnabled(true);
        m_isBusy = false;
        return;
    }

    QString photoPath;
    QImage photo = m_camera->saveFrame(frame, "/mnt/sdcard/0330", &photoPath);

    if (!photo.isNull()) {
        m_thumbLabel->setPixmap(
            QPixmap::fromImage(photo).scaled(
                m_thumbLabel->size(),
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation
            )
        );
        appendSerialLog("[系统] 照片已保存：" + photoPath);
    } else {
        appendSerialLog("[警告] 照片保存失败，但继续尝试识别");
    }

    QString code;
    QString type;

    if (!m_recognizer.recognize(frame, code, type)) {
        m_barcodeValueLabel->setText("--");
        m_barcodeTypeLabel->setText("--");
        m_itemLabel->setText("--");
        m_statusLabel->setText("未识别到条形码");
        appendSerialLog("[系统] 本地识别失败，未找到有效条码");

        if (!sendResultPacket("NOT_FOUND", "NOT_FOUND")) {
            appendSerialLog("[错误] 未识别结果发送失败");
        }

        m_captureBtn->setEnabled(true);
        m_isBusy = false;
        return;
    }

    QString item = queryLabelByCode(code);

    m_barcodeValueLabel->setText(code);
    m_barcodeTypeLabel->setText(type.isEmpty() ? "UNKNOWN" : type);
    m_itemLabel->setText(item);
    m_statusLabel->setText("识别成功");

    appendSerialLog(QString("[系统] 本地识别成功：code=%1, type=%2, item=%3")
                    .arg(code, type, item));

    if (!sendResultPacket(code, item)) {
        m_statusLabel->setText("识别成功，但串口发送失败");
    }

    m_captureBtn->setEnabled(true);
    m_isBusy = false;
}

void MainWindow::serialPortReadyRead()
{
    if (!m_serialPort) {
        return;
    }

    QByteArray data = m_serialPort->readAll();
    if (data.isEmpty()) {
        return;
    }

    QByteArray filtered;
    filtered.reserve(data.size());

    for (int i = 0; i < data.size(); ++i) {
        char c = data.at(i);

        if (c == '\r' || c == '\n' || (c >= 32 && c <= 126)) {
            filtered.append(c);
        }
    }

    if (filtered.isEmpty()) {
        return;
    }

    m_rxBuffer.append(filtered);

    if (m_rxBuffer.size() > 256) {
        appendSerialLog("[警告] 串口缓存异常，已清空");
        m_rxBuffer.clear();
        return;
    }

    while (m_rxBuffer.contains('\n')) {
        int index = m_rxBuffer.indexOf('\n');
        QByteArray lineData = m_rxBuffer.left(index);
        m_rxBuffer.remove(0, index + 1);

        QString line = QString::fromUtf8(lineData).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        appendSerialLog("[RX] " + line);
        processResultLine(line);
    }
}

void MainWindow::processResultLine(const QString &line)
{
    QString workLine = line.trimmed();

    // 支持 ESP32 下行前缀：MQTT_RX:
    if (workLine.startsWith("MQTT_RX:")) {
        workLine = workLine.mid(QString("MQTT_RX:").length()).trimmed();
    }

    // ACK
    if (workLine.startsWith("ACK:")) {
        QString body = workLine.mid(4).trimmed();
        m_statusLabel->setText("上云成功：" + body);
        return;
    }

    // ERR
    if (workLine.startsWith("ERR:")) {
        QString body = workLine.mid(4).trimmed();
        m_statusLabel->setText("ESP32错误：" + body);
        return;
    }

    // 只处理 RESULT:
    if (!workLine.startsWith("RESULT:")) {
        return;
    }

    QString body = workLine.mid(QString("RESULT:").length()).trimmed();
    QStringList parts;

    if (body.contains(",")) {
        parts = body.split(",", QString::KeepEmptyParts);
    } else if (body.contains("|")) {
        parts = body.split("|", QString::KeepEmptyParts);
    }

    if (parts.size() < 2) {
        m_statusLabel->setText("收到结果格式不正确");
        return;
    }

    QString barcode = parts.at(0).trimmed();
    QString item = parts.at(1).trimmed();

    if (barcode.isEmpty()) {
        m_statusLabel->setText("收到结果格式不正确");
        return;
    }

    m_barcodeValueLabel->setText(barcode);

    // 未识别场景
    if (barcode == "NOT_FOUND") {
        m_barcodeTypeLabel->setText("--");
        m_itemLabel->setText("未识别到条形码");
        m_statusLabel->setText("未识别到条形码");
        return;
    }

    // 如果下发的是 OK / UNKNOWN / 空值，就按本地条码表查标签
    if (item.isEmpty() || item == "OK" || item == "UNKNOWN") {
        item = queryLabelByCode(barcode);
    }

    m_itemLabel->setText(item);
    m_barcodeTypeLabel->setText("EXTERNAL");
    m_statusLabel->setText("收到外部结果");
}

void MainWindow::onCameraError(const QString &msg)
{
    appendSerialLog("[错误] " + msg);
    m_statusLabel->setText(msg);
    m_captureBtn->setEnabled(true);
    m_isBusy = false;
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
#if __arm__
    if (event->key() == Qt::Key_VolumeDown ||
        event->key() == Qt::Key_Camera ||
        event->key() == Qt::Key_Enter ||
        event->key() == Qt::Key_Return) {
        appendSerialLog("[系统] 实体按键触发拍照识别");
        takePhotoAndRecognize();
        return;
    }
#else
    if (event->key() == Qt::Key_Return ||
        event->key() == Qt::Key_Enter ||
        event->key() == Qt::Key_Space) {
        appendSerialLog("[系统] 键盘按键触发拍照识别");
        takePhotoAndRecognize();
        return;
    }
#endif

    QMainWindow::keyPressEvent(event);
}
