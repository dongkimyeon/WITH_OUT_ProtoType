@echo off
setlocal

set SCRIPT_DIR=%~dp0
set VENV_DIR=%SCRIPT_DIR%.venv

echo [TTS_Bridge] Creating virtual environment: %VENV_DIR%
echo [TTS_Bridge] Package versions in requirements.txt were validated against Python 3.10.
echo [TTS_Bridge] Trying Python 3.10 via the py launcher first...

py -3.10 -m venv "%VENV_DIR%" 2>nul

if not exist "%VENV_DIR%\Scripts\python.exe" (
    echo [TTS_Bridge] Python 3.10 not available via py launcher. Falling back to default python.
    echo [TTS_Bridge] If pip install fails below, install Python 3.10 and re-run this script.
    python -m venv "%VENV_DIR%"
)

if not exist "%VENV_DIR%\Scripts\python.exe" (
    echo [TTS_Bridge] Failed to create virtual environment. Make sure Python is installed and on PATH.
    pause
    exit /b 1
)

call "%VENV_DIR%\Scripts\activate.bat"
python -m pip install --upgrade pip
pip install -r "%SCRIPT_DIR%requirements.txt"

if errorlevel 1 (
    echo.
    echo [TTS_Bridge] pip install FAILED - see the error above. The venv is incomplete.
    echo [TTS_Bridge] This is most likely a Python version mismatch - requirements.txt was
    echo [TTS_Bridge] validated against Python 3.10. Install Python 3.10 and re-run this script.
    pause
    exit /b 1
)

python -m unidic download

if errorlevel 1 (
    echo.
    echo [TTS_Bridge] unidic dictionary download FAILED - see the error above.
    echo [TTS_Bridge] MeCab will fail to initialize without it. Re-run this script.
    pause
    exit /b 1
)

echo.
echo [TTS_Bridge] Setup complete. You can now run start_tts_server.bat.
pause
