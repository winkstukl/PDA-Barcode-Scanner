from datetime import datetime

from sqlalchemy import Column, Integer, String, Float, Boolean, DateTime, Text

from app.database import Base

class Product(Base):
    __tablename__ = "products"

    id = Column(Integer, primary_key=True, index=True)
    barcode = Column(String(64), unique=True, nullable=False, index=True)
    name = Column(String(255), nullable=False)
    qt_label = Column(String(255), nullable=False)
    spec = Column(String(255), nullable=True)
    price = Column(Float, nullable=True)
    enabled = Column(Boolean, default=True, nullable=False)

    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow, nullable=False)

class ScanLog(Base):
    __tablename__ = "scan_logs"

    id = Column(Integer, primary_key=True, index=True)
    barcode = Column(String(64), nullable=False, index=True)
    status = Column(String(64), nullable=False)
    source_status = Column(String(64), nullable=True)
    matched = Column(Boolean, default=False, nullable=False)
    product_name = Column(String(255), nullable=True)
    qt_payload = Column(Text, nullable=True)
    raw_payload = Column(Text, nullable=False)

    created_at = Column(DateTime, default=datetime.utcnow, nullable=False)