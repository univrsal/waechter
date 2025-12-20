target_sources(waechterd
        PRIVATE
        PacketParser.cpp
        PacketParser.hpp
        SocketStateParser.cpp
        SocketStateParser.hpp
        Resolver.cpp
        Resolver.hpp
        IPLink.cpp
        IPLink.hpp
)

add_subdirectory(IPLinkProc)