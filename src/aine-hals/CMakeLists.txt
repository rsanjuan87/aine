# src/aine-hals/CMakeLists.txt — platform-aware HAL bridges

if(AINE_PLATFORM_MACOS)
    add_subdirectory(macos)
elseif(AINE_PLATFORM_LINUX)
    add_subdirectory(linux)
endif()

# Android API stub dylibs (replace /system/lib64/liblog.so etc. via aine-loader)
add_subdirectory(liblog)
add_subdirectory(libandroid)
