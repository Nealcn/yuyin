"""录音悬浮窗"""
from PyQt5.QtWidgets import QWidget, QLabel, QVBoxLayout
from PyQt5.QtCore import Qt, QTimer, pyqtSignal
from PyQt5.QtGui import QPainter, QColor, QPen, QFont


class OverlayWindow(QWidget):
    """半透明悬浮窗 — 显示录音/识别状态"""

    def __init__(self):
        super().__init__(None)
        self._setup_ui()
        self._status_text = "就绪"
        self._blink = False
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(500)

    def _setup_ui(self):
        self.setWindowFlags(
            Qt.WindowStaysOnTopHint | Qt.FramelessWindowHint | Qt.Tool
        )
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.setFixedSize(220, 50)

        self._label = QLabel("就绪", self)
        self._label.setAlignment(Qt.AlignCenter)
        self._label.setStyleSheet("""
            QLabel {
                color: #ffffff;
                font-size: 16px;
                font-weight: bold;
                background: rgba(0, 0, 0, 180);
                border-radius: 8px;
                padding: 6px 16px;
            }
        """)
        self._label.setGeometry(0, 0, 220, 50)

        # 放在屏幕右下角
        screen = self.screen().availableGeometry()
        self.move(screen.width() - 240, screen.height() - 100)

    def set_status(self, text: str):
        self._status_text = text
        self._label.setText(text)

        if "录音" in text:
            self._label.setStyleSheet("""
                QLabel {
                    color: #ff4444;
                    font-size: 16px;
                    font-weight: bold;
                    background: rgba(0, 0, 0, 200);
                    border-radius: 8px;
                    padding: 6px 16px;
                    border: 2px solid #ff4444;
                }
            """)
        elif "识别" in text or "粘贴" in text:
            self._label.setStyleSheet("""
                QLabel {
                    color: #ffaa00;
                    font-size: 16px;
                    font-weight: bold;
                    background: rgba(0, 0, 0, 200);
                    border-radius: 8px;
                    padding: 6px 16px;
                    border: 2px solid #ffaa00;
                }
            """)
        else:
            self._label.setStyleSheet("""
                QLabel {
                    color: #ffffff;
                    font-size: 16px;
                    font-weight: bold;
                    background: rgba(0, 0, 0, 180);
                    border-radius: 8px;
                    padding: 6px 16px;
                }
            """)
        self.show()

    def _tick(self):
        if "录音" in self._status_text:
            self._blink = not self._blink
            border = "#ff4444" if self._blink else "#880000"
            self._label.setStyleSheet(f"""
                QLabel {{
                    color: #ff4444;
                    font-size: 16px;
                    font-weight: bold;
                    background: rgba(0, 0, 0, 200);
                    border-radius: 8px;
                    padding: 6px 16px;
                    border: 2px solid {border};
                }}
            """)

    def hide_overlay(self):
        self.hide()
