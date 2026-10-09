from datetime import datetime
from typing import Optional

from pydantic import BaseModel, Field, ConfigDict

class ProductBase(BaseModel):
    barcode: str = Field(..., max_length=64)
    name: str = Field(..., max_length=255)
    qt_label: str = Field(..., max_length=255, description="发给QT显示的标签，建议ASCII")
    spec: Optional[str] = Field(None, max_length=255)
    price: Optional[float] = None
    enabled: bool = True

class ProductCreate(ProductBase):
    pass

class ProductUpdate(ProductBase):
    pass

class ProductOut(ProductBase):
    id: int
    created_at: datetime
    updated_at: datetime

    model_config = ConfigDict(from_attributes=True)

class ScanLogOut(BaseModel):
    id: int
    barcode: str
    status: str
    source_status: Optional[str] = None
    matched: bool
    product_name: Optional[str] = None
    qt_payload: Optional[str] = None
    raw_payload: str
    created_at: datetime

    model_config = ConfigDict(from_attributes=True)

class MQTTStatusOut(BaseModel):
    connected: bool
    host: str
    port: int
    topic_result: str
    topic_cmd: str
    last_error: Optional[str] = None
    last_message_at: Optional[datetime] = None
    last_incoming_payload: Optional[str] = None
    last_outgoing_payload: Optional[str] = None

class PublishResultOut(BaseModel):
    ok: bool
    payload: str
    reason: Optional[str] = None