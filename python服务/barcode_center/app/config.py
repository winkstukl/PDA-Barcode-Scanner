import os
from dotenv import load_dotenv

load_dotenv()

def _to_bool(value: str, default: bool = False) -> bool:
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}

class Settings:
    app_name: str = os.getenv("APP_NAME", "Barcode Center")

    database_url: str = os.getenv("DATABASE_URL", "sqlite:///./barcode.db")

    mqtt_host: str = os.getenv("MQTT_HOST", "127.0.0.1")
    mqtt_port: int = int(os.getenv("MQTT_PORT", "1883"))
    mqtt_username: str = os.getenv("MQTT_USERNAME", "")
    mqtt_password: str = os.getenv("MQTT_PASSWORD", "")
    mqtt_client_id: str = os.getenv("MQTT_CLIENT_ID", "barcode_web_server")

    mqtt_use_tls: bool = _to_bool(os.getenv("MQTT_USE_TLS", "false"))
    mqtt_tls_insecure: bool = _to_bool(os.getenv("MQTT_TLS_INSECURE", "false"))

    mqtt_topic_result: str = os.getenv("MQTT_TOPIC_RESULT", "mp157/barcode/result")
    mqtt_topic_cmd: str = os.getenv("MQTT_TOPIC_CMD", "mp157/barcode/cmd")

    mqtt_subscribe_qos: int = int(os.getenv("MQTT_SUBSCRIBE_QOS", "0"))
    mqtt_publish_qos: int = int(os.getenv("MQTT_PUBLISH_QOS", "0"))

settings = Settings()