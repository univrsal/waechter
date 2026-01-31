---
title: Documentation
---

<iframe
width="100%"
style="aspect-ratio:16/9"
src="https://www.youtube.com/embed/r0jEP4FqzbI"
title="YouTube video player"
frameborder="0"
allow="accelerometer; clipboard-write; encrypted-media; gyroscope; picture-in-picture"
allowfullscreen
> </iframe>

[Local version of the video](/intro.mp4)

Wächter is a tool to monitor and control network traffic on Linux.

It's currently in early development so expect some bugs and crashes but the basic
features should be working.

If you have questions you can
use [discussions](https://github.com/univrsal/waechter/discussions), #waechter on [rizon](https://rizon.net)
or [discord](https://discord.gg/kZsHuncu3q). Issues can be reported on [github](https://github.com/univrsal/waechter/issues).

### How am I supposed to pronounce Wächter?

Pick whatever option you like:

- like the German word for guardian,
- a weird variant of the word "vector",
- the 'w' like the 'v' in vase, the 'ä' like the 'a' in bat, the 'ch' like an angry cat hissing, the 't' like in bat and the 'er' is basically the same in English or
- something completely else.

### Why does it say "unregistered"?

See [registering](/register/index.md).

# Quickstart

If you're using an Arch-based distribution you can install waechter from
the AUR package [waechter-git](https://aur.archlinux.org/packages/waechter-git/).
The daemon config file will be installed to `/etc/waechterd/waechterd.ini`, see [Configuration](#configuration) for more
information on how to adjust it, specifically the part about the network interface name and the daemon group.

If you have the daemon installed on a server and configured it to use the WebSocket server and just need
a quick way to connect to it, you can use the client directly from your broser:

- For the stable version: [waechter.st/client](/client)
- For nighly builds: [waechter.st/client-nightly](/client-nightly)

The AUR package automatically adds the `waechter` group to your system, so just add your user to it with:

```shell
sudo gpasswd -a $USER waechter
```

## Installing

For arch-based distributions you can install `waechter-git` via your AUR manager. For Debian based distributions you can
install the .deb package from the
[releases page](https://github.com/univrsal/waechter/releases):

```shell
sudo dpkg -i waechter_<version>_linux-amd64.deb
```

For other distributions you'll have to compile Wächter
from source,
see [Compiling](#compiling) for instructions.

### Verifying releases

To verify the releases you can import the public key with:

```shell
curl -sL waechter.st/pub.asc | gpg --import
```

The keys fingerprint is:

```shell
C89F 8282 BB72 68E0 889A  7572 B463 3609 015D 13FF
```

Then you can verify the signature of the release files with:

```shell
gpg --verify waechter_<version>_linux-amd64.deb.asc waechter_<version>_linux-amd64.deb
```

## Build requirements

To compile and use Wächter you'll need the following things:

- [CMake](https://cmake.org),
- [clang](https://llvm.org),
- [libbpf](https://github.com/libbpf/libbpf),
- [git](https://git-scm.com) and 
- a recent Linux kernel (currently only tested with 6.17 but 5 should be fine as well).

## Compiling

After that the build process is fairly straight forward:

```shell
git clone https://github.com/univrsal/waechter &&
cd waechter &&
mkdir build &&
cd build &&
cmake .. &&
make
```

Once the build has finished you'll find the binaries here:

- `./Source/Daemon/waechterd` - the daemon binary,
- `./Source/Daemon/Net/IPLinkProc/waechter-iplink` - utility binary and
- `./Source/Gui/waechter` - the GUI.

The GUI is self-contained and can be placed anywhere you want. The daemon can also be placed anywhere and will look
for its config file in `/etc/waechterd/waechterd.ini` or in the current working directory. The waechter-iplink utility
has to be placed in a folder that is in your $PATH so the daemon can launch it.

## Configuration

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
- `websocket_auth_token`, if you're using WebSocket connections instead of Unix sockets, you have to set an
  authentication token here. Clients must provide this token in the `Authorization` header as a Bearer token. If it is
  left to the default value (`change_me`) the server will generate a random string at each startup.

Make sure that any user that wants to use the GUI to connect to the daemon is part of the group specified in the config,
otherwise the GUI won't be able to access the daemon socket.

Then just run the daemon with root privileges and start the GUI to connect to it.

## WebSocket remote connections

If you want to connect to the daemon remotely you can make it use a websocket server instead of a unix socket by
changing the `socket_path` entry in the config to something like `ws://127.0.0.1:<port>`. To secure the connection you
will need a reverse proxy like nginx or caddy that provides TLS encryption. An example nginx config could look like
this:

```nginx
    # ... other config options ...
    location /ws/ {
        proxy_pass http://127.0.0.1:8890;

        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";

        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        proxy_read_timeout 86400;
        proxy_send_timeout 86400;

        proxy_buffering off;
    }
```

Afterward you can connect to the daemon using `wss://<your-domain>/ws/` as the socket path in the GUI under Tools >
Settings. Make sure to set
a `websocket_auth_token` in the daemon config and provide the same token in the GUI to authenticate the connection.
