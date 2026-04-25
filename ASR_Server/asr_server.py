import asyncio
import json
import websockets

HOST = "0.0.0.0"
PORT = 8000
PATH = "/asr/ws"

async def handler(websocket):
    peer = websocket.remote_address
    print(f"client connected from {peer}", flush=True)
    pcm_bytes = 0
    frame_count = 0

    async for message in websocket:
        if isinstance(message, str):
            print(f"text from client: {message}", flush=True)
            continue

        pcm_bytes += len(message)
        frame_count += 1
        print(f"received pcm bytes={len(message)}, total={pcm_bytes}, frames={frame_count}", flush=True)

        if frame_count % 20 == 0:
            result = {
                "type": "result",
                "text": f"本地服务收到音频，累计 {pcm_bytes} 字节"
            }
            await websocket.send(json.dumps(result, ensure_ascii=False))
            print(f"sent result text at frame {frame_count}", flush=True)

async def main():
    async def ws_handler(websocket):
        request_path = getattr(websocket, "path", None)
        if request_path is None:
            request = getattr(websocket, "request", None)
            request_path = getattr(request, "path", None)
        print(f"incoming websocket path: {request_path}", flush=True)
        if request_path != PATH:
            print(f"invalid path: {request_path}", flush=True)
            await websocket.close()
            return
        await handler(websocket)

    async with websockets.serve(ws_handler, HOST, PORT):
        print(f"ASR server listening on ws://{HOST}:{PORT}{PATH}", flush=True)
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())
