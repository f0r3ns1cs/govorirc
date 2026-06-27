#pragma once

#include <cstdint>

// RFC 1459/2812: 512 bytes total including CRLF.
static constexpr uint32_t MAX_LINE_LEN = 8704; // 512 + 8191 (IRCv3 tags)
static constexpr uint32_t MAX_BODY_LEN = 510;

static constexpr uint32_t MAX_NICKLEN = 30;
static constexpr uint32_t MAX_USERLEN = 16;
static constexpr uint32_t MAX_REALNAMELEN = 200;

static constexpr uint32_t MAX_CHANNELLEN = 64;

static constexpr uint32_t MAX_RECVBUFLEN = 16384;
static constexpr uint32_t MAX_SENDBUFLEN = 1 << 20;  // 1 MiB per-client write buffer cap

static constexpr uint32_t MAX_CHANNELS_PER_USER = 20;
static constexpr uint32_t MAX_USERS_PER_CHANNEL = 1000;
static constexpr uint32_t MAX_BAN_LIST          = 50;

static constexpr uint16_t DEFAULT_PORT = 6667;
static constexpr uint16_t DEFAULT_SSL_PORT = 6697;

static constexpr uint32_t MAX_CONNECTIONS = 10000;

static constexpr uint32_t PING_INTERVAL_SEC = 60;
static constexpr uint32_t PING_TIMEOUT_SEC = 120;
static constexpr uint32_t CONNECT_TIMEOUT_SEC = 60;

static constexpr uint32_t MAX_HOSTLEN = 64;
static constexpr uint32_t MAX_IPLEN = 45; // IPv6 max length
static constexpr uint32_t MAX_SERVERNAMELEN = 64;

static constexpr const char* CRLF = "\r\n";

static constexpr const char* SERVER_VERSION    = "v1.0.260526";
static constexpr const char* SERVER_CREATED_AT = __DATE__ " " __TIME__ " UTC";

static constexpr const char* DEFAULT_SERVER_NAME  = "irc.f0r3ns1cs.rip";
static constexpr const char* DEFAULT_NETWORK      = "govornet";
static constexpr const char* DEFAULT_DESCRIPTION  = "govorIRC";

static constexpr char IRC_BOLD = '\x02';
static constexpr char IRC_COLOR = '\x03';
static constexpr char IRC_RESET = '\x0F';
static constexpr char IRC_ITALIC = '\x1D';
static constexpr char IRC_UNDERLINE = '\x1F';

static constexpr int RPL_MOTDSTART = 375;
static constexpr int RPL_MOTD = 372;
static constexpr int RPL_ENDOFMOTD = 376;
static constexpr int ERR_NOMOTD = 422;

static constexpr int ERR_NOORIGIN = 409;
