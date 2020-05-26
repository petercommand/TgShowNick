cmake_minimum_required(VERSION 3.10)

add_subdirectory(td)
include_directories(td)

project(TgShowNick)

add_executable(TgShowNick main.cxx)
target_link_libraries(TgShowNick PRIVATE Td::TdStatic)

set_property(TARGET TgShowNick PROPERTY CXX_STANDARD 17)
