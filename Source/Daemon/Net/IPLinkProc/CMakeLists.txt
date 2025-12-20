add_executable(waechter-iplink
        IPLinkProc.cpp
)

target_link_libraries(waechter-iplink
        PRIVATE
        waechter::util
        thirdparty::spdlog
)