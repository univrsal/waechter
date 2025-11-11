import socket
import time

# Create server TCP socket
server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind(('127.0.0.1', 8888))

# Listen for incoming connections in the background
server_socket.listen(5)
print("TCP server listening on port 8888")

connceting_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
connceting_socket.connect(('127.0.0.1', 8888))

client_socket, addr = server_socket.accept()
print(f"Accepted connection from {addr}")

time.sleep(10)
# close the connecting socket to trigger accept on server side
client_socket.close()
connceting_socket.close()

time.sleep(10)

server_socket.close()

time.sleep(10)
