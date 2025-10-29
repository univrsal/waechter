Ebpf:

- [x] Re-add fetching of socket cookie
- [x] Re-add fetching of PID
- [x] Switch map to ring buffer
- [x] Move struct types to shared header between eBPF and user space

Ebpf -> Daemon:

- [ ] Assign a Src -> Dest IP pair to each socket
- [ ] Track socket state

- Daemon:

- [x] Read pid/cookie/packet header/total packet length from ring buffer
- [x] Track traffic statistics per binary/pid/src-dst pair
- [x] Remove closed processes from tracking map
- [x] Remove closed sockets from tracking map
- [ ] Don't immediately remove closed processes/sockets
- [ ] (maybe) compress messages

Gui:

- [x] Decode traffic tree
- [ ] Highlight about-to-be removed PIDs and Sockets
- [ ] Add traffic graph window
    - [ ] Show traffic over time upload/download for entire system
- [ ] Connection detail window (depends on selected row in traffic tree)
    - [ ] Show socket state
    - [ ] Show process details
    - [ ] Show app details
    - [ ] Show system details

Daemon -> Gui:

- [x] Send traffic tree updates
- [ ] Add/remove tree entries