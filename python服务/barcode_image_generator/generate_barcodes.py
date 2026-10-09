import argparse
import csv
import re
from pathlib import Path
from typing import Dict, List

import barcode
from barcode.writer import ImageWriter, SVGWriter

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_INPUT = SCRIPT_DIR / "products_200_ean13_full.csv"
DEFAULT_OUTPUT_DIR = SCRIPT_DIR / "output" / "barcodes"
DEFAULT_INDEX_FILE = SCRIPT_DIR / "output" / "generated_files.csv"

REQUIRED_HEADERS = [
    "code_id",
    "barcode",
    "name",
    "qt_label",
    "spec",
    "price",
    "enabled",
]

def parse_bool(value: str) -> bool:
    if value is None:
        return False
    return str(value).strip().lower() in {"1", "true", "yes", "on"}

def safe_filename(text: str) -> str:
    text = str(text).strip()
    text = re.sub(r"[^0-9A-Za-z_.-]+", "_", text)
    text = re.sub(r"_+", "_", text).strip("._")
    return text or "unknown"

def normalize_headers(fieldnames: List[str]) -> List[str]:
    result = []
    for name in fieldnames or []:
        if name is None:
            result.append("")
        else:
            result.append(str(name).strip().replace("\\ufeff", ""))
    return result

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

def validate_ean13(barcode_value: str) -> str:
    raw = str(barcode_value)
    cleaned = raw.replace("\\ufeff", "").strip().strip('"').strip("'")
    cleaned = re.sub(r"\\D", "", cleaned)

    if len(cleaned) != 13:
        raise ValueError(
            f"barcode 不是合法的 13 位数字: 原始值={raw!r}, 清洗后={cleaned!r}"
        )

    first12 = cleaned[:12]
    last1 = cleaned[12]
    expected = calc_ean13_checksum(first12)

    if last1 != expected:
        raise ValueError(
            f"barcode 校验位错误: {cleaned}，期望最后一位为 {expected}"
        )

    return cleaned

def read_csv_rows(csv_file: Path) -> List[Dict[str, str]]:
    with open(csv_file, "r", encoding="utf-8-sig", newline="") as f:
        reader = csv.DictReader(f)

        if not reader.fieldnames:
            raise ValueError("CSV 表头读取失败")

        reader.fieldnames = normalize_headers(reader.fieldnames)

        if reader.fieldnames != REQUIRED_HEADERS:
            raise ValueError(
                f"CSV 表头不正确。\\n期望: {REQUIRED_HEADERS}\\n实际: {reader.fieldnames}"
            )

        rows: List[Dict[str, str]] = []
        for row in reader:
            normalized_row = {}
            for k, v in row.items():
                nk = (k or "").strip().replace("\\ufeff", "")
                value = (v or "").strip()

                if nk == "barcode":
                    value = value.replace("\\ufeff", "").strip().strip('"').strip("'")

                normalized_row[nk] = value

            if not any(normalized_row.values()):
                continue

            rows.append(normalized_row)

        if not rows:
            raise ValueError("CSV 数据为空")

        return rows

def build_output_name(row: Dict[str, str]) -> str:
    barcode_value = row["barcode"].strip()
    qt_label = (row.get("qt_label") or "").strip()
    name = (row.get("name") or "").strip()
    label_part = qt_label or name or "product"
    return f"{barcode_value}_{safe_filename(label_part)}"

def generate_barcode_svg(ean13_value: str, output_base: Path) -> str:
    code = barcode.get("ean13", ean13_value[:12], writer=SVGWriter())

    options = {
        "module_width": 0.33,
        "module_height": 25.0,
        "quiet_zone": 6.5,
        "font_size": 11,
        "text_distance": 4.5,
        "write_text": True,
        "background": "white",
        "foreground": "black",
    }

    saved_path = code.save(str(output_base), options=options)
    return saved_path

