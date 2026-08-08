"""
ESP32 智能家居系统 - Flask API + Web 面板 + MQTT 接收（合体版）
Week 3+4

一个程序搞定所有：
  - 后台自动连接 MQTT，接收 ESP32 数据 → 写入 SQLite
  - Web 面板实时显示（3 秒轮询）
  - 继电器远程控制 API

运行方式：
  python flask_api.py

依赖：
  pip install flask paho-mqtt
"""

import os
import ssl
import random
import string
import sqlite3
import json
import threading
import time
from flask import Flask, jsonify, request, send_from_directory
import paho.mqtt.client as mqtt

# ============ 配置 ============
DB_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "sensor_data.db")
CA_CERT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "emqxsl-ca.crt")
STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")

# MQTT 配置（和 ESP32 一致）
MQTT_BROKER = "b71f890f.ala.cn-shenzhen.emqxsl.cn"
MQTT_PORT   = 8883
MQTT_USER   = "esp32"
MQTT_PASS   = "123456"
MQTT_TOPIC_DATA = "sensor/data"      # ESP32 上报数据的 topic
MQTT_TOPIC_CMD  = "sensor/command"   # Web → ESP32 控制命令的 topic

app = Flask(__name__)


def get_db():
    """获取数据库连接"""
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    return conn


