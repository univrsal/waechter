target_sources(waechter
        PRIVATE
        IDaemonSocket.hpp
)

if (UNIX OR APPLE AND NOT EMSCRIPTEN)
	target_sources(waechter
			PRIVATE
        	UnixSocketSource.cpp
        	UnixSocketSource.hpp
	)
endif()
if (WAECHTER_WITH_WEBSOCKETCLIENT)
    target_sources(waechter
            PRIVATE
            WebSocketSource.cpp
            WebSocketSource.hpp
    )
endif ()
