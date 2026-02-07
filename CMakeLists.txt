cmake_minimum_required(VERSION 4.1)
project(Steam_showcase-Gen)

set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "" FORCE)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_C_STANDARD 23)

if (MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8 /W4 /permissive-)
endif ()

find_package(FFMPEG REQUIRED)
find_package(OpenCV REQUIRED)
find_package(ftxui REQUIRED)

file(GLOB_RECURSE SRC_FILES "src/*.cpp")
set(RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.rc")

add_executable(Steam_showcase-Gen
        ${SRC_FILES}
        ${RC_FILE}
)

set_target_properties(Steam_showcase-Gen PROPERTIES
        OUTPUT_NAME "SS-G"
        DEBUG_POSTFIX "_debug"
)

# 版本与信息定义
set(APP_VERSION "1.0.0")
set(APP_REPO_URL "github.com/flowersauce/Steam_showcase-Gen")
set(APP_AUTHOR "Flowersauce")
set(APP_LICENSE "MIT License")
string(TIMESTAMP APP_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP APP_BUILD_YEAR "%Y")

# 语言选择与宏定义
target_compile_definitions(Steam_showcase-Gen PRIVATE
        LANG_ZH_CN
        # LANG_EN_US

        APP_VERSION=\"${APP_VERSION}\"
        APP_REPO_URL=\"${APP_REPO_URL}\"
        APP_BUILD_DATE=\"${APP_BUILD_DATE}\"
        APP_BUILD_YEAR=\"${APP_BUILD_YEAR}\"
        APP_AUTHOR=\"${APP_AUTHOR}\"
        APP_LICENSE=\"${APP_LICENSE}\"
)

# 链接头文件路径
target_include_directories(Steam_showcase-Gen PRIVATE
        "include"
        ${FFMPEG_INCLUDE_DIRS}
)

# 链接第三方库
target_link_libraries(Steam_showcase-Gen PRIVATE
        ${OpenCV_LIBS}
        ftxui::screen ftxui::dom ftxui::component
        ${FFMPEG_LIBRARIES}
)
