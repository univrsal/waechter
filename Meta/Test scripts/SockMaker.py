import socket
import sys
import threading
import time

HOST = '127.0.0.1'
PORT = 9999
clients = {}
server_conns = []
next_id = 1


def server_thread():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        server.bind((HOST, PORT))
    except OSError as e:
        print(f"Error binding server: {e}")
        return
    server.listen()
    # print(f"Server listening on {HOST}:{PORT}")
    try:
        while True:
            conn, addr = server.accept()
            server_conns.append(conn)
    except Exception as e:
        pass


def start_server():
    t = threading.Thread(target=server_thread, daemon=True)
    t.start()


def connect_client():
    global next_id
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect((HOST, PORT))
        cid = next_id
        next_id += 1
        clients[cid] = s
        print(f"Connected client {cid}")
    except Exception as e:
        print(f"Failed to connect: {e}")


def close_client(cid):
    if cid in clients:
        try:
            clients[cid].close()
            del clients[cid]
            print(f"Closed client {cid}")
        except Exception as e:
            print(f"Error closing client {cid}: {e}")
    else:
        print(f"Client {cid} not found")


def main():
    print(f"Starting server on {HOST}:{PORT}...")
    start_server()
    # Give server a moment to start
    time.sleep(0.1)
    print("Commands:")
    print("  connect      - Create a new client connection")
    print("  close <id>   - Close a specific client connection")
    print("  list         - List open client connections")
    print("  exit         - Exit the script")
    while True:
        try:
            sys.stdout.write("> ")
            sys.stdout.flush()
            line = sys.stdin.readline()
            if not line:
                break
            parts = line.strip().split()
            if not parts:
                continue
            cmd = parts[0].lower()
            if cmd == 'connect':
                connect_client()
            elif cmd == 'close':
                if len(parts) > 1:
                    try:
                        cid = int(parts[1])
                        close_client(cid)
                    except ValueError:
                        print("Invalid ID. Usage: close <id>")
                else:
                    print("Usage: close <id>")
            elif cmd == 'list':
                print(f"Open clients: {list(clients.keys())}")
            elif cmd == 'exit':
                break
            else:
                print("Unknown command")
        except KeyboardInterrupt:
            break
    # Cleanup
    for s in clients.values():
        try:
            s.close()
        except:
            pass
    for s in server_conns:
        try:
            s.close()
        except:
            pass


if __name__ == "__main__":
    main()
