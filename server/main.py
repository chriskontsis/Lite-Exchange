import sys
from PyQt5.QtWidgets import QApplication
from OrderBookWidget import OrderBookVisualizer


def main():
    app = QApplication(sys.argv)
    window = OrderBookVisualizer()
    window.setStyleSheet("background-color: white;")
    window.setGeometry(500, 400, 750, 750)  # Set window size
    window.setWindowTitle('White Background')
    window.show()
    sys.exit(app.exec_())

if __name__ == "__main__":
    main()

