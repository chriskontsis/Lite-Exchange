import sys
from PyQt5.QtWidgets import QApplication
from OrderBookWidget import OrderBookVisualizer


def main():
    app = QApplication(sys.argv)
    window = OrderBookVisualizer()
    window.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()

# import sys
# from PyQt5.QtWidgets import QApplication, QMainWindow, QTableWidget, QTableWidgetItem, QVBoxLayout, QWidget


# class SimpleTable(QWidget):
#     def __init__(self):
#         super().__init__()
#         self.initUI()

#     def initUI(self):
#         layout = QVBoxLayout()
#         self.tableWidget = QTableWidget()
#         self.tableWidget.setColumnCount(3)
#         self.tableWidget.setHorizontalHeaderLabels(["Column 1", "Column 2", "Column 3"])

#         layout.addWidget(self.tableWidget)
#         self.setLayout(layout)

#         # Add rows to the table widget
#         self.add_table_rows()

#     def add_table_rows(self):
#         # Sample data to add to the table
#         data = [
#             ("Data 1", "Value 1", "Entry 1"),
#             ("Data 2", "Value 2", "Entry 2"),
#             ("Data 3", "Value 3", "Entry 3"),
#             # Add more rows here as needed
#         ]

#         # Add rows to the table widget
#         self.tableWidget.setRowCount(len(data))
#         for row, rowData in enumerate(data):
#             for col, value in enumerate(rowData):
#                 item = QTableWidgetItem(str(value))
#                 self.tableWidget.setItem(row, col, item)


# def main():
#     app = QApplication(sys.argv)
#     window = QMainWindow()
#     window.setGeometry(100, 100, 400, 300)
#     table = SimpleTable()
#     window.setCentralWidget(table)
#     window.setWindowTitle('Simple Table Widget')
#     window.show()
#     sys.exit(app.exec_())


# if __name__ == '__main__':
#     main()

