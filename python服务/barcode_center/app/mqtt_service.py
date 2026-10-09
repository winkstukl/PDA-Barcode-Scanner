import json
import logging
from datetime import datetime
from threading import Lock
from typing import Optional

import paho.mqtt.client as mqtt

from app.config import settings
from app.database import SessionLocal
from app.models import Product, ScanLog

logger = logging.getLogger(__name__)

class MQTTService:
    def __init__(self):
        self.client = mqtt.Client(client_id=settings.mqtt_client_id, clean_session=True)

        self.connected = False
        self.last_error: Optional[str] = None
        self.last_message_at: Optional[datetime] = None
        self.last_incoming_payload: Optional[str] = None
        self.last_outgoing_payload: Optional[str] = None

        self._started = False
        self._lock = Lock()

    def start(self):
        if self._started:
            return

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

        if settings.mqtt_username:
            self.client.username_pw_set(settings.mqtt_username, settings.mqtt_password)

        if settings.mqtt_use_tls:
            self.client.tls_set()
            self.client.tls_insecure_set(settings.mqtt_tls_insecure)

        self.client.connect_async(settings.mqtt_host, settings.mqtt_port, keepalive=60)
        self.client.loop_start()

        self._started = True
        logger.info("MQTT service started")

    def stop(self):
        if not self._started:
            return

        try:
            self.client.loop_stop()
            self.client.disconnect()
        finally:
            self._started = False
            with self._lock:
                self.connected = False

    def get_status(self):
        with self._lock:
            return {
                "connected": self.connected,
                "host": settings.mqtt_host,
                "port": settings.mqtt_port,
                "topic_result": settings.mqtt_topic_result,
                "topic_cmd": settings.mqtt_topic_cmd,
                "last_error": self.last_error,
                "last_message_at": self.last_message_at,
                "last_incoming_payload": self.last_incoming_payload,
                "last_outgoing_payload": self.last_outgoing_payload,
            }

    def publish_qt_result(self, barcode: str, qt_label: str):
        payload = f"RESULT:{barcode},{qt_label}"

        if not self.connected:
            reason = "MQTT 未连接"
            with self._lock:
                self.last_error = reason
                self.last_outgoing_payload = payload
            return {
                "ok": False,
                "payload": payload,
                "reason": reason,
            }

        info = self.client.publish(
            settings.mqtt_topic_cmd,
            payload,
            qos=settings.mqtt_publish_qos,
        )

        ok = info.rc == mqtt.MQTT_ERR_SUCCESS
        reason = None if ok else f"publish rc={info.rc}"

        with self._lock:
            self.last_outgoing_payload = payload
            self.last_message_at = datetime.utcnow()
            self.last_error = reason

        return {
            "ok": ok,
            "payload": payload,
            "reason": reason,
        }

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            with self._lock:
                self.connected = True
                self.last_error = None

            client.subscribe(settings.mqtt_topic_result, qos=settings.mqtt_subscribe_qos)
            logger.info("MQTT connected and subscribed to %s", settings.mqtt_topic_result)
        else:
            with self._lock:
                self.connected = False
                self.last_error = f"连接失败 rc={rc}"
            logger.error("MQTT connect failed rc=%s", rc)

    def _on_disconnect(self, client, userdata, rc):
        with self._lock:
            self.connected = False
            if rc != 0:
                self.last_error = f"意外断开 rc={rc}"
        logger.warning("MQTT disconnected rc=%s", rc)

    def _on_message(self, client, userdata, msg):
        payload = msg.payload.decode("utf-8", errors="ignore")

        with self._lock:
            self.last_incoming_payload = payload
            self.last_message_at = datetime.utcnow()

        logger.info("MQTT recv topic=%s payload=%s", msg.topic, payload)

        if msg.topic == settings.mqtt_topic_result:
            self._handle_result_message(payload)

    def _handle_result_message(self, payload: str):
        db = SessionLocal()
        try:
            try:
                data = json.loads(payload)
            except Exception as exc:
                with self._lock:
                    self.last_error = f"JSON 解析失败: {exc}"

                log = ScanLog(
                    barcode="PARSE_ERROR",
                    status="bad_json",
                    source_status="bad_json",
                    matched=False,
                    product_name=None,
                    qt_payload=None,
                    raw_payload=payload,
                )
                db.add(log)
                db.commit()
                return

            barcode = str(data.get("barcode", "")).strip() or "NOT_FOUND"
            source_status = str(data.get("status", "")).strip() or "unknown"

            matched = False
            product_name = None
            qt_payload = None

            if barcode != "NOT_FOUND":
                product = (
                    db.query(Product)
                    .filter(Product.barcode == barcode, Product.enabled.is_(True))
                    .first()
                )
            else:
                product = None

            if product:
                matched = True
                product_name = product.name

                result = self.publish_qt_result(
                    barcode=product.barcode,
                    qt_label=(product.qt_label or product.name or "UNKNOWN"),
                )
                qt_payload = result["payload"]

            if barcode == "NOT_FOUND":
                status = "not_found"
            elif matched:
                status = "matched"
            else:
                status = "unmatched"

            log = ScanLog(
                barcode=barcode,
                status=status,
                source_status=source_status,
                matched=matched,
                product_name=product_name,
                qt_payload=qt_payload,
                raw_payload=payload,
            )
            db.add(log)
            db.commit()

        except Exception as exc:
            db.rollback()
            with self._lock:
                self.last_error = str(exc)
            logger.exception("handle mqtt message error: %s", exc)
        finally:
            db.close()

mqtt_service = MQTTService()