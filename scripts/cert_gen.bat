@echo off
setlocal

set SCRIPT_DIR=%~dp0
set CERTS_DIR=%SCRIPT_DIR%..\certs

if not exist "%CERTS_DIR%" mkdir "%CERTS_DIR%"

where openssl >nul 2>&1
if %errorlevel% neq 0 (
    echo error: openssl not found on PATH
    echo Install OpenSSL or add it to your PATH before running this script.
    exit /b 1
)

echo Generating 4096-bit RSA key and self-signed certificate for localhost...

openssl req -x509 ^
    -newkey rsa:4096 ^
    -keyout "%CERTS_DIR%\key.pem" ^
    -out    "%CERTS_DIR%\cert.pem" ^
    -days   365 ^
    -nodes ^
    -subj   "/CN=localhost" ^
    -addext "subjectAltName=DNS:localhost,IP:127.0.0.1"

if %errorlevel% neq 0 (
    echo error: certificate generation failed.
    exit /b 1
)

echo.
echo cert.pem and key.pem written to %CERTS_DIR%
endlocal
