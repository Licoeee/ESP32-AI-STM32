import asyncio
import json
from dataclasses import dataclass
from typing import Optional

from websockets.server import serve


HOST = "0.0.0.0"
PORT = 8000
PATH = "/asr/ws"
FRAME_BYTES = 320 * 2


@dataclass
class SessionState:
    sample_rate: int = 16000
    bits: int = 16
    channels: int = 1
    bytes_received: int = 0
    packets_received: int = 0
    speaking: bool = False


async def handle_client(websocket):
    state = SessionState()
    print(f"client connected: {websocket.remote_address}")

    try:
        async for message in websocket:
            if isinstance(message, str):
                try:
                    payload = json.loads(message)
                except json.JSONDecodeError:
                    payload = {"type": "text", "raw": message}

                msg_type = payload.get("type")
                if msg_type == "start":
                    state.sample_rate = int(payload.get("sample_rate", state.sample_rate))
                    state.bits = int(payload.get("bits", state.bits))
                    state.channels = int(payload.get("channels", state.channels))
                    await websocket.send(json.dumps({
                        "type": "ready",
                        "message": "local asr service ready",
                        "sample_rate": state.sample_rate,
                        "bits": state.bits,
                        "channels": state.channels,
                    }, ensure_ascii=False))
                elif msg_type == "stop":
                    await websocket.send(json.dumps({
                        "type": "final",
                        "text": "本地服务收到停止指令",
                    }, ensure_ascii=False))
                else:
                    await websocket.send(json.dumps({
                        "type": "info",
                        "echo": payload,
                    }, ensure_ascii=False))
                continue

            if isinstance(message, (bytes, bytearray)):
                state.bytes_received += len(message)
                state.packets_received += 1

                if not state.speaking and len(message) >= FRAME_BYTES:
                    state.speaking = True
                    await websocket.send(json.dumps({
                        "type": "partial",
                        "text": "检测到语音输入，开始本地识别",
                    }, ensure_ascii=False))

                if state.packets_received % 8 == 0:
                    await websocket.send(json.dumps({
                        "type": "partial",
                        "text": f"已接收 {state.bytes_received} 字节音频",
                    }, ensure_ascii=False))

                if state.packets_received % 20 == 0:
                    await websocket.send(json.dumps({
                        "type": "final",
                        "text": f"本地模拟识别结果：已收到 {state.bytes_received} 字节 PCM 数据",
                    }, ensure_ascii=False))
                    state.speaking = False

    except Exception as exc:
        print(f"client error: {exc}")
    finally:
        print(f"client disconnected: {websocket.remote_address}")


async def main():
    async def process_request(path: str, headers):
        if path != PATH:
            return 404, [], b"not found"
        return None

    print(f"local ASR websocket server on ws://{HOST}:{PORT}{PATH}")
    async with serve(handle_client, HOST, PORT, process_request=process_request):
        await asyncio.Future()


if __name__ == "__main__":
    asyncio.run(main())
