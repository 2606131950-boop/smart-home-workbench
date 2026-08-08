"""
ESP32 智能家居系统 - MQTT → SQLite 数据存储
Week 3 Day 1

功能：
  1. 连接 EMQX Cloud MQTT Broker（TLS 加密）
  2. 订阅 Topic: sensor/data
  3. 接收 ESP32 发来的 JSON 数据
  4. 存入 SQLite 数据库 sensor_data.db

运行方式：
  python mqtt_to_sqlite.py

依赖：
  pip install paho-mqtt
  （sqlite3 是 Python 自带，无需安装）
"""

import json
import ssl
import os
import sqlite3
import random
import string
from datetime import datetime
import paho.mqtt.client as mqtt

# ============ MQTT 配置（和 ESP32 端一致）============
MQTT_BROKER = "b71f890f.ala.cn-shenzhen.emqxsl.cn"
MQTT_PORT   = 8883
MQTT_USER   = "esp32"
MQTT_PASS   = "123456"
MQTT_TOPIC  = "sensor/data"

# ============ 数据库配置 ============
# 数据库文件和本脚本同目录
DB_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sensor_data.db")

# CA 证书路径
CA_CERT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "emqxsl-ca.crt")

# 消息计数
msg_count = 0
_is_subscribed = False


def init_database():
    """创建数据库和表（如果不存在）"""
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()

    cursor.execute("""
        CREATE TABLE IF NOT EXISTS sensor_data (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP,
            temperature REAL,
            humidity    REAL,
            lux         REAL,
            pir         INTEGER,
            relay       INTEGER
        )
    """)

    conn.commit()
    conn.close()
    print(f"[OK] 数据库已就绪: {DB_FILE}")


def save_to_db(temp, humi, lux, pir, relay):
    """将一条传感器数据存入 SQLite"""
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()

        cursor.execute("""
            INSERT INTO sensor_data (temperature, humidity, lux, pir, relay)
            VALUES (?, ?, ?, ?, ?)
        """, (temp, humi, lux, pir, relay))

        conn.commit()
        conn.close()
        return True
    except Exception as e:
        print(f"[ERROR] 数据库写入失败: {e}")
        return False


def show_stats():
    """显示数据库统计信息"""
    try:
        conn = sqlite3.connect(DB_FILE)
        cursor = conn.cursor()

        # 总数
        cursor.execute("SELECT COUNT(*) FROM sensor_data")
        total = cursor.fetchone()[0]

        # 最新一条
        cursor.execute("""
            SELECT timestamp, temperature, humidity, lux, pir, relay
            FROM sensor_data ORDER BY id DESC LIMIT 1
        """)
        row = cursor.fetchone()

        conn.close()

        print(f"\n  [数据库] 共 {total} 条记录")
        if row:
            print(f"  最新: {row[0]} | T:{row[1]}C H:{row[2]}% L:{row[3]}lx PIR:{row[4]} RLY:{row[5]}")
    except Exception as e:
        print(f"\n  [数据库] 查询失败: {e}")


def on_connect(client, userdata, flags, reason_code, properties):
    """连接成功后的回调"""
    global _is_subscribed
    if reason_code == 0:
        print("[OK] 已连接 EMQX Cloud!")
        if not _is_subscribed:
            print(f"[OK] 正在订阅 Topic: {MQTT_TOPIC}")
            print("=" * 50)
            client.subscribe(MQTT_TOPIC)
            _is_subscribed = True
    else:
        print(f"[FAIL] 连接失败，错误码: {reason_code}")


def on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    """断开连接的回调"""
    if reason_code == 0:
        print("[INFO] 正常断开连接")
    else:
        print(f"[WARN] 连接断开，原因码: {reason_code}，正在自动重连...")


def on_message(client, userdata, msg):
    """收到消息的回调：解析 + 打印 + 存数据库"""
    global msg_count
    msg_count += 1

    # 解析 JSON
    try:
        data = json.loads(msg.payload.decode("utf-8"))
        temp   = data.get("temp", None)
        humi   = data.get("humi", None)
        lux    = data.get("lux", None)
        pir    = data.get("pir", 0)
        relay  = data.get("relay", 0)
    except json.JSONDecodeError:
        print(f"[ERROR] JSON 解析失败: {msg.payload}")
        return

    # 当前时间
    now = datetime.now().strftime("%H:%M:%S")

    # 终端打印
    pir_label = "有人!!!" if pir == 1 else "无人"
    relay_label = "ON" if relay == 1 else "OFF"
    print(f"[{now}] #{msg_count}  T:{temp}C  H:{humi}%  L:{lux}lx  PIR:{pir_label}  RLY:{relay_label}")

    # 存数据库
    ok = save_to_db(temp, humi, lux, pir, relay)
    if ok and msg_count % 6 == 0:
        # 每 6 条（约每 30 秒）显示一次数据库统计
        show_stats()


def main():
    print("=" * 55)
    print("  ESP32 智能家居 - MQTT → SQLite 数据存储")
    print("=" * 55)
    print(f"  Broker:  {MQTT_BROKER}")
    print(f"  端口:    {MQTT_PORT} (TLS)")
    print(f"  Topic:   {MQTT_TOPIC}")
    print(f"  数据库:  {DB_FILE}")
    print("=" * 55)
    print()

    # 初始化数据库
    init_database()

    # 检查 CA 证书
    if not os.path.exists(CA_CERT):
        print(f"[ERROR] CA 证书不存在: {CA_CERT}")
        print("请确认 emqxsl-ca.crt 文件和本脚本在同一目录")
        return

    # 创建 MQTT 客户端
    client_id = "py_sqlite_" + "".join(random.choices(string.ascii_lowercase + string.digits, k=6))
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=client_id,
        protocol=mqtt.MQTTv311,
        clean_session=True
    )

    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.tls_set(ca_certs=CA_CERT, tls_version=ssl.PROTOCOL_TLS_CLIENT)
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    client.on_connect = on_connect
    client.on_disconnect = on_disconnect
    client.on_message = on_message

    # 连接
    print("[...] 正在连接 EMQX Cloud...")
    try:
        client.connect(MQTT_BROKER, MQTT_PORT, keepalive=30)
    except Exception as e:
        print(f"[ERROR] 连接失败: {e}")
        return

    # 持续运行
    print("[...] 等待接收数据...（按 Ctrl+C 退出）")
    print()
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print()
        print(f"\n[BYE] 共收到 {msg_count} 条数据")
        show_stats()
        print("\n数据库文件: " + DB_FILE)
        client.disconnect()


if __name__ == "__main__":
    main()
