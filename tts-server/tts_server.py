"""LAN streaming speech endpoint for muse-desk-216. Never logs reply text."""
import asyncio
import hmac
import logging
import os

import edge_tts
from fastapi import FastAPI, HTTPException, Request
from starlette.responses import StreamingResponse

app = FastAPI(title="muse-desk-216 speech", docs_url=None, redoc_url=None)
VOICE = os.getenv("MUSE_TTS_VOICE", "zh-CN-XiaoxiaoNeural")
PROXY = os.getenv("MUSE_TTS_PROXY", "") or None
# Bearer token the device must present on /tts. /health stays open so LAN
# monitors can poll without a secret. A missing token fails closed (503):
# serving unauthenticated speech synthesis on the LAN repeats the upstream
# box3 mistake this file was copied from.
TTS_TOKEN = os.getenv("MUSE_TTS_TOKEN", "")
MAX_BYTES = 4095
slots = asyncio.Semaphore(2)
log = logging.getLogger("muse_tts")


def check_bearer(request: Request) -> None:
    if not TTS_TOKEN:
        raise HTTPException(503, "TTS token not configured")
    scheme, _, presented = request.headers.get("authorization", "").partition(" ")
    if scheme.lower() != "bearer" or not presented:
        raise HTTPException(401, "Missing bearer token")
    if not hmac.compare_digest(presented, TTS_TOKEN):
        raise HTTPException(401, "Invalid bearer token")


class SpeechResponse(StreamingResponse):
    async def __call__(self, scope, receive, send):
        try:
            await super().__call__(scope, receive, send)
        finally:
            slots.release()  # also runs if the client disconnects before the first chunk


async def speech_chunks(text):
    stream = edge_tts.Communicate(text, VOICE, proxy=PROXY).stream()
    try:
        while True:
            try:
                event = await asyncio.wait_for(anext(stream), timeout=20)
            except StopAsyncIteration:
                return
            if event.get("type") == "audio" and event.get("data"):
                yield event["data"]
    except Exception as exc:
        log.warning("Speech upstream failed: %s", type(exc).__name__)
        raise  # abort chunked HTTP, so the device can detect failure and show captions
    finally:
        await stream.aclose()


@app.get("/health")
async def health():
    return {"status": "ok", "voice": VOICE, "format": "audio-24khz-48kbitrate-mono-mp3"}


@app.post("/tts")
async def tts(request: Request):
    check_bearer(request)
    if request.headers.get("content-type", "").split(";", 1)[0].strip().lower() != "text/plain":
        raise HTTPException(415, "Use UTF-8 text/plain")
    body = bytearray()
    async for chunk in request.stream():
        if len(body) + len(chunk) > MAX_BYTES:
            raise HTTPException(413, "Reply exceeds the device text limit")
        body.extend(chunk)
    try:
        text = body.decode("utf-8").strip()
    except UnicodeDecodeError:
        raise HTTPException(400, "Invalid UTF-8") from None
    if not text:
        raise HTTPException(400, "Empty text")
    try:
        await asyncio.wait_for(slots.acquire(), timeout=0.2)
    except TimeoutError:
        raise HTTPException(503, "Speech server busy") from None
    try:
        return SpeechResponse(speech_chunks(text), media_type="audio/mpeg",
                              headers={"Cache-Control": "no-store"})
    except BaseException:
        slots.release()
        raise
