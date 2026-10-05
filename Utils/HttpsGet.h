#pragma once

#include "UtilsTypes.h"
#include "Error.h"

// 同步 HTTP/HTTPS GET，跟随重定向。httpCode 为最终状态码。
int httpGetUrl(cstr_t url, cstr_t userAgent, int &httpCode, string &body, cstr_t referer = nullptr);
