from PyQt5.QtWidgets import QApplication, QWidget, QHBoxLayout, QTableWidget, QTableWidgetItem, QListWidgetItem, QListWidget
from PyQt5.QtCore import QThread
from SocketThread import SocketThread
from collections import OrderedDict
from PyQt5.QtGui import QColor

class OrderBookVisualizer(QWidget):
    def __init__(self):
        super().__init__()
        self.order_book_data = {}  # Initialize order book data
        self.initUI()
        self.start_socket_thread()

    def initUI(self):
        layout = QHBoxLayout()

        self.equity_list = QListWidget()
        self.equity_list.itemClicked.connect(self.on_equity_clicked)
        self.equity_list.setStyleSheet("background-color: white; color: black;") 

        self.order_table = QTableWidget()
        self.order_table.setColumnCount(3)
        self.order_table.setHorizontalHeaderLabels(["Bid Volume", "Price", "Ask Volume"])
        self.order_table.setStyleSheet(
            "background-color: white; color: black; border: 1px solid black"
        )  

        header = self.order_table.horizontalHeader()
        header.setStyleSheet("background-color: navy; color: white; border: 2px solid black;") 

        layout.addWidget(self.equity_list)
        layout.addWidget(self.order_table)

        self.setLayout(layout)
        self.setWindowTitle('Order Book')

    def on_equity_clicked(self, item):
        ticker = item.text()
        print(self.order_book_data[ticker])

        if ticker in self.order_book_data:
            self.order_table.clearContents()

            data = self.order_book_data[ticker]
            buy_data = data['buy']
            sell_data = data['sell']

            buys = OrderedDict(sorted(buy_data.items(), key=lambda t: t[0]))
            sells = OrderedDict(sorted(sell_data.items(), key=lambda t: t[0], reverse=True))

            rows = len(buy_data) + len(sell_data)
            self.order_table.setRowCount(rows)

            column_backgrounds = [
                QColor("#DFF2FF"),  
                QColor("#FFFFFF"),  
                QColor("#F9CCCC")   
            ]

            row = 0
            for _, (price, sell_volume) in enumerate(sells.items()):
                if not sell_volume:
                    continue
                for col, cell_value in enumerate([buy_data.get(price, ''), price, sell_volume]):
                    item = QTableWidgetItem(str(cell_value))
                    item.setBackground(column_backgrounds[col])
                    self.order_table.setItem(row, col, item)
                row += 1

            for (price, buy_volume) in buys.items():
                if not buy_volume:
                    continue
                for col, cell_value in enumerate([buy_volume, price, sell_data.get(price, '')]):
                    item = QTableWidgetItem(str(cell_value))
                    item.setBackground(column_backgrounds[col])
                    self.order_table.setItem(row, col, item)
                row += 1


    def update_equity_list(self):
        self.equity_list.clear()
        for ticker in self.order_book_data.keys():
            item = QListWidgetItem(ticker)
            self.equity_list.addItem(item)

    def update_order_book(self, new_data):
        for line in new_data.split('#'):
            if line:
                parts = line.split(',')
                if len(parts) == 4:
                    action, ticker, price, quantity = parts
                    print(parts)
                    if not quantity:
                        break
                    if ticker not in self.order_book_data:
                        self.order_book_data[ticker] = {'buy': {}, 'sell': {}}
                    if action.strip() == 'BUY':
                        self.order_book_data[ticker]['buy'][float(price)] = int(quantity)
                    elif action.strip() == 'SELL':
                        self.order_book_data[ticker]['sell'][float(price)] = int(quantity)

            self.update_equity_list()

    def start_socket_thread(self):
        self.socket_thread = SocketThread('127.0.0.1', 8080)
        self.socket_thread.data_received.connect(self.update_order_book)
        self.socket_qthread = QThread()
        self.socket_thread.moveToThread(self.socket_qthread)
        self.socket_qthread.started.connect(self.socket_thread.run)
        self.socket_qthread.start()

def main():
    app = QApplication([])
    window = OrderBookVisualizer()
    window.show()
    app.exec_()

if __name__ == "__main__":
    main()
