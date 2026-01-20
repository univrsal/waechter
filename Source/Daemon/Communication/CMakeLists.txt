target_sources(waechterd
        PRIVATE
        DaemonSocket.hpp
        DaemonSocket.cpp
        DaemonClient.hpp
        DaemonClient.cpp
        IServerSocket.hpp
        IClientSocket.hpp
        DaemonUnixSocket.cpp
        DaemonUnixSocket.hpp
        ClientUnixSocket.hpp
)

if (WAECHTER_WITH_WEBSOCKETSERVER)
    target_sources(waechterd
            PRIVATE
            DaemonWebSocket.cpp
            DaemonWebSocket.hpp
            ClientWebSocket.cpp
            ClientWebSocket.hpp
    )
endif ()