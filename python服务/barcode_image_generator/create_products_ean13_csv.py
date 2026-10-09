import csv
import re
from pathlib import Path
from typing import Dict, List

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_OUTPUT = SCRIPT_DIR / "products_200_ean13_full.csv"

EAN13_PREFIX = "690123456"

PRODUCT_TEMPLATES = [
    {"name": "Mineral Water", "spec": "250ml", "price": 3.45},
    {"name": "Pure Milk", "spec": "330ml", "price": 4.40},
    {"name": "Instant Noodles", "spec": "500ml", "price": 5.35},
    {"name": "Red Bull", "spec": "1L", "price": 6.30},
    {"name": "Shampoo", "spec": "1.5L", "price": 7.25},
    {"name": "Biscuits", "spec": "200g", "price": 8.20},
    {"name": "Potato Chips", "spec": "500g", "price": 9.15},
    {"name": "Green Tea", "spec": "750g", "price": 10.10},
    {"name": "Black Tea", "spec": "1kg", "price": 11.05},
    {"name": "Orange Juice", "spec": "12pcs", "price": 12.00},
    {"name": "Soy Milk", "spec": "250ml", "price": 12.95},
    {"name": "Yogurt Drink", "spec": "330ml", "price": 13.90},
    {"name": "Chocolate Bar", "spec": "500ml", "price": 14.85},
    {"name": "Toothpaste", "spec": "1L", "price": 15.80},
    {"name": "Laundry Detergent", "spec": "1.5L", "price": 16.75},
    {"name": "Hand Wash", "spec": "200g", "price": 17.70},
    {"name": "Facial Tissue", "spec": "500g", "price": 18.65},
    {"name": "Wet Wipes", "spec": "750g", "price": 19.60},
    {"name": "Cooking Oil", "spec": "1kg", "price": 20.55},
    {"name": "Soy Sauce", "spec": "12pcs", "price": 21.50},
]

def safe_label(text: str) -> str:
    text = str(text).strip().upper()
    text = re.sub(r"[^A-Z0-9]+", "_", text)
    text = re.sub(r"_+", "_", text).strip("_")
    return text or "PRODUCT"

def calc_ean13_checksum(first12: str) -> str:
    if len(first12) != 12 or not first12.isdigit():
        raise ValueError(f"EAN-13 前12位必须是12位数字，当前值: {first12}")

    total = 0
    for index, ch in enumerate(first12):
        digit = int(ch)
        position = index + 1
        if position % 2 == 0:
            total += digit * 3
        else:
            total += digit

    checksum = (10 - (total % 10)) % 10
    return str(checksum)

def build_ean13_from_code_id(code_id: str) -> str:
    code_id = str(code_id).strip()
    if not code_id.isdigit():
        raise ValueError(f"code_id 必须是数字: {code_id}")

    n = int(code_id)
    if n < 1 or n > 200:
        raise ValueError(f"code_id 必须在 1 到 200 之间: {code_id}")

    suffix3 = f"{n:03d}"
    first12 = EAN13_PREFIX + suffix3
    checksum = calc_ean13_checksum(first12)
    return first12 + checksum

def build_rows(count: int = 200) -> List[Dict[str, str]]:
    rows: List[Dict[str, str]] = []

    for i in range(1, count + 1):
        template = PRODUCT_TEMPLATES[(i - 1) % len(PRODUCT_TEMPLATES)]
        code_id = f"{i:03d}"
        barcode = build_ean13_from_code_id(code_id)

        base_name = template["name"]
        spec = template["spec"]
        price = template["price"]

        name = f"{base_name} {i}"
        qt_label = f"{safe_label(base_name)}_{i}"

        rows.append(
            {
                "code_id": code_id,
                "barcode": barcode,
                "name": name,
                "qt_label": qt_label,
                "spec": spec,
                "price": f"{price:.2f}",
                "enabled": "true",
            }
        )

    return rows

def write_csv(output_file: Path, rows: List[Dict[str, str]]) -> None:
    output_file.parent.mkdir(parents=True, exist_ok=True)

    with open(output_file, "w", encoding="utf-8-sig", newline="") as f:
        writer = csv.DictWriter(
            f,
            fieldnames=[
                "code_id",
                "barcode",
                "name",
                "qt_label",
                "spec",
                "price",
                "enabled",
            ],
        )
        writer.writeheader()
        writer.writerows(rows)

def main() -> None:
    rows = build_rows(200)
    write_csv(DEFAULT_OUTPUT, rows)

    print("CSV 生成完成")
    print(f"文件路径: {DEFAULT_OUTPUT}")
    print(f"总行数: {len(rows)}")
    print("前 5 条示例：")
    for item in rows[:5]:
        print(item)

if __name__ == "__main__":
    main()