def generate_barcode_png(ean13_value: str, output_base: Path) -> str:
    code = barcode.get("ean13", ean13_value[:12], writer=ImageWriter())

    options = {
        "module_width": 0.33,
        "module_height": 25.0,
        "quiet_zone": 6.5,
        "font_size": 11,
        "text_distance": 4.5,
        "dpi": 600,
        "write_text": True,
        "background": "white",
        "foreground": "black",
    }

    saved_path = code.save(str(output_base), options=options)
    return saved_path

def write_index_csv(index_file: Path, items: List[Dict[str, str]]) -> None:
    index_file.parent.mkdir(parents=True, exist_ok=True)

    with open(index_file, "w", encoding="utf-8-sig", newline="") as f:
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
                "svg_file",
                "svg_path",
                "png_file",
                "png_path",
            ],
        )
        writer.writeheader()
        writer.writerows(items)

def main() -> None:
    parser = argparse.ArgumentParser(description="从完整 EAN-13 CSV 批量生成高质量条形码 SVG + PNG")
    parser.add_argument("--input", default=str(DEFAULT_INPUT), help="输入 CSV 文件路径")
    parser.add_argument("--output", default=str(DEFAULT_OUTPUT_DIR), help="输出图片目录")
    parser.add_argument("--index", default=str(DEFAULT_INDEX_FILE), help="生成结果索引文件")
    parser.add_argument("--include-disabled", action="store_true", help="包含 enabled=false 数据")
    args = parser.parse_args()

    input_file = Path(args.input).resolve()
    output_dir = Path(args.output).resolve()
    index_file = Path(args.index).resolve()

    if not input_file.exists():
        raise FileNotFoundError(f"找不到输入文件: {input_file}")

    svg_dir = output_dir / "svg"
    png_dir = output_dir / "png"

    svg_dir.mkdir(parents=True, exist_ok=True)
    png_dir.mkdir(parents=True, exist_ok=True)
    index_file.parent.mkdir(parents=True, exist_ok=True)

    rows = read_csv_rows(input_file)

    generated_items: List[Dict[str, str]] = []
    success_count = 0
    skip_count = 0
    fail_count = 0

    print("检测到数据行数:", len(rows))

    for row in rows:
        code_id = (row.get("code_id") or "").strip()
        barcode_value = (row.get("barcode") or "").strip()
        name = (row.get("name") or "").strip()
        qt_label = (row.get("qt_label") or "").strip()
        spec = (row.get("spec") or "").strip()
        price = (row.get("price") or "").strip()
        enabled_text = (row.get("enabled") or "true").strip()

        if not code_id:
            print("[跳过] 空 code_id 行 ->", row)
            skip_count += 1
            continue

        enabled = parse_bool(enabled_text)
        if (not enabled) and (not args.include_disabled):
            print(f"[跳过] disabled 商品: {code_id}")
            skip_count += 1
            continue

        try:
            barcode_value = validate_ean13(barcode_value)
            row["barcode"] = barcode_value

            output_name = build_output_name(row)

            svg_base = svg_dir / output_name
            png_base = png_dir / output_name

            svg_path = generate_barcode_svg(barcode_value, svg_base)
            png_path = generate_barcode_png(barcode_value, png_base)

            generated_items.append(
                {
                    "code_id": code_id,
                    "barcode": barcode_value,
                    "name": name,
                    "qt_label": qt_label,
                    "spec": spec,
                    "price": price,
                    "enabled": str(enabled).lower(),
                    "svg_file": Path(svg_path).name,
                    "svg_path": str(svg_path),
                    "png_file": Path(png_path).name,
                    "png_path": str(png_path),
                }
            )

            print(f"[成功] {code_id} -> {barcode_value}")
            success_count += 1

        except Exception as e:
            print(f"[失败] code_id={code_id}, barcode原始值={barcode_value!r}: {e}")
            fail_count += 1

    write_index_csv(index_file, generated_items)

    print()
    print("===== 生成完成 =====")
    print(f"成功: {success_count}")
    print(f"跳过: {skip_count}")
    print(f"失败: {fail_count}")
    print(f"SVG目录: {svg_dir}")
    print(f"PNG目录: {png_dir}")
    print(f"索引文件: {index_file}")

if __name__ == "__main__":
    main()
    