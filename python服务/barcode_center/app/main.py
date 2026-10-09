from app.import_products_csv import import_products_from_generated_csv
from typing import List, Optional

from fastapi import FastAPI, Depends, HTTPException, Query, Request
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles
from sqlalchemy.orm import Session
from starlette.templating import Jinja2Templates

from app.config import settings
from app.database import engine, get_db
from app import models
from app.mqtt_service import mqtt_service
from app.schemas import (
    ProductCreate,
    ProductOut,
    ProductUpdate,
    ScanLogOut,
    MQTTStatusOut,
    PublishResultOut,
)

models.Base.metadata.create_all(bind=engine)

app = FastAPI(title=settings.app_name)
templates = Jinja2Templates(directory="app/templates")
app.mount("/static", StaticFiles(directory="app/static"), name="static")

def clean_text(value: Optional[str]) -> Optional[str]:
    if value is None:
        return None
    value = value.strip()
    return value if value else None

def normalize_product_payload(payload: ProductCreate | ProductUpdate):
    data = payload.model_dump()
    data["barcode"] = payload.barcode.strip()
    data["name"] = payload.name.strip()
    data["qt_label"] = payload.qt_label.strip()
    data["spec"] = clean_text(payload.spec)

    if not data["barcode"]:
        raise HTTPException(status_code=400, detail="barcode 不能为空")
    if not data["name"]:
        raise HTTPException(status_code=400, detail="name 不能为空")
    if not data["qt_label"]:
        raise HTTPException(status_code=400, detail="qt_label 不能为空")

    return data



@app.on_event("startup")
def on_startup():
    import_products_from_generated_csv(force_reload=True)
    mqtt_service.start()

@app.on_event("shutdown")
def on_shutdown():
    mqtt_service.stop()

@app.get("/", response_class=HTMLResponse)
def index(request: Request):
    return templates.TemplateResponse(
        "index.html",
        {
            "request": request,
            "app_name": settings.app_name,
        },
    )

@app.get("/api/status", response_model=MQTTStatusOut)
def get_status():
    return mqtt_service.get_status()

@app.get("/api/products", response_model=List[ProductOut])
def list_products(db: Session = Depends(get_db)):
    return db.query(models.Product).order_by(models.Product.id.desc()).all()

@app.post("/api/products", response_model=ProductOut)
def create_product(payload: ProductCreate, db: Session = Depends(get_db)):
    data = normalize_product_payload(payload)

    existing = db.query(models.Product).filter(models.Product.barcode == data["barcode"]).first()
    if existing:
        raise HTTPException(status_code=400, detail="该条码已存在")

    product = models.Product(**data)
    db.add(product)
    db.commit()
    db.refresh(product)
    return product

@app.put("/api/products/{product_id}", response_model=ProductOut)
def update_product(product_id: int, payload: ProductUpdate, db: Session = Depends(get_db)):
    product = db.query(models.Product).filter(models.Product.id == product_id).first()
    if not product:
        raise HTTPException(status_code=404, detail="商品不存在")

    data = normalize_product_payload(payload)

    other = (
        db.query(models.Product)
        .filter(models.Product.barcode == data["barcode"], models.Product.id != product_id)
        .first()
    )
    if other:
        raise HTTPException(status_code=400, detail="该条码已被其他商品使用")

    product.barcode = data["barcode"]
    product.name = data["name"]
    product.qt_label = data["qt_label"]
    product.spec = data["spec"]
    product.price = data["price"]
    product.enabled = data["enabled"]

    db.commit()
    db.refresh(product)
    return product

@app.delete("/api/products/{product_id}")
def delete_product(product_id: int, db: Session = Depends(get_db)):
    product = db.query(models.Product).filter(models.Product.id == product_id).first()
    if not product:
        raise HTTPException(status_code=404, detail="商品不存在")

    db.delete(product)
    db.commit()
    return {"ok": True}

@app.post("/api/products/{product_id}/publish", response_model=PublishResultOut)
def publish_product_to_qt(product_id: int, db: Session = Depends(get_db)):
    product = db.query(models.Product).filter(models.Product.id == product_id).first()
    if not product:
        raise HTTPException(status_code=404, detail="商品不存在")

    result = mqtt_service.publish_qt_result(
        barcode=product.barcode,
        qt_label=(product.qt_label or product.name),
    )

    if not result["ok"]:
        raise HTTPException(status_code=500, detail=result["reason"] or "发送失败")

    return result

@app.get("/api/scans", response_model=List[ScanLogOut])
def list_scans(
    limit: int = Query(50, ge=1, le=200),
    db: Session = Depends(get_db),
):
    return (
        db.query(models.ScanLog)
        .order_by(models.ScanLog.id.desc())
        .limit(limit)
        .all()
    )