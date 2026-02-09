cmake_minimum_required(VERSION 4.1)

# ==========================================================
# vcpkg triplet（必须在 project() 之前）
#
# 说明：
# - vcpkg toolchain 在 project() / enable_language()
#   阶段初始化
# - triplet 必须在那之前就确定，否则无效
# - 这里只提供“默认值”，绝不 FORCE
# ==========================================================
if (WIN32 AND NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING
            "Default vcpkg triplet for Windows static build")
endif ()

# ==========================================================
# 项目定义
# ==========================================================
project(Steam_showcase-Gen
        VERSION 1.1.1
        LANGUAGES C CXX
)

# ==========================================================
# 平台相关配置
# ==========================================================
if (WIN32)
    if (MSVC)
        # MSVC 静态运行时（MT / MTd）
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

        # 编译选项
        add_compile_options(
                /utf-8
                /W4
                /permissive-
        )
    endif ()

    # Windows 资源文件
    set(RC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.rc")
else ()
    set(RC_FILE "")
endif ()

# ==========================================================
# 语言标准
# ==========================================================
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 23)

# ==========================================================
# 第三方库
#
# 说明：
# - 最低版本由 vcpkg.json 统一约束
# - CMake 这里只负责“使用”
# ==========================================================
find_package(OpenCV REQUIRED)
find_package(ftxui REQUIRED)
find_package(FFMPEG REQUIRED)

# ==========================================================
# 源文件
# ==========================================================
file(GLOB_RECURSE SRC_FILES
        CONFIGURE_DEPENDS
        "src/*.cpp"
)

# ==========================================================
# 目标
# ==========================================================
add_executable(Steam_showcase-Gen
        ${SRC_FILES}
        ${RC_FILE}
)

set_target_properties(Steam_showcase-Gen PROPERTIES
        OUTPUT_NAME "SS-G"
        DEBUG_POSTFIX "_debug"
)

# ==========================================================
# 构建信息宏
# ==========================================================
string(TIMESTAMP APP_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP APP_BUILD_YEAR "%Y")

target_compile_definitions(Steam_showcase-Gen PRIVATE
        APP_VERSION="${PROJECT_VERSION}"
        APP_REPO_URL="github.com/flowersauce/Steam_showcase-Gen"
        APP_AUTHOR="Flowersauce"
        APP_LICENSE="MIT License"
        APP_BUILD_DATE="${APP_BUILD_DATE}"
        APP_BUILD_YEAR="${APP_BUILD_YEAR}"
)

# ==========================================================
# 语言选择
# ==========================================================
target_compile_definitions(Steam_showcase-Gen PRIVATE
        LANG_ZH_CN
        # LANG_EN_US
)

# ==========================================================
# 头文件路径
# ==========================================================
target_include_directories(Steam_showcase-Gen PRIVATE
        "include"
        ${FFMPEG_INCLUDE_DIRS}
)

# ==========================================================
# 链接库
# ==========================================================
target_link_libraries(Steam_showcase-Gen PRIVATE
        ${OpenCV_LIBS}
        ftxui::screen
        ftxui::dom
        ftxui::component
        ${FFMPEG_LIBRARIES}
)
