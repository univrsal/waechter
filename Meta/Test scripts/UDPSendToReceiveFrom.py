import socket
import time

RECV_BUFFER = 4096

# Create and bind the receiver socket so it actually receives datagrams sent to port 55511
receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
receiver.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
receiver.bind(("127.0.0.1", 55511))
receiver.settimeout(3.0)

# Sender will send to the bound receiver
dest = ("127.0.0.1", 55511)
sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
time.sleep(10)

sent = sender.sendto("Test".encode("utf-8"), dest)
print(f"Sent {sent} bytes to {dest}")

try:
    data, sender_addr = receiver.recvfrom(RECV_BUFFER)
    print(f"Received data: {data!r} from {sender_addr}")
    time.sleep(1)

    print(f"Received data: {data!r} from {sender_addr}")
except socket.timeout:
    print("Receiver timed out waiting for data")
finally:
    print("Sockets closed, exiting")

time.sleep(100000)
sender.close()
receiver.close()
