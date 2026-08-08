"""
ESP32 智能家居系统 - 数据库查询工具
用法：
  python query_data.py          # 查看最新 10 条
  python query_data.py --all    # 查看全部
  python query_data.py --stats  # 只看统计
"""

import os
import sqlite3
import sys

DB_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sensor_data.db")


def show_stats(cursor):
    """显示统计信息"""
    cursor.execute("SELECT COUNT(*) FROM sensor_data")
    total = cursor.fetchone()[0]
    print(f"\n  总记录数: {total}")

    if total == 0:
        return

    cursor.execute("""
        SELECT
            MIN(temperature), MAX(temperature), AVG(temperature),
            MIN(humidity), MAX(humidity), AVG(humidity),
            MIN(lux), MAX(lux)
        FROM sensor_data
    """)
    t_min, t_max, t_avg, h_min, h_max, h_avg, l_min, l_max = cursor.fetchone()

    print(f"  温度范围: {t_min:.1f} ~ {t_max:.1f} C  (平均 {t_avg:.1f})")
    print(f"  湿度范围: {h_min:.0f} ~ {h_max:.0f} %  (平均 {h_avg:.0f})")
    print(f"  光照范围: {l_min:.0f} ~ {l_max:.0f} lx")

    cursor.execute("SELECT COUNT(*) FROM sensor_data WHERE pir = 1")
    pir_count = cursor.fetchone()[0]
    print(f"  检测到人: {pir_count} 次")

    cursor.execute("SELECT COUNT(*) FROM sensor_data WHERE relay = 1")
    relay_count = cursor.fetchone()[0]
    print(f"  继电器开: {relay_count} 次")

    cursor.execute("""
        SELECT timestamp, temperature, humidity, lux, pir, relay
        FROM sensor_data ORDER BY id DESC LIMIT 1
    """)
    row = cursor.fetchone()
    if row:
        print(f"\n  最新一条: {row[0]}")
        print(f"    温度: {row[1]} C | 湿度: {row[2]} % | 光照: {row[3]} lx")
        print(f"    人体: {'有人' if row[4]==1 else '无人'} | 继电器: {'ON' if row[5]==1 else 'OFF'}")


def show_rows(cursor, limit=None):
    """显示数据行"""
    if limit:
        cursor.execute("""
            SELECT id, timestamp, temperature, humidity, lux, pir, relay
            FROM sensor_data ORDER BY id DESC LIMIT ?
        """, (limit,))
    else:
        cursor.execute("""
            SELECT id, timestamp, temperature, humidity, lux, pir, relay
            FROM sensor_data ORDER BY id DESC
        """)

    rows = cursor.fetchall()
    if not rows:
        print("\n  数据库为空，还没有记录。")
        return

    print(f"\n{'ID':>4}  {'时间':<20}  {'温度':>6}  {'湿度':>5}  {'光照':>6}  {'人体':>4}  {'继电器':>4}")
    print("-" * 65)
    for row in rows:
        pir_label = "有人" if row[5] == 1 else "无人"
        relay_label = "ON" if row[6] == 1 else "OFF"
        print(f"{row[0]:>4}  {row[1]:<20}  {row[2]:>5}C  {row[3]:>4}%  {row[4]:>5}lx  {pir_label:>4}  {relay_label:>4}")


def main():
    if not os.path.exists(DB_FILE):
        print(f"[ERROR] 数据库文件不存在: {DB_FILE}")
        print("请先运行 mqtt_to_sqlite.py 收集数据")
        return

    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()

    print("=" * 50)
    print("  ESP32 智能家居 - 数据库查询")
    print(f"  文件: {DB_FILE}")
    print("=" * 50)

    if "--stats" in sys.argv:
        show_stats(cursor)
    elif "--all" in sys.argv:
        show_stats(cursor)
        show_rows(cursor)
    else:
        show_stats(cursor)
        show_rows(cursor, limit=10)

    conn.close()


if __name__ == "__main__":
    main()
