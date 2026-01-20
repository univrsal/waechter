---
title: Documentation
---

Wächter is a tool to monitor and control network traffic on Linux.

It's currently in early development so there's no ready to use builds but it can be compiled from source for testing.

## Quickstart

If you're using an Arch-based distribution you can install waechter from
the AUR package [waechter-git](https://aur.archlinux.org/packages/waechter-git/).
The daemon config file will be installed to `/etc/waechterd/waechterd.ini`, see [Configuration](#configuration) for more
information on how to adjust it, specifically the part about the network interface name and the daemon group.
The AUR package automatically adds the `waechter` group to your system, so just add your user to it with:

```terminal
sudo gpasswd -a $USER waechter
```

### Build requirements

To compile and use Wächter you'll need the following things:

- [CMake](https://cmake.org)
- [clang](https://llvm.org)
- [libbpf](https://github.com/libbpf/libbpf)
- [git](https://git-scm.com)
- A recent Linux kernel (currently only tested with 6.17 but 5 should be fine as well)

### Compiling

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

The GUI is self-contained and can be placed anywhere you want. The `waechterd` and `waechter-iplink` binary need to
placed together in a folder.

### Configuration

The daemon also needs a config file, if none is provided it will use the defaults which most likely won't work. An
example config is provided
in `Meta/Package/waechterd.ini`. The AUR package also installs this file to
`/etc/waechterd/waechterd.ini`.

You'll have to adjust the following entries:

- `interface`, set this to your primary network interface. You can usually list them with the command `ip addr`, usually
  it's the second interface after `lo`
- `ingress_interface`, this one only needs to be changed if you're using a VPN. VPNs will create their own interface
  through which your traffic goes. For download bandwidth limits to work you need to find your VPNs interface name and
  use it here. If you do not use a VPN you can just set this to the same value as `interface`
- `user` and `group` the daemon does not run as root, it only needs it run during initialization, afterward the daemon
  will run as this user. For testing, you can just set to your current user.
- `socket_path` the gui communicates with the daemon via a unix socket, you can change where this socket is created
  here.
  Alternatively you can change the path to `ws://x.x.x.x:port` to use a WebSocket instead (i.e. `ws://127.0.0.1:9876`).
  Note that you'll need a `websocket_auth_token` to connect and a reverse proxy to use secure WebSocket connections over
  TLS.
- `websocket_auth_token` (optional), if you're using WebSocket connections instead of Unix sockets, you can set an
  authentication
  token here. When set, clients must provide this token in the `Authorization` header as a Bearer token. This helps
  secure remote connections to the daemon. Leave this empty or commented out if you're using Unix sockets.

Make sure that any user that wants to use the GUI to connect to the daemon is part of the group specified in the config,
otherwise the GUI won't be able to access the daemon socket.

Then just run the daemon with root privileges and start the GUI to connect to it.
