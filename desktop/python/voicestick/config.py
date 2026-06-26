"""VoiceStick 配置管理（JSON）"""
import json
from pathlib import Path
from dataclasses import dataclass, field, asdict


@dataclass
class AppConfig:
    paired_device_ids: list[str] = field(default_factory=list)
    asr_server_url: str = "wss://openspeech.bytedance.com/api/v3/sauc/bigmodel_async"
    asr_api_key: str = ""
    language: str = "zh-CN"
    paste_on_final: bool = True
    press_enter_after_paste: bool = False
    interaction_mode: str = "hold_to_talk"
    overlay_theme: str = "auto"
    subtitle_enabled: bool = True
    subtitle_position: str = "bottom"
    brightness: int = 50
    debug_audio: bool = False
    debug_audio_dir: str = ""

    CONFIG_PATH = Path.home() / ".voicestick" / "config.json"

    @classmethod
    def load(cls) -> "AppConfig":
        path = cls.CONFIG_PATH
        if not path.exists():
            return cls()
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
            return cls(**{k: v for k, v in data.items() if k in cls.__dataclass_fields__})
        except Exception:
            return cls()

    def save(self):
        path = self.CONFIG_PATH
        path.parent.mkdir(parents=True, exist_ok=True)
        data = {k: v for k, v in asdict(self).items() if k != "CONFIG_PATH"}
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
