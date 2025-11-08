Ebpf:

- [x] Re-add fetching of socket cookie
- [x] Re-add fetching of PID
- [x] Switch map to ring buffer
- [x] Move struct types to shared header between eBPF and user space

Ebpf -> Daemon:

- [ ] Assign a Src -> Dest IP pair to each socket
    - [x] TCP connect sockets
    - [ ] UDP sockets (?)
    - [ ] TCP accepted sockets (?)
- [x] Track socket state

Daemon:

- [x] Read pid/cookie/packet header/total packet length from ring buffer
- [x] Track traffic statistics per binary/pid/src-dst pair
- [x] Remove closed processes from tracking map
- [x] Remove closed sockets from tracking map
- [x] Don't immediately remove closed processes/sockets
- [x] (maybe) compress messages
- [ ] Resolve IP addresses to hostnames asynchronously
- [ ] Handle accept event

Gui:

- [x] Decode traffic tree
- [x] Highlight about-to-be removed PIDs and Sockets
- [x] Add traffic graph window
    - [x] Show traffic over time upload/download for entire system
- [x] apps and processes showing up twice for some reason
- [x] Connection detail window (depends on selected row in traffic tree)
    - [x] Show socket state
    - [x] Show process details
    - [x] Show app details
    - [x] Show system details
- [ ] ? App traffic still gets stuck sometimes
- [x] Some sockets share the same item id
- [x] After last socket closes PID is no longer drawn, but app is still there and traffic is stuck
- [ ] Traffic graph should adjust y axis based on visible traffic data

Daemon -> Gui:

- [ ] Send icon atlas updates
- [x] Some apps have the entire command line as the name
- [x] There's an app with "" as the name for some reason
- [x] switch from json to cereal
- [x] Send traffic tree updates
- [x] Add/remove tree entries