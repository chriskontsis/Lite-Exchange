import socket

def receive_order_book_data():
    host = '127.0.0.1'  # Change this to your server's IP address if needed
    port = 8080

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.connect((host, port))
        order_book_data = s.recv(1024).decode()  # Adjust buffer size as needed
        print("Received order book data:")
        print(order_book_data)  # Print or process the received order book data here

# Receive order book data from the C++ server
receive_order_book_data()
