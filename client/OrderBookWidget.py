from PyQt5.QtWidgets import QWidget, QHBoxLayout, QListWidget, QListWidgetItem
from collections import OrderedDict

class OrderBookVisualizer(QWidget):
    def __init__(self):
        super().__init__()
        self.order_book_data = {} 
        self.initUI()

    def initUI(self):
        layout = QHBoxLayout()

        # Create QListWidget for equities
        self.equity_list = QListWidget()
        self.equity_list.itemClicked.connect(self.on_equity_clicked)

        layout.addWidget(self.equity_list)

        # Create QListWidgets for buy and sell sides
        self.buy_list = QListWidget()
        self.sell_list = QListWidget()

        layout.addWidget(self.buy_list)
        layout.addWidget(self.sell_list)

        self.setLayout(layout)
        self.setWindowTitle('Order Book')
    
    def on_equity_clicked(self, item):
        ticker = item.text()
        buy_data = self.order_book_data[ticker]['buy']
        sell_data = self.order_book_data[ticker]['sell']

        # Clear previous data from buy and sell lists
        self.buy_list.clear()
        self.sell_list.clear()

        # Populate buy list
        sorted_buy_data = OrderedDict(sorted(buy_data.items(), key=lambda x: x[0], reverse=True))
        for price, quantity in sorted_buy_data.items():
            self.buy_list.addItem(f"Buy - Price: {price}, Quantity: {quantity}")

        # Populate sell list
        for price, quantity in sell_data.items():
            self.sell_list.addItem(f"Sell - Price: {price}, Quantity: {quantity}")

    def update_order_book(self, new_data):
        for line in new_data.split('#'):
            if line:
                parts = line.split(',')
                if len(parts) == 4:
                    action, ticker, price, quantity = parts
                    if ticker not in self.order_book_data:
                        self.order_book_data[ticker] = {'buy': {}, 'sell': {}}
                        item = QListWidgetItem(ticker)
                        self.equity_list.addItem(item)
                    if action.strip() == 'BUY':
                        self.order_book_data[ticker]['buy'][float(price)] = int(quantity)
                    elif action.strip() == 'SELL':
                        self.order_book_data[ticker]['sell'][float(price)] = int(quantity)

                    # Update the equity item text if it already exists
                    for index in range(self.equity_list.count()):
                        item = self.equity_list.item(index)
                        if item.text() == ticker:
                            item.setText(ticker)