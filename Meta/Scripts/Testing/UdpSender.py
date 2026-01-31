import socket
import time

UDP_IP = "1.1.1.1"
UDP_PORT = 5005

MESSAGE = b"0" * 10  # 1 KB of data

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

sock.bind(("0.0.0.0", 5005))
sock.connect((UDP_IP, 5005))

for i in range(0, 5000):
    print(f"sent {sock.sendto(MESSAGE, (UDP_IP, UDP_PORT))}")
    time.sleep(0.7)  # 10 ms interval for ~1kb/s
sock.close()
print("done")
time.sleep(100)
