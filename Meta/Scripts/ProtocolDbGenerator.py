#!/usr/bin/env python3

import re
from collections import OrderedDict

SERVICES_FILE = "/etc/services"
OUTPUT = "BuilltinProtocolDB.json"

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
import json
with open(OUTPUT, "w") as out:
    json.dump({"tcp": tcp, "udp": udp}, out, indent=4)
    

print(f"Wrote {OUTPUT}")

