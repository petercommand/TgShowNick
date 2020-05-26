cmake_minimum_required(VERSION 3.10)

add_subdirectory(td)
include_directories(td)

project(TgUpdateNick)

add_executable(TgUpdateNick main.cxx)
target_link_libraries(TgUpdateNick PRIVATE Td::TdStatic)

set_property(TARGET TgUpdateNick PROPERTY CXX_STANDARD 17)
