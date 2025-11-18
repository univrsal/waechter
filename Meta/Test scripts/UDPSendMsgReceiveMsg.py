import socket
import time

receiver = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
receiver.bind(("127.0.0.1", 1234))

time.sleep(5)

msg = "Hello, Server!"
sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
time.sleep(15)
bytes_sent = sender.sendmsg([msg.encode('utf-8')], [], 0, ("127.0.0.1", 1234))
print(f"Sent {bytes_sent} bytes")

data, ancdata, msg_flags, address = receiver.recvmsg(4096)
print(f"From {address} {data.decode('utf-8')}, {ancdata}, {msg_flags}")
time.sleep(150000)

receiver.close()
sender.close()
