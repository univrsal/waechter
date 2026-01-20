add_executable(waechter-iplink
        IPLinkProc.cpp
)

target_link_libraries(waechter-iplink
        PRIVATE
        waechter::util
        thirdparty::spdlog
        thirdparty::cereal
        thirdparty::sigslot
        thirdparty::tracy
)

install(TARGETS waechter-iplink
        RUNTIME DESTINATION bin)