# ============================================================================
# 数据库初始化
# ============================================================================
def init_database():
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
            relay       INTEGER,
            led         INTEGER DEFAULT 0
        )
    """)
    # 老数据库兼容：如果没有 led 列就加上
    try:
        cursor.execute("ALTER TABLE sensor_data ADD COLUMN led INTEGER DEFAULT 0")
        print("[DB] 已添加 led 列")
    except sqlite3.OperationalError:
        pass  # 列已存在，跳过
    conn.commit()
    conn.close()
    print(f"[DB] 数据库就绪: {DB_FILE}")


# ============================================================================
# 后台 MQTT 接收线程
# ============================================================================
mqtt_data_count = 0  # 启动后收到的消息计数

def _on_connect(client, userdata, flags, reason_code, properties):
    if reason_code == 0:
        print("[MQTT] 已连接 EMQX Cloud，订阅 sensor/data ...")
        client.subscribe(MQTT_TOPIC_DATA)
        print("[MQTT] 等待 ESP32 数据...")
    else:
        print(f"[MQTT] 连接失败，错误码: {reason_code}")

def _on_disconnect(client, userdata, disconnect_flags, reason_code, properties):
    if reason_code != 0:
        print(f"[MQTT] 断开 (原因: {reason_code})，自动重连中...")

def _on_message(client, userdata, msg):
    global mqtt_data_count
    try:
        data = json.loads(msg.payload.decode("utf-8"))
        temp  = data.get("temp", None)
        humi  = data.get("humi", None)
        lux   = data.get("lux", None)
        pir   = data.get("pir", 0)
        relay = data.get("relay", 0)
        led   = data.get("led", 0)

        conn = sqlite3.connect(DB_FILE)
        conn.execute(
            "INSERT INTO sensor_data (temperature, humidity, lux, pir, relay, led) VALUES (?,?,?,?,?,?)",
            (temp, humi, lux, pir, relay, led)
        )
        conn.commit()
        conn.close()

        mqtt_data_count += 1
        pir_label = "有人!!!" if pir == 1 else "无人"
        if mqtt_data_count % 10 == 0:
            print(f"[MQTT] 已收到 {mqtt_data_count} 条数据 (最新: T:{temp} H:{humi}% L:{lux}lx PIR:{pir_label} R:{relay} LED:{led})")
    except Exception as e:
        print(f"[MQTT] 数据处理失败: {e}")

def _mqtt_loop():
    """后台线程：持续接收 MQTT 消息"""
    client_id = "flask_main_" + "".join(random.choices(string.ascii_lowercase + string.digits, k=6))
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=client_id,
        protocol=mqtt.MQTTv311,
        clean_session=True
    )
    client.username_pw_set(MQTT_USER, MQTT_PASS)
    client.tls_set(ca_certs=CA_CERT, tls_version=ssl.PROTOCOL_TLS_CLIENT)
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    client.on_connect = _on_connect
    client.on_disconnect = _on_disconnect
    client.on_message = _on_message

    while True:
        try:
            client.connect(MQTT_BROKER, MQTT_PORT, keepalive=30)
            client.loop_forever()
        except Exception as e:
            print(f"[MQTT] 连接异常: {e}，5 秒后重试...")
            time.sleep(5)

def start_mqtt_background():
    """启动后台 MQTT 接收线程"""
    t = threading.Thread(target=_mqtt_loop, daemon=True, name="mqtt-listener")
    t.start()
    print("[MQTT] 后台接收线程已启动")


# ============================================================================
# API 路由
# ============================================================================

@app.route("/api/latest", methods=["GET"])
def api_latest():
    """最新一条传感器数据"""
    try:
        conn = get_db()
        cursor = conn.cursor()
        cursor.execute("""
            SELECT id, timestamp, temperature, humidity, lux, pir, relay, led
            FROM sensor_data ORDER BY id DESC LIMIT 1
        """)
        row = cursor.fetchone()
        conn.close()
        if row is None:
            return jsonify({"ok": False, "msg": "数据库还没有数据"}), 200
        return jsonify({"ok": True, "data": {
            "id": row["id"], "timestamp": row["timestamp"],
            "temperature": row["temperature"], "humidity": row["humidity"],
            "lux": row["lux"], "pir": row["pir"], "relay": row["relay"],
            "led": row["led"] if "led" in row.keys() else 0
        }})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500


@app.route("/api/history", methods=["GET"])
def api_history():
    """历史数据"""
    try:
        limit = min(request.args.get("limit", 50, type=int), 200)
        conn = get_db()
        cursor = conn.cursor()
        cursor.execute(
            "SELECT id, timestamp, temperature, humidity, lux, pir, relay, led FROM sensor_data ORDER BY id DESC LIMIT ?",
            (limit,)
        )
        rows = cursor.fetchall()
        conn.close()
        history = [{
            "id": r["id"], "timestamp": r["timestamp"],
            "temperature": r["temperature"], "humidity": r["humidity"],
            "lux": r["lux"], "pir": r["pir"], "relay": r["relay"],
            "led": r["led"] if "led" in r.keys() else 0
        } for r in rows]
        return jsonify({"ok": True, "count": len(history), "data": history})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500


@app.route("/api/relay", methods=["POST"])
def api_relay():
    """远程控制继电器"""
    try:
        body = request.get_json(silent=True) or {}
        action = body.get("action", "").lower()
        if action not in ("on", "off", "auto"):
            return jsonify({"ok": False, "msg": "action 必须是 'on'、'off' 或 'auto'"}), 400

        cmd = json.dumps({"relay": action})
        ok = _publish_mqtt(cmd)
        if ok:
            return jsonify({"ok": True, "msg": f"已发送继电器 {action} 命令"})
        else:
            return jsonify({"ok": False, "msg": "MQTT 发送失败"}), 500
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500


@app.route("/api/led", methods=["POST"])
def api_led():
    """远程控制 LED 灯"""
    try:
        body = request.get_json(silent=True) or {}
        action = body.get("action", "").lower()
        if action not in ("on", "off", "auto"):
            return jsonify({"ok": False, "msg": "action 必须是 'on'、'off' 或 'auto'"}), 400

        cmd = json.dumps({"led": action})
        ok = _publish_mqtt(cmd)
        if ok:
            return jsonify({"ok": True, "msg": f"已发送 LED {action} 命令"})
        else:
            return jsonify({"ok": False, "msg": "MQTT 发送失败"}), 500
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500


@app.route("/api/stats", methods=["GET"])
def api_stats():
    """统计分析：平均值、最大最小值、时间跨度等"""
    try:
        conn = get_db()
        row = conn.execute("""
            SELECT
                COUNT(*)                    AS total,
                ROUND(AVG(temperature), 1)  AS temp_avg,
                ROUND(MIN(temperature), 1)  AS temp_min,
                ROUND(MAX(temperature), 1)  AS temp_max,
                ROUND(AVG(humidity), 1)     AS humi_avg,
                ROUND(MIN(humidity), 1)     AS humi_min,
                ROUND(MAX(humidity), 1)     AS humi_max,
                ROUND(AVG(lux), 1)          AS lux_avg,
                ROUND(MIN(lux), 1)          AS lux_min,
                ROUND(MAX(lux), 1)          AS lux_max,
                SUM(pir)                    AS pir_total,
                MIN(timestamp)              AS first_time,
                MAX(timestamp)              AS last_time
            FROM sensor_data
        """).fetchone()
        conn.close()

        return jsonify({"ok": True, "data": {
            "total": row["total"],
            "received_since_start": mqtt_data_count,
            "temperature": {
                "avg": row["temp_avg"], "min": row["temp_min"], "max": row["temp_max"]
            },
            "humidity": {
                "avg": row["humi_avg"], "min": row["humi_min"], "max": row["humi_max"]
            },
            "lux": {
                "avg": row["lux_avg"], "min": row["lux_min"], "max": row["lux_max"]
            },
            "pir_detections": row["pir_total"],
            "time_range": {
                "from": row["first_time"], "to": row["last_time"]
            }
        }})
    except Exception as e:
        return jsonify({"ok": False, "msg": str(e)}), 500


def _publish_mqtt(payload):
    """向 ESP32 发送 MQTT 命令（短连接）"""
    try:
        client_id = "flask_relay_" + "".join(random.choices(string.ascii_lowercase + string.digits, k=6))
        pub = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id,
                          protocol=mqtt.MQTTv311, clean_session=True)
        pub.username_pw_set(MQTT_USER, MQTT_PASS)
        pub.tls_set(ca_certs=CA_CERT, tls_version=ssl.PROTOCOL_TLS_CLIENT)
        pub.connect(MQTT_BROKER, MQTT_PORT, keepalive=10)
        pub.loop_start()
        info = pub.publish(MQTT_TOPIC_CMD, payload, qos=1)
        info.wait_for_publish(timeout=3)
        pub.loop_stop()
        pub.disconnect()
        return info.rc == mqtt.MQTT_ERR_SUCCESS
    except Exception as e:
        print(f"[MQTT] 命令发送失败: {e}")
        return False


# ============================================================================
# 静态页面
# ============================================================================

@app.route("/panel")
def panel():
    return send_from_directory(STATIC_DIR, "panel.html")


@app.route("/")
def index():
    return """<!DOCTYPE html>
