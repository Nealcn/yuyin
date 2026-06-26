"""字幕窗口 — 显示实时识别文字"""
from PyQt5.QtWidgets import QWidget, QLabel
from PyQt5.QtCore import Qt, QTimer
from PyQt5.QtGui import QFont


class SubtitleWindow(QWidget):
    """透明字幕窗口 — 跟随输入焦点显示识别结果"""

    def __init__(self):
        super().__init__(None)
        self._setup_ui()
        self._hide_timer = QTimer(self)
        self._hide_timer.timeout.connect(self.hide)
        self._hide_timer.setSingleShot(True)

    def _setup_ui(self):
        self.setWindowFlags(
            Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool
        )
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setFixedSize(600, 80)

        self._label = QLabel("", self)
        self._label.setAlignment(Qt.AlignCenter)
        self._label.setWordWrap(True)
        self._label.setStyleSheet("""
            QLabel {
                color: #ffffff;
                font-size: 20px;
                background: rgba(0, 0, 0, 160);
                border-radius: 10px;
                padding: 12px 20px;
            }
        """)
        self._label.setGeometry(0, 0, 600, 80)

        # 底部居中
        screen = self.screen().availableGeometry()
        self.move(
            (screen.width() - 600) // 2,
            screen.height() - 160,
        )

    def show_text(self, text: str):
        if not text.strip():
            self.hide()
            return
        self._label.setText(text)
        self.show()
        self._hide_timer.start(5000)  # 5 秒后自动隐藏

    def set_partial(self, text: str):
        """显示实时中间结果"""
        self._label.setText(text)
        self.show()
        self._hide_timer.stop()  # 持续显示不隐藏

    def set_final(self, text: str):
        """显示最终结果"""
        self._label.setText(text)
        self.show()
        self._hide_timer.start(8000)  # 8 秒后自动隐藏
