import csv
from pathlib import Path

from app.database import Base, engine, SessionLocal
from app.models import Product

CSV_FILE = Path(__file__).resolve().parent.parent / "generated_files.csv"

def import_products_from_generated_csv(force_reload: bool = False):
    Base.metadata.create_all(bind=engine)

    db = SessionLocal()
    try:
        if force_reload:
            db.query(Product).delete()
            db.commit()

        current_count = db.query(Product).count()
        if current_count > 0 and not force_reload:
            print(f"products 表已有 {current_count} 条数据，跳过 CSV 导入。")
            return

        items = []

        with open(CSV_FILE, "r", encoding="utf-8-sig", newline="") as f:
            reader = csv.DictReader(f)

            for row in reader:
                barcode = (row.get("barcode") or "").strip()
                name = (row.get("name") or "").strip()
                qt_label = (row.get("qt_label") or "").strip()
                spec = (row.get("spec") or "").strip() or None
                price_text = (row.get("price") or "").strip()
                enabled_text = (row.get("enabled") or "true").strip().lower()

                if not barcode or not name or not qt_label:
                    continue

                price = float(price_text) if price_text else None
                enabled = enabled_text in {"1", "true", "yes", "on"}

                items.append(
                    Product(
                        barcode=barcode,
                        name=name,
                        qt_label=qt_label,
                        spec=spec,
                        price=price,
                        enabled=enabled,
                    )
                )

        if items:
            db.add_all(items)
            db.commit()

        print(f"已成功从 generated_files.csv 导入 {len(items)} 条商品数据。")

    except Exception as e:
        db.rollback()
        print("CSV 导入失败：", e)
        raise
    finally:
        db.close()

if __name__ == "__main__":
    import_products_from_generated_csv(force_reload=True)