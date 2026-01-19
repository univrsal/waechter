target_sources(waechter
        PRIVATE
        IDaemonSocket.hpp
        UnixSocketSource.cpp
        UnixSocketSource.hpp
)

if (WAECHTER_WITH_WEBSOCKETCLIENT)
    target_sources(waechter
            PRIVATE
            WebSocketSource.cpp
            WebSocketSource.hpp
    )
endif ()
