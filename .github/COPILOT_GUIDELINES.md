Formatting:
Code should follow the formatting of existing code as defined in the .clang-format file in the repository.
The style closely matches the Unreal Engine coding standards.

Here's an example class:

```cpp
class WFoo
{
    bool bIsLoaded{};
    void Unload();

public:
    void DoSomething(std::string const& StringValue, bool& bOutSuccess);
    void Load();
    
    virtual void Tick(float DeltaTime) {}
};
```

Project overview:

The project is split into five parts

- Source/Daemon/ - Contains the daemon that runs as a service in the background
    - Loads eBPF program (WaechterEbpf.cpp, WaechterEbpf.hpp)
    - Retrieves all events from eBPF and handles them
    - Keeps track of all sockets in SystemMap.cpp
    - Handles traffic statistics and rules (Counters.cpp)
- Source/Client/ - Contains the client application that communicates with the daemon
    - Gui uses imgui and glfw for rendering
    - Different windows are in Source/Client/Gui/Windows/
- An eBPF program in Source/Daemon/EBPF
    - Split up into multiple header files
    - Tracks socket creation, destruction, state change and traffic
- Shared utility code is in Source/Util
    - Contains data structures for the client daemon communication in Source/Util/Data
    - Data structures are (de)serialized using cereal library and sent through a Unix socket
- Thirdparty libraries are in Thirdparty/