<html lang="zh"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>智能家居 API</title>
<style>body{font-family:system-ui,sans-serif;max-width:600px;margin:40px auto;padding:20px;background:#1a1a2e;color:#eee}h1{color:#00d4aa;text-align:center}.card{background:#16213e;border-radius:10px;padding:20px;margin:15px 0}.card h2{margin-top:0;color:#e94560}code{background:#0f3460;padding:4px 8px;border-radius:4px}a{color:#00d4aa;text-decoration:none;font-size:16px}</style>
</head><body><h1>🏠 智能家居 API 服务</h1>
<div class="card" style="background:#00d4aa22;border:1px solid #00d4aa;"><h2>📱 Web 控制面板</h2><p>手机/电脑浏览器打开，实时查看传感器数据</p>
<a href="/panel" style="font-size:18px;font-weight:700;">👉 打开控制面板</a></div>
<div class="card"><h2>GET /api/latest</h2><p>最新一条传感器数据</p><a href="/api/latest">👉 试试看</a></div>
<div class="card"><h2>GET /api/history</h2><p>历史数据（?limit=10）</p><a href="/api/history?limit=10">👉 最近 10 条</a></div>
<div class="card"><h2>GET /api/stats</h2><p>系统统计</p><a href="/api/stats">👉 查看</a></div>
<div class="card"><h2>POST /api/relay</h2><p>控制继电器（on/off/auto）</p></div>
<div class="card"><h2>POST /api/led</h2><p>控制LED灯（on/off/auto）</p></div>
<p style="text-align:center;color:#888">手机浏览器: <code>http://你的电脑IP:5000/panel</code></p></body></html>"""


# ============================================================================
# 启动
# ============================================================================
if __name__ == "__main__":
    print("=" * 55)
    print("  智能家居系统 — Flask API + MQTT 接收")
    print("=" * 55)
    init_database()
    start_mqtt_background()
    print(f"  面板:    http://localhost:5000/panel")
    print(f"  首页:    http://localhost:5000")
    print(f"  API:     /api/latest  |  /api/history  |  /api/relay  |  /api/led  |  /api/stats")
    print("=" * 55)
    print()

    app.run(host="0.0.0.0", port=5000, debug=False)
