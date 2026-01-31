import socket
import time

server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

server_socket.bind(('127.0.0.1', 8888))

server_socket.listen(5)
print("TCP server listening on port 8888")

try:
    while True:
        time.sleep(0.1)
except KeyboardInterrupt:
    server_socket.close()
    print("Server closed")
    time.sleep(10000);
