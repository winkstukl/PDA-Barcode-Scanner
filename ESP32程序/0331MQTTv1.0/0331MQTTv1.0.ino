#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// ===================== WiFi 配置 =====================
static const char* WIFI_SSID = "WIFI_SSID";
static const char* WIFI_PASS = "WIFI_PASS";

// ===================== 私有云 MQTT 配置 =====================
static const char* MQTT_HOST = "云服务网址";
static const uint16_t MQTT_PORT = 8883;
static const char* MQTT_CLIENT_ID = "ESP32_Bridge_001";
static const char* MQTT_USERNAME = "MQTT_USERNAME";
static const char* MQTT_PASSWORD = "MQTT_PASSWORD";

// ===================== MQTT 主题 =====================
static const char* MQTT_PUB_TOPIC = "mp157/barcode/result";
static const char* MQTT_SUB_TOPIC = "mp157/barcode/cmd";

// ===================== 串口配置 =====================
// GPIO16: ESP32 RX2 <- MP157 TX
// GPIO17: ESP32 TX2 -> MP157 RX
static const int MP157_RX_PIN = 16;
static const int MP157_TX_PIN = 17;
static const uint32_t MP157_BAUD = 115200;

// ===================== TLS 配置 =====================
// true: 跳过证书校验，仅用于调试
// false: 使用 CA 证书校验
static const bool USE_INSECURE_TLS_FOR_TEST = true;

static const char* ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
)EOF";

// ===================== 全局对象 =====================
WiFiClientSecure espTlsClient;
PubSubClient mqttClient(espTlsClient);
HardwareSerial mpSerial(2);

String uartLine;
unsigned long lastWifiRetryMs = 0;
unsigned long lastMqttRetryMs = 0;

// ===================== 工具函数 =====================
void sendToQt(const String &msg)
{
    mpSerial.print(msg);
    mpSerial.print('\n');
}

String keepPrintableAscii(const String &src)
{
    String out;
    out.reserve(src.length());

    for (size_t i = 0; i < src.length(); ++i) {
        char c = src[i];
        if (c >= 32 && c <= 126) {
            out += c;
        }
    }

    return out;
}

String jsonEscape(const String &src)
{
    String out;
    out.reserve(src.length() + 16);

    for (size_t i = 0; i < src.length(); ++i) {
        char c = src[i];
        switch (c) {
        case '\"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            out += c;
            break;
        }
    }

    return out;
}

bool connectWiFi(uint32_t timeoutMs)
{
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    WiFi.disconnect(true, true);
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED) {
        delay(200);
        if (millis() - startMs >= timeoutMs) {
            return false;
        }
    }

    return true;
}

bool publishJson(const String &barcode,
                 const String &label,
                 const String &status,
                 const String &reason = "")
{
    String safeBarcode = keepPrintableAscii(barcode);
    String safeLabel   = keepPrintableAscii(label);
    String safeStatus  = keepPrintableAscii(status);
    String safeReason  = keepPrintableAscii(reason);

    safeBarcode.trim();
    safeLabel.trim();
    safeStatus.trim();
    safeReason.trim();

    if (safeBarcode.length() == 0) {
        safeBarcode = "NOT_FOUND";
    }

    if (safeLabel.length() == 0) {
        safeLabel = "UNKNOWN";
    }

    if (safeStatus.length() == 0) {
        safeStatus = "unknown";
    }

    String payload = "{";
    payload += "\"device_id\":\"ESP32_Bridge_001\",";
    payload += "\"barcode\":\"" + jsonEscape(safeBarcode) + "\",";
    payload += "\"label\":\"" + jsonEscape(safeLabel) + "\",";
    payload += "\"status\":\"" + jsonEscape(safeStatus) + "\"";

    if (safeReason.length() > 0) {
        payload += ",\"reason\":\"" + jsonEscape(safeReason) + "\"";
    }

    payload += "}";

    bool ok = mqttClient.publish(MQTT_PUB_TOPIC, payload.c_str());
    if (!ok) {
        sendToQt("ERR:PUB_FAIL");
        return false;
    }

    return true;
}

void reportFail(const String &reason,
                const String &barcode = "NOT_FOUND",
                const String &label = "NOT_FOUND")
{
    if (!mqttClient.connected()) {
        sendToQt("ERR:MQTT_DOWN");
        return;
    }

    publishJson(barcode, label, "fail", reason);
}

void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    String msg;
    msg.reserve(length + 1);

    for (unsigned int i = 0; i < length; ++i) {
        char c = (char)payload[i];
        if (c >= 32 && c <= 126) {
            msg += c;
        }
    }

    msg.trim();

    if (msg.length() == 0) {
        sendToQt("ERR:EMPTY_DOWNLINK");
        return;
    }

    // 下行原样透传给 QT，保留前缀便于区分来源
    sendToQt("MQTT_RX:" + msg);
}

