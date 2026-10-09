#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include "barcode_recognizer.h"

class Camera;
class QLabel;
class QPushButton;
class QTextEdit;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void showCameraImage(const QImage &img);
    void takePhotoAndRecognize();
    void serialPortReadyRead();
    void onCameraError(const QString &msg);

private:
    void initUi();
    void initSerial();
    void initCamera();
    void appendSerialLog(const QString &text);
    bool sendResultPacket(const QString &barcode, const QString &item);
    void processResultLine(const QString &line);
    QString queryLabelByCode(const QString &code) const;

private:
    Camera *m_camera;
    QSerialPort *m_serialPort;
    BarcodeRecognizer m_recognizer;

    QWidget *m_centralWidget;
    QTabWidget *m_tabWidget;

    // 拍摄页
    QLabel *m_cameraLabel;
    QLabel *m_thumbLabel;
    QLabel *m_statusLabel;
    QLabel *m_barcodeValueLabel;
    QLabel *m_barcodeTypeLabel;
    QLabel *m_itemLabel;
    QPushButton *m_captureBtn;
    QPushButton *m_backBtn;

    // 串口调试页
    QTextEdit *m_serialLog;

    QByteArray m_rxBuffer;
    bool m_isBusy;
};

#endif // MAINWINDOW_H
