//
// Created by usr on 27/10/2025.
//

#pragma once
#include <json11.hpp>

using WJson = json11::Json;
using WJsonValue = json11::JsonValue;
using EJsonParse = json11::JsonParse;

// Json consts
#define wJC static constexpr char const*

wJC JSON_KEY_BINARY_PATH = "bpath";
wJC JSON_KEY_BINARY_NAME = "bname";
wJC JSON_KEY_UPLOAD = "ul";
wJC JSON_KEY_DOWNLOAD = "dl";
wJC JSON_KEY_PID = "pid";
wJC JSON_KEY_CMDLINE = "cmdline";
wJC JSON_KEY_SOCKET_COOKIE = "sc";
wJC JSON_KEY_SOCKETS = "socks";
wJC JSON_KEY_PROCESSES = "procs";
wJC JSON_KEY_APPS = "apps";