bool connectMqtt()
{
    if (mqttClient.connected()) {
        return true;
    }

    bool ok = mqttClient.connect(
        MQTT_CLIENT_ID,
        MQTT_USERNAME,
        MQTT_PASSWORD
    );

    if (!ok) {
        return false;
    }

    mqttClient.subscribe(MQTT_SUB_TOPIC);
    sendToQt("STATUS:MQTT_OK");
    return true;
}

void processUartLine(const String &line)
{
    String cleanLine = keepPrintableAscii(line);
    cleanLine.trim();

    if (cleanLine.length() == 0) {
        sendToQt("ERR:EMPTY_LINE");
        reportFail("EMPTY_LINE");
        return;
    }

    if (!cleanLine.startsWith("RESULT:")) {
        sendToQt("ERR:BAD_PREFIX");
        reportFail("BAD_PREFIX");
        return;
    }

    // 先回 ACK，证明 ESP32 已收到 QT 上行串口数据
    sendToQt("ACK:UART_OK");

    if (!mqttClient.connected()) {
        sendToQt("ERR:MQTT_DOWN");
        return;
    }

    String body = cleanLine.substring(7);
    body.trim();

    int splitPos = body.indexOf(',');
    if (splitPos < 0) {
        splitPos = body.indexOf('|');
    }

    if (splitPos < 0) {
        sendToQt("ERR:BAD_FORMAT");
        reportFail("BAD_FORMAT");
        return;
    }

    String barcode = body.substring(0, splitPos);
    String label = body.substring(splitPos + 1);

    barcode.trim();
    label.trim();

    if (barcode.length() == 0) {
        sendToQt("ERR:EMPTY_BARCODE");
        reportFail("EMPTY_BARCODE");
        return;
    }

    if (label.length() == 0) {
        label = "UNKNOWN";
    }

    // 业务失败场景：明确未识别
    if (barcode == "NOT_FOUND" || label == "NOT_FOUND") {
        if (publishJson(barcode, label, "fail", "SCAN_NOT_FOUND")) {
            sendToQt("ACK:PUB_OK");
        }
        return;
    }

    // 正常成功上报
    if (publishJson(barcode, label, "ok")) {
        sendToQt("ACK:PUB_OK");
    }
}

void setup()
{
    mpSerial.begin(MP157_BAUD, SERIAL_8N1, MP157_RX_PIN, MP157_TX_PIN);
    delay(1000);

    sendToQt("STATUS:ESP32_BOOT");

    if (USE_INSECURE_TLS_FOR_TEST) {
        espTlsClient.setInsecure();
    } else {
        espTlsClient.setCACert(ROOT_CA);
    }

    espTlsClient.setTimeout(8000);

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(512);

    uartLine.reserve(256);

    bool wifiOk = connectWiFi(10000);
    if (wifiOk) {
        sendToQt("STATUS:WIFI_OK");
    } else {
        sendToQt("ERR:WIFI_FAIL");
    }

    if (WiFi.status() == WL_CONNECTED) {
        if (connectMqtt()) {
            sendToQt("STATUS:MQTT_CONNECTED");
        } else {
            sendToQt("ERR:MQTT_CONNECT_FAIL");
        }
    }
}

void loop()
{
    unsigned long now = millis();

    // WiFi 每 5 秒重连一次，避免死循环
    if (WiFi.status() != WL_CONNECTED) {
        if (now - lastWifiRetryMs >= 5000) {
            lastWifiRetryMs = now;

            if (connectWiFi(5000)) {
                sendToQt("STATUS:WIFI_RECONNECTED");
            } else {
                sendToQt("ERR:WIFI_DOWN");
            }
        }

        delay(10);
        return;
    }

    // MQTT 每 5 秒重连一次，避免死循环
    if (!mqttClient.connected()) {
        if (now - lastMqttRetryMs >= 5000) {
            lastMqttRetryMs = now;

            if (connectMqtt()) {
                sendToQt("STATUS:MQTT_RECONNECTED");
            } else {
                sendToQt("ERR:MQTT_DOWN");
            }
        }

        delay(10);
        return;
    }

    mqttClient.loop();

    while (mpSerial.available() > 0) {
        char c = (char)mpSerial.read();

        if (c == '\r') {
            continue;
        }

        if (c == '\n') {
            uartLine.trim();

            if (uartLine.length() > 0) {
                processUartLine(uartLine);
            }

            uartLine = "";
            continue;
        }

        // 只接收可打印 ASCII，避免乱码污染缓冲区
        if (c >= 32 && c <= 126) {
            uartLine += c;
        }

        if (uartLine.length() > 255) {
            uartLine = "";
            sendToQt("ERR:LINE_TOO_LONG");
            reportFail("LINE_TOO_LONG");
        }
    }

    delay(10);
}
