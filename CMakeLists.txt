cmake_minimum_required(VERSION 3.14)
project(ImageOrganizer VERSION 1.0 LANGUAGES CXX)

add_executable(ImageOrganizer main.cpp)
find_package(exiv2 CONFIG REQUIRED) # 找
target_link_libraries(ImageOrganizer PRIVATE Exiv2::exiv2lib)  # 用
set_target_properties(ImageOrganizer PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED YES
)

# 🗂️ 所以最基本要素是：
# 需要什麼	      作用
# .cpp + .h	    你的原始碼
# 編譯器 (clang++, g++)	轉成物件檔
# 連結器	把所有物件檔連起來，接到外部函式庫
# 外部依賴	有時要 find_package / -l 指定
# 執行路徑	給 OS 執行時找到動態函式庫
# CMake 步驟	實際做了什麼
# project()	設定專案名字 & 語言
# add_executable()	告訴它哪些檔案要編譯成執行檔
# find_package()	找外部函式庫 & 設定 -I -L -l
# target_include_directories()	設定編譯器的 include path
# target_link_libraries()	告訴連結器要連結什麼函式庫
# set_target_properties()	指定 C++11/17/20 標準版本
