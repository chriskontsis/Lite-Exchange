import socket
from PyQt5.QtCore import pyqtSignal, QObject


class SocketThread(QObject):
    #this is a pyqt signal which emits a string type when data is recieved
    data_received = pyqtSignal(str)

    def __init__(self, host, port):
        super().__init__()
        self.host = host
        self.port = port

    def run(self):
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.bind((self.host, self.port))
        server_socket.listen(1)  # Listen for incoming connections

        print(f"Server listening on {self.host}:{self.port}")

        client_socket, addr = server_socket.accept()
        print(f"Connection established with {addr}")

        while True:
            data = client_socket.recv(1024).decode()
            if not data:
                break
            self.data_received.emit(data)
            print(f"Received data: {data}")

        client_socket.close()
        server_socket.close()

