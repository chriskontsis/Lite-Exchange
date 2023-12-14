import socket

def receive_updated_price_levels(s):
    while True:
        try:
            data = s.recv(1024).decode()  # Adjust buffer size as needed
            if not data:
                break
            # Process received data and update GUI
            print("Received updated price levels:", data)  # Replace with your GUI update logic
        except ConnectionError as e:
            print("Connection closed:", e)
            break

def main():
    host = '127.0.0.1'  # Replace with your server's IP address
    port = 8080  # Same port used in the C++ server

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        s.connect((host, port))
        receive_updated_price_levels(s)
    except ConnectionRefusedError as e:
        print("Connection failed:", e)

if __name__ == "__main__":
    main()



# import sys
# from PyQt5.QtWidgets import QApplication, QWidget, QVBoxLayout, QListWidget, QListWidgetItem, QHBoxLayout, QLabel

# class OrderBookGUI(QWidget):
#     def __init__(self, order_book_data):
#         super().__init__()
#         self.order_book_data = order_book_data
#         self.initUI()

#     def initUI(self):
#         layout = QHBoxLayout()

#         # Create QListWidget for equities
#         self.equity_list = QListWidget()
#         self.equity_list.itemClicked.connect(self.on_equity_clicked)

#         # Populate QListWidget with equities
#         for ticker in self.order_book_data.keys():
#             item = QListWidgetItem(ticker)
#             self.equity_list.addItem(item)

#         layout.addWidget(self.equity_list)

#         # Create QListWidgets for buy and sell sides
#         self.buy_list = QListWidget()
#         self.sell_list = QListWidget()

#         layout.addWidget(self.buy_list)
#         layout.addWidget(self.sell_list)

#         self.setLayout(layout)
#         self.setWindowTitle('Order Book')

#     def on_equity_clicked(self, item):
#         ticker = item.text()
#         buy_data = self.order_book_data[ticker]['buy']
#         sell_data = self.order_book_data[ticker]['sell']

#         # Clear previous data from buy and sell lists
#         self.buy_list.clear()
#         self.sell_list.clear()

#         # Populate buy list
#         for price, quantity in buy_data.items():
#             self.buy_list.addItem(f"Buy - Price: {price}, Quantity: {quantity}")

#         # Populate sell list
#         for price, quantity in sell_data.items():
#             self.sell_list.addItem(f"Sell - Price: {price}, Quantity: {quantity}")

# def display_order_book_gui(order_book_data):
#     app = QApplication(sys.argv)
#     window = OrderBookGUI(order_book_data)
#     window.show()
#     sys.exit(app.exec_())

# # Example order book data (similar structure to previous examples)
# order_book_data = {
#     'AAPL': {
#         'buy': {150.0: 100, 149.5: 50},
#         'sell': {155.0: 75, 156.0: 100}
#     },
#     'GOOGL': {
#         'buy': {2700.0: 200, 2680.0: 50},
#         'sell': {2750.0: 100, 2760.0: 150}
#     }
#     # Add more equities as needed
# }

# # Display the order book GUI
# display_order_book_gui(order_book_data)
