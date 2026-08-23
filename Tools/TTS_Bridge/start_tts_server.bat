@echo off
setlocal

set SCRIPT_DIR=%~dp0
set VENV_PYTHON=%SCRIPT_DIR%.venv\Scripts\python.exe

if not exist "%VENV_PYTHON%" (
    echo [TTS_Bridge] Virtual environment not found. Run setup_tts_env.bat first.
    pause
    exit /b 1
)

"%VENV_PYTHON%" "%SCRIPT_DIR%server.py"
pause
