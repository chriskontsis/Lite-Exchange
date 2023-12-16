import sys
from PyQt5.QtWidgets import QApplication
import socket
from OrderBookWidget import OrderBookVisualizer

def receive_updated_price_levels(s,window):
    while True:
        try:
            data = s.recv(1024).decode()  
            if not data:
                break
            window.update_order_book(data)
        except ConnectionError as e:
            print("Connection closed:", e)
            break

def main():
    app = QApplication(sys.argv)
    window = OrderBookVisualizer()
    window.show()
    
    host = '127.0.0.1'
    port = 8080  

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((host, port))
        receive_updated_price_levels(s, window)
    except ConnectionRefusedError as e:
        print("Connection failed:", e)

    sys.exit(app.exec_())

if __name__ == "__main__":
    main()