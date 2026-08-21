import io
import soundfile as sf
from fastapi import FastAPI
from fastapi.responses import Response
from pydantic import BaseModel
from melo.api import TTS

app = FastAPI()

# 모델 로드 (서버 시작 시 1회)
model = TTS(language='KR', device='cuda:0')
speaker_ids = model.hps.data.spk2id

class TTSRequest(BaseModel):
    text: str
    language: str = "Korean"
    speaker: str = "Sohee"

@app.post("/tts")
async def tts(req: TTSRequest):
    audio = model.tts_to_file(
        req.text,
        speaker_ids['KR'],
        quiet=True,
        speed=1.0
    )
    wav_buf = io.BytesIO()
    sf.write(wav_buf, audio, model.hps.data.sampling_rate, format="WAV")
    return Response(content=wav_buf.getvalue(), media_type="audio/wav")

@app.get("/health")
async def health():
    return {"status": "ok", "voice": "MeloTTS-KR"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="127.0.0.1", port=8080)
