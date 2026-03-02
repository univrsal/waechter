target_sources(waechterd
        PRIVATE
        PacketParser.cpp
        PacketParser.hpp
        ../../Util/Data/SocketStateParser.cpp
        ../../Util/Data/SocketStateParser.hpp
        Resolver.cpp
        Resolver.hpp
        IPLink.cpp
        IPLink.hpp
)

add_subdirectory(IPLinkProc)