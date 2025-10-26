Ebpf:

- [x] Re-add fetching of socket cookie
- [x] Re-add fetching of PID
- [x] Switch map to ring buffer
- [ ] Move struct types to shared header between eBPF and user space

Daemon:

- [x] Read pid/cookie/packet header/total packet length from ring buffer
- [ ] Track traffic statistics per binary/pid/src-dst pair