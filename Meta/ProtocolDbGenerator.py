#!/usr/bin/env python3

import re
from collections import OrderedDict

SERVICES_FILE = "/etc/services"
OUTPUT_CPP = "BuilltinProtocolDB.cpp"

tcp = OrderedDict()
udp = OrderedDict()

line_re = re.compile(r'^(\S+)\s+(\d+)/(tcp|udp)')

with open(SERVICES_FILE, "r") as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("#"):
            continue

        m = line_re.match(line)
        if not m:
            continue

        name, port, proto = m.groups()
        port = int(port)

        # Keep first name only (canonical)
        if proto == "tcp" and port not in tcp:
            tcp[port] = name
        elif proto == "udp" and port not in udp:
            udp[port] = name

with open(OUTPUT_CPP, "w") as out:
    out.write("""#include <unordered_map>
#include <string>
#include <cstdint>

const std::unordered_map<uint16_t, std::string> GTCPServices = {
""")
    for port, name in tcp.items():
        out.write(f'    {{{port}, "{name}"}},\n')

    out.write("""};

const std::unordered_map<uint16_t, std::string> GUDPServices = {
""")
    for port, name in udp.items():
        out.write(f'    {{{port}, "{name}"}},\n')

    out.write("};\n")

print(f"Wrote {OUTPUT_CPP}")

