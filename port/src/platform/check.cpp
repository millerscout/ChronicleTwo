#include "platform/check.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "platform/breadcrumbs.hpp"
#include "platform/crash.hpp"
#include "platform/log.hpp"

void CheckFailed(const char *expression, const char *file, int line, const char *format, ...) {
    char    message[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof message, format, args);
    va_end(args);

    char reason[1024];
    std::snprintf(reason, sizeof reason, "CT_CHECK(%s) failed at %s:%d: %s", expression, file, line, message);
    CrashSetReason(reason);
    LogWrite(LogLevel::Error, "check", reason);
    for (int crumb = 0; crumb < static_cast<int>(Crumb::Count); ++crumb) {
        CT_LOG_ERROR("check", "  {}: {}", BreadcrumbName(static_cast<Crumb>(crumb)), BreadcrumbGet(static_cast<Crumb>(crumb)));
    }
    std::abort();
}
