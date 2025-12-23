---
title: Documentation
---

Wächter is a tool to monitor and control network traffic on Linux.

It's currently in early development so there's no ready to use builds but it can be compiled from source for testing.

## Quickstart

To compile and use Wächter you'll need the following things:

- [CMake](https://cmake.org)
- [clang](https://llvm.org)
- [libbpf](https://github.com/libbpf/libbpf)
- [git](https://git-scm.com)
- A recent Linux kernel (currently only tested with 6.17 but 5 should be fine as well)

After that the build process is fairly straight forward:

```terminal
git clone https://github.com/univrsal/waechter &&
cd waechter &&
mkdir build &&
cd build &&
cmake .. &&
make
```

Once the build has finished you'll find the binaries here:

- `./Source/Daemon/waechterd` - the daemon binary
- `./Source/Daemon/Net/IPLinkProc/waechter-iplink` - utility binary
- `./Source/Gui/waechter` - the GUI

The GUI is self contained and can be placed anywhere you want. The `waechterd` and `waechter-iplink` binary need to placed together in a folder.
The daemon also needs a config file, if none is provided it will use the defaults which most likely won't work. An example config is provided
in `Meta/waechterd.ini`.

You'll have to adjust the following entries:

- `interface`, set this to your primary network interface. You can usually list them with the command `ip addr`, usually it's the second interface after `lo`
- `ingress_interface`, this one only needs to be changed if you're using a VPN. VPNs will create their own interface through which your traffic goes. For download bandwidth limits to work you need to find your VPNs interface name and use it here. If you do not use a VPN you can just set this to the same value as `interface`
- `user` and `group` the daemon does not run as root, it only needs it run during initialization, afterwards the daemon will run as this user. For testing you can just set to your current user.

Then just run the daemon with root privileges and start the GUI to connect to it.
