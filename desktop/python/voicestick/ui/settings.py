"""设置对话框"""
from PyQt5.QtWidgets import (
    QDialog, QVBoxLayout, QFormLayout, QLineEdit,
    QCheckBox, QSpinBox, QComboBox, QPushButton,
    QHBoxLayout, QLabel, QGroupBox,
)
from PyQt5.QtCore import Qt

from ..config import AppConfig


class SettingsDialog(QDialog):
    def __init__(self, config: AppConfig, parent=None):
        super().__init__(parent)
        self._config = config
        self._changed = False
        self.setWindowTitle("VoiceStick 设置")
        self.setFixedSize(450, 380)
        self._setup_ui()

    def _setup_ui(self):
        layout = QVBoxLayout(self)

        # BLE 设置
        ble_group = QGroupBox("蓝牙")
        ble_layout = QFormLayout(ble_group)
        self._paired_ids = QLineEdit(", ".join(self._config.paired_device_ids))
        self._paired_ids.setPlaceholderText("例如: E3F6, A1B2")
        ble_layout.addRow("已配对设备 ID：", self._paired_ids)
        layout.addWidget(ble_group)

        # ASR 设置
        asr_group = QGroupBox("语音识别")
        asr_layout = QFormLayout(asr_group)
        self._asr_url = QLineEdit(self._config.asr_server_url)
        self._asr_url.setPlaceholderText("火山引擎: wss://openspeech.bytedance.com/api/v3/sauc/bigmodel")
        asr_layout.addRow("ASR 服务器：", self._asr_url)
        self._asr_key = QLineEdit(self._config.asr_api_key)
        self._asr_key.setPlaceholderText("火山引擎控制台的 APP Key")
        self._asr_key.setEchoMode(QLineEdit.Password)
        asr_layout.addRow("API Key：", self._asr_key)
        layout.addWidget(asr_group)

        # 输入设置
        input_group = QGroupBox("输入")
        input_layout = QFormLayout(input_group)
        self._paste_check = QCheckBox("识别完成后自动粘贴")
        self._paste_check.setChecked(self._config.paste_on_final)
        input_layout.addRow(self._paste_check)
        self._enter_check = QCheckBox("粘贴后按 Enter")
        self._enter_check.setChecked(self._config.press_enter_after_paste)
        input_layout.addRow(self._enter_check)
        layout.addWidget(input_group)

        # 显示设置
        disp_group = QGroupBox("显示")
        disp_layout = QFormLayout(disp_group)
        self._subtitle_check = QCheckBox("显示字幕")
        self._subtitle_check.setChecked(self._config.subtitle_enabled)
        disp_layout.addRow(self._subtitle_check)
        layout.addWidget(disp_group)

        # 按钮
        btn_layout = QHBoxLayout()
        save_btn = QPushButton("保存")
        save_btn.clicked.connect(self._save)
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(self.reject)
        btn_layout.addStretch()
        btn_layout.addWidget(save_btn)
        btn_layout.addWidget(cancel_btn)
        layout.addLayout(btn_layout)

    def _save(self):
        # 解析配对设备 ID
        ids_text = self._paired_ids.text().strip()
        if ids_text:
            self._config.paired_device_ids = [
                pid.strip() for pid in ids_text.replace("，", ",").split(",") if pid.strip()
            ]
        else:
            self._config.paired_device_ids = []
        self._config.asr_server_url = self._asr_url.text().strip()
        self._config.asr_api_key = self._asr_key.text().strip()
        self._config.paste_on_final = self._paste_check.isChecked()
        self._config.press_enter_after_paste = self._enter_check.isChecked()
        self._config.subtitle_enabled = self._subtitle_check.isChecked()
        self._config.save()
        self._changed = True
        self.accept()

    @property
    def changed(self) -> bool:
        return self._changed
