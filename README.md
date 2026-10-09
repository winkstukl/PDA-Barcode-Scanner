# 轻量化 PDA 条形码扫描终端

基于 STM32MP157 + Qt5、ESP32 与 FastAPI 的「端-边-云」三层架构一维条形码扫描终端，旨在降低商用 PDA 的硬件成本并实现开放架构。

## 目录结构

- `QT程序/` —— 端层：Qt5 界面 + OpenCV 图像处理 + ZBar 解码（运行于 ATK-MP157 开发板）
- `ESP32程序/` —— 边层：串口通信 + MQTT 上云
- `python服务/` —— 云层：FastAPI 后端 + SQLite 数据库
- `运行BASH.txt` —— 板端部署脚本（挂载 SD 卡、配置库链接并启动程序）

## 板端部署

将编译产物与 `libzbar.so.0.3.0` 拷贝到开发板后，按 `运行BASH.txt` 中的命令依次执行。

## 环境配置

云端服务的配置项见 `python服务/barcode_center/.env.example`，复制为 `.env` 并填入真实的 MQTT 服务器地址、账号与密码后即可运行（`.env` 不在版本库中）。
