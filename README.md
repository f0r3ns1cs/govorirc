# govorirc

IRC server and client written in C++23.

<div align="center">
  <a style="margin: 2px;">
    <img alt="C++23" src="https://img.shields.io/badge/language-C%2B%2B23-blue?logo=C%2B%2B">
  </a>
  <a href="https://github.com/f0r3ns1cs/govorirc/blob/master/LICENSE.txt" style="margin: 2px;">
    <img alt="LICENSE-MIT" src="https://img.shields.io/badge/LICENSE-MIT-green">
  </a>
</div>

---

## Overview

Implemented commands: `CAP`, `PASS`, `NICK`, `USER`, `JOIN`, `PART`, `PRIVMSG`, `NOTICE`, `KICK`, `INVITE`, `MODE`, `TOPIC`, `LIST`, `NAMES`, `WHO`, `WHOIS`, `WHOWAS`, `MOTD`, `QUIT`, `PING`/`PONG`.

---

## Building

Dependencies are managed via [vcpkg](https://github.com/microsoft/vcpkg). Set `VCPKG_ROOT` before configuring.

**Linux / macOS**
```sh
cmake --preset linux-x64   # or mac, mac-intel, mac-fat
cmake --build --preset linux-x64-release
```

**Windows (x64, from a VS developer prompt)**
```sh
cmake --preset win-x64
cmake --build --preset win-x64-release
```

Outputs go to `build/<preset>/bin/<config>/`.

Available presets: `linux-x64`, `mac` (arm64), `mac-intel`, `mac-fat` (universal), `win-x64`, `win-x86`.

---

## Certificates

A self-signed cert is required to run the server locally. Generate one with:

```sh
scripts\cert_gen.bat      # Windows
```

---

## Usage

**Server**
```
govord [port] [cert.pem] [key.pem] [server-name] [network]
```
```sh
govord 6697 certs/cert.pem certs/key.pem irc.example.com mynet
```
Defaults: port `6697`, `cert.pem`, `key.pem`, `irc.f0r3ns1cs.rip`, `govornet`.

**Client**
```
govorirc <host> <port> [options]
  -n, --nick <nick>      nickname
  -u, --user <user>      username
  -r, --real <realname>  real name
  -p, --pass <password>  server password
  -k, --insecure         skip TLS cert verification (self-signed certs)
  -y, --yes              use defaults, skip prompts
```
```sh
govorirc irc.example.com 6697 -n mynick
govorirc localhost 6697 -n mynick --insecure
```

---

## Requirements

- C++23 compiler (MSVC 19.38+, GCC 13+, Clang 17+)
- CMake 3.25+
- vcpkg
- OpenSSL

---

## License

MIT. See [LICENSE.MD](LICENSE.MD).
