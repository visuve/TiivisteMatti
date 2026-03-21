#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <SDKDDKVer.h>
#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <cassert>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <ranges>
#include <span>
#include <sstream>
#include <stop_token>
#include <string_view>
#include <thread>