import paho.mqtt.publish as publish
import struct
import random
import time

broker = "broker.emqx.io"
topic = "audio/chunk1"

while True:
    fake_audio = [random.randint(0,1023) for _ in range(256)]
    payload = b''.join(struct.pack("<H", x) for x in fake_audio)

    publish.single(topic, payload, hostname=broker)
    print("sent fake audio")
    time.sleep(1)
