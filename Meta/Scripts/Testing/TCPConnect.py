import socket
import time

# connect to 127.0.0.1:8888
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket.connect(('127.0.1', 8888))

try:
    while True:
        time.sleep(0.1)
except KeyboardInterrupt:
    client_socket.close()
    print("Client closed")
    time.sleep(10000)
