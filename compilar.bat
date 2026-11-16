@echo off
cd /d "%~dp0"
echo Parando processo antigo...
taskkill /f /im LiveAvatar.exe 2>nul
timeout /t 1 /nobreak >nul

echo Compilando...
set PATH=C:\msys64\mingw64\bin;%PATH%
g++ -O2 -std=c++17 main.cpp -o LiveAvatar.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32
if %errorlevel% neq 0 (
    echo ERRO DE COMPILACAO!
    pause
    exit /b 1
)
echo Compilado com sucesso!
echo Iniciando LiveAvatar...
start "" "%~dp0LiveAvatar.exe"
echo Pronto! Verifique o arquivo liveavatar_log.txt para diagnostico.
