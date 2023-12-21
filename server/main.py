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

