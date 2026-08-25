import io
import os
import sys

# 언리얼이 CreateProc(bLaunchDetached=true)로 이 exe를 띄우면 Windows가 DETACHED_PROCESS로
# 실행해서 콘솔을 아예 안 만들어준다. 그러면 sys.stdout/stderr가 None이 되는데, import 시점에
# 여러 라이브러리가 warnings.warn()으로 stderr에 쓰려다 None.write()로 죽어버린다. 터미널에서
# 직접 실행할 땐 콘솔이 이미 있어서 이 문제가 재현되지 않는다.
if sys.stdout is None:
    sys.stdout = open(os.devnull, "w")
if sys.stderr is None:
    sys.stderr = open(os.devnull, "w")

# PyInstaller로 얼린 exe(USB 배포용)일 때만 모델 캐시를 exe 옆 로컬 폴더로 고정한다. .venv로
# 직접 돌릴 때(에디터 개발 워크플로)는 기본 HF 캐시(~/.cache/huggingface)를 그대로 써서, 이미
# 받아둔 모델을 다시 다운로드하지 않게 한다.
if getattr(sys, "frozen", False):
    _base_dir = os.path.dirname(sys.executable)
    os.environ.setdefault("HF_HOME", os.path.join(_base_dir, "hf_cache"))

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
