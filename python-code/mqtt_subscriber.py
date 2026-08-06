"""
ESP32 智能家居系统 - Python MQTT 订阅端
Week 2 Day 10

功能：
  1. 连接 EMQX Cloud MQTT Broker（TLS 加密）
  2. 订阅 Topic: sensor/data
  3. 接收 ESP32 发来的 JSON 数据，解析并打印

运行方式：
  python mqtt_subscriber.py

依赖：
  pip install paho-mqtt
"""

import json
import ssl
import os
import time
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

# CA 证书路径（和本脚本同目录）
CA_CERT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "emqxsl-ca.crt")

# 收到消息的计数
msg_count = 0
# 是否已经订阅过（避免重复订阅）
_is_subscribed = False


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
    """断开连接的回调（paho-mqtt 2.x VERSION2 签名：5个参数）"""
    if reason_code == 0:
        print("[INFO] 正常断开连接")
    else:
        print(f"[WARN] 连接断开，原因码: {reason_code}，正在自动重连...")


def on_message(client, userdata, msg):
    """收到消息的回调"""
    global msg_count
    msg_count += 1

    # 解析 JSON
    try:
        data = json.loads(msg.payload.decode("utf-8"))
        temp   = data.get("temp", "--")
        humi   = data.get("humi", "--")
        lux    = data.get("lux", "--")
        pir    = data.get("pir", 0)
        relay  = data.get("relay", 0)
    except json.JSONDecodeError:
        print(f"[ERROR] JSON 解析失败: {msg.payload}")
        return

    # 当前时间
    now = datetime.now().strftime("%H:%M:%S")

    # 打印
    print(f"[{now}] 第 {msg_count} 条数据")
    print(f"  温度:   {temp} C")
    print(f"  湿度:   {humi} %")
    print(f"  光照:   {lux} lx")
    print(f"  人体:   {'有人!!!' if pir == 1 else '无人'}")
    print(f"  继电器: {'ON' if relay == 1 else 'OFF'}")
    print("-" * 50)


def main():
    print("=" * 50)
    print("  ESP32 智能家居 - Python MQTT 订阅端")
    print("=" * 50)
    print(f"  Broker:  {MQTT_BROKER}")
    print(f"  端口:    {MQTT_PORT} (TLS)")
    print(f"  Topic:   {MQTT_TOPIC}")
    print(f"  CA 证书: {CA_CERT}")
    print("=" * 50)
    print()

    # 检查 CA 证书是否存在
    if not os.path.exists(CA_CERT):
        print(f"[ERROR] CA 证书不存在: {CA_CERT}")
        print("请确认 emqxsl-ca.crt 文件和本脚本在同一目录")
        return

    # 创建客户端（paho-mqtt 2.x API）
    # client_id 每次随机，避免和服务器残留会话冲突
    client_id = "python_sub_" + "".join(random.choices(string.ascii_lowercase + string.digits, k=6))
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=client_id,
        protocol=mqtt.MQTTv311,
        clean_session=True
    )

    # 设置用户名密码
    client.username_pw_set(MQTT_USER, MQTT_PASS)

    # 设置 TLS 加密
    client.tls_set(ca_certs=CA_CERT, tls_version=ssl.PROTOCOL_TLS_CLIENT)

    # 设置重连间隔（首次 1 秒，最大 30 秒）
    client.reconnect_delay_set(min_delay=1, max_delay=30)

    # 设置回调
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

    # 保持运行，持续接收消息
    print("[...] 等接收数据中...（按 Ctrl+C 退出）")
    print()
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print()
        print(f"[BYE] 共收到 {msg_count} 条数据，再见！")
        client.disconnect()


if __name__ == "__main__":
    main()
