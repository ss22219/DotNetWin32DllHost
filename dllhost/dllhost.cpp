// dllhost.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include <Windows.h>
#include <algorithm>
#include "pal.h"
#include "hostfxr_resolver.h"
#include "trace.h"
#include "utils.h"

uint8_t placeholder[] = {
    0x8b, 0x12, 0x02, 0xb9, 0x6a, 0x61, 0x20, 0x38,
        0x72, 0x7b, 0x93, 0x02, 0x14, 0xd7, 0xa0, 0x32,
        0x13, 0xf5, 0xb9, 0xe6, 0xef, 0xae, 0x33, 0x18,
        0xee, 0x3b, 0x2d, 0xce, 0x24, 0xb3, 0x6a, 0xae
};

int64_t get_header_offset(pal::string_t host_path)
{
    size_t length = 0;
    auto map = pal::mmap_read(host_path, &length);

    auto index = std::search((char*)map, ((char*)map + length), (char*)placeholder, (char*)placeholder + 32);
    return *(int64_t*)(index - 8);
}

void need_newer_framework_error()
{
    pal::string_t url = get_download_url();
    trace::error(_X("  _ To run this application, you need to install a newer version of .NET Core."));
    trace::error(_X(""));
    trace::error(_X("  - %s&apphost_version=%s"), url.c_str(), _STRINGIFY(COMMON_HOST_PKG_VER));
}

int main()
{
    int rc = 0;
#if _DEBUG
    trace::enable();
#else
    trace::setup();
#endif // DEBUG

    pal::string_t host_path = _X("C:\\Users\\Lenovo\\source\\repos\\ComInterop\\apphost\\bin\\Release\\net5.0\\publish\\apphost.exe");
    pal::string_t embedded_app_name = _X("apphost.dll");
    pal::string_t app_path;
    pal::string_t app_root;

    auto bundle_header_offset = get_header_offset(host_path);
    bool requires_hostfxr_startupinfo_interface = false;
    app_path.assign(get_directory(host_path));
    append_path(&app_path, embedded_app_name.c_str());
    app_root.assign(get_directory(app_path));
    hostfxr_resolver_t fxr{ app_root };

    auto hostfxr_main_bundle_startupinfo = fxr.resolve_main_bundle_startupinfo();
    if (hostfxr_main_bundle_startupinfo != nullptr)
    {
        const pal::char_t* host_path_cstr = host_path.c_str();
        const pal::char_t* dotnet_root_cstr = fxr.dotnet_root().empty() ? nullptr : fxr.dotnet_root().c_str();
        const pal::char_t* app_path_cstr = app_path.empty() ? nullptr : app_path.c_str();

        trace::info(_X("Invoking fx resolver [%s] hostfxr_main_bundle_startupinfo"), fxr.fxr_path().c_str());
        trace::info(_X("Host path: [%s]"), host_path.c_str());
        trace::info(_X("Dotnet path: [%s]"), fxr.dotnet_root().c_str());
        trace::info(_X("App path: [%s]"), app_path.c_str());
        trace::info(_X("Bundle Header Offset: [%lx]"), bundle_header_offset);

        auto set_error_writer = fxr.resolve_set_error_writer();
        const char_t* args[1]{ host_path.c_str()};
        rc = hostfxr_main_bundle_startupinfo(1, args, host_path_cstr, dotnet_root_cstr, app_path_cstr, bundle_header_offset);
    }
    else
    {
        // The host components will be statically linked with the app-host: https://github.com/dotnet/runtime/issues/32823
        // Once this work is completed, an outdated hostfxr can only be found for framework-related apps.
        trace::error(_X("The required library %s does not support single-file apps."), fxr.fxr_path().c_str());
        need_newer_framework_error();
        rc = StatusCode::FrameworkMissingFailure;
    }
    return rc;
}