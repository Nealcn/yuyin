"""语音速记协调器 — 状态机"""
import asyncio
import logging
from typing import Optional

from .protocol import StateEvent, AudioFrame, ui_state_payload
from .ble import BleClient
from .asr_client import AsrClient
from .input_injector import paste_text

logger = logging.getLogger(__name__)


class Coordinator:
    """BLE + ASR + 输入注入协调器"""

    def __init__(self, ble: BleClient, asr_client: AsrClient):
        self._ble = ble
        self._asr = asr_client
        self._session_id: Optional[int] = None
        self._recording = False
        self._ogg_buffer: list[bytes] = []
        self._processing_audio = False  # 防止重复处理

        # UI 回调
        self.on_status: Optional[callable] = None
        self.on_partial_text: Optional[callable] = None
        self.on_final_text: Optional[callable] = None
        self.on_device_connected: Optional[callable] = None
        self.on_device_disconnected: Optional[callable] = None

        # 配置
        self.paste_on_final = True
        self.press_enter_after_paste = False
        self.asr_server_url = "ws://localhost:8080"

        # 注册 BLE 回调
        self._ble.on_audio_frame = self._on_audio_frame
        self._ble.on_state_event = self._on_state_event
        self._ble.on_connected = self._on_ble_connected
        self._ble.on_disconnected = self._on_ble_disconnected

        # 注册 ASR 回调
        self._asr.on_partial = self._on_asr_partial
        self._asr.on_final = lambda t: asyncio.create_task(self._on_asr_final(t))
        self._asr.on_error = self._on_asr_error

    async def start(self):
        """启动协调器"""
        self._set_status("就绪")
        # ASR 连接在后台进行，不阻塞
        asyncio.create_task(self._connect_asr())

    async def _connect_asr(self):
        """后台连接 ASR"""
        ok = await self._asr.start()
        if not ok:
            self._set_status("ASR 未连接")

    async def shutdown(self):
        await self._asr.stop()
        await self._ble.disconnect()

    def _set_status(self, status: str):
        logger.info("状态: %s", status)
        if self.on_status:
            self.on_status(status)

    # ---- BLE 回调 ----

    def _on_ble_connected(self):
        self._set_status("已连接")
        if self.on_device_connected:
            self.on_device_connected(self._ble.device_name)

    def _on_ble_disconnected(self):
        self._recording = False
        self._set_status("已断开")
        if self.on_device_disconnected:
            self.on_device_disconnected()

    def _on_state_event(self, event: StateEvent):
        logger.debug("状态事件: %s", event)
        if event.event == "button_down":
            self._handle_button_down(event)
        elif event.event == "button_up":
            self._handle_button_up(event)

    def _handle_button_down(self, event: StateEvent):
        if event.button == "primary":
            if not self._recording:
                self._recording = True
                self._session_id = event.session_id
                self._ogg_buffer = []
                self._set_status("录音中…")
                asyncio.create_task(self._start_asr_session())
            else:
                self._recording = False
                self._set_status("识别中…")

    async def _start_asr_session(self):
        """开始 ASR 会话"""
        ok = await self._asr.start_session()
        if not ok:
            self._set_status("ASR 会话失败")

    def _handle_button_up(self, event: StateEvent):
        # Use button_up as fallback if END frame didn't trigger processing
        if event.session_id is not None:
            self._session_id = event.session_id
        if self._ogg_buffer and not self._processing_audio:
            logger.info("button_up: 处理 %d 帧缓冲音频 (END帧可能丢失)", len(self._ogg_buffer))
            self._trigger_process_audio()

    def _trigger_process_audio(self):
        """触发 ASR 处理（防止重复调用）"""
        if self._processing_audio:
            return
        if not self._ogg_buffer:
            return
        self._processing_audio = True
        asyncio.create_task(self._process_audio())

    def _on_audio_frame(self, frame: AudioFrame):
        # Accept frame if session matches, or if no session set yet (use first frame's session)
        if self._session_id is not None and frame.session_id != self._session_id:
            return
        if self._session_id is None:
            self._session_id = frame.session_id
            logger.info("从音频帧设置 session_id=%d", self._session_id)

        if frame.is_end():
            # END 帧只是触发信号，不加入音频缓冲
            logger.info("收到结束帧，启动 ASR 处理: %d 帧缓冲", len(self._ogg_buffer))
            self._trigger_process_audio()
        else:
            self._ogg_buffer.append(frame.payload)

    async def _process_audio(self):
        """将缓冲的 Opus 数据发送到 ASR"""
        buffer = self._ogg_buffer
        self._ogg_buffer = []
        if not buffer:
            logger.warning("_process_audio: 无音频数据")
            self._processing_audio = False
            return
        logger.info("_process_audio: 发送 %d 帧到 ASR", len(buffer))
        self._set_status("识别中…")

        total = len(buffer)
        for i, chunk in enumerate(buffer):
            await self._asr.send_audio(chunk, is_last=(i == total - 1))
            await asyncio.sleep(0.01)

        await self._asr.stop_session()
        self._processing_audio = False

    # ---- ASR 回调 ----

    def _on_asr_partial(self, text: str):
        if self.on_partial_text:
            self.on_partial_text(text)

    async def _on_asr_final(self, text: str):
        """最终 ASR 结果（协程）"""
        logger.info("ASR 最终结果: %s", text)
        self._set_status("就绪")

        if self.on_final_text:
            self.on_final_text(text)

        # 自动粘贴
        if self.paste_on_final and text.strip():
            self._set_status("粘贴中…")
            ok = await asyncio.to_thread(paste_text, text, self.press_enter_after_paste)
            if ok:
                self._set_status("已粘贴")
            else:
                self._set_status("粘贴失败")
            await asyncio.sleep(1)
            self._set_status("就绪")

    def _on_asr_error(self, message: str):
        logger.error("ASR 错误: %s", message)
        self._set_status(f"错误: {message}")
