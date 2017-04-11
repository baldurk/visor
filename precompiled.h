#pragma once

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR 1
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#endif

typedef unsigned char byte;

#include <assert.h>
#include <stdint.h>
#include <algorithm>
#include <vector>
#include "3rdparty/microprofile.h"
#include "3rdparty/vk_icd.h"
#include "3rdparty/vulkan.h"
#include "icd_common.h"
