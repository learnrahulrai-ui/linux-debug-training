#!/bin/sh
# Read-only probe. No root, sysctl writes, modprobe, compilation or execution.
set -u
printf 'UNAME: '; uname -a
printf 'BITS: '; getconf LONG_BIT
printf 'CPU: '; grep -m1 '^model name' /proc/cpuinfo || true
printf 'GCC: '; gcc --version | head -1
printf 'LIBURING pkg-config: '; pkg-config --modversion liburing 2>/dev/null || echo unknown
printf 'LIBURING header: '; if printf '#include <liburing.h>\n' | cc -x c -E - >/dev/null 2>&1; then echo yes; else echo no; fi
printf 'KERNEL io_uring_disabled: '; cat /proc/sys/kernel/io_uring_disabled 2>/dev/null || echo unreadable
printf 'TLS module: '; modinfo -n tls 2>/dev/null || echo not_listed
printf 'KERNEL CFG: '; f="/boot/config-$(uname -r)"; if [ -r "$f" ]; then grep -E '^(CONFIG_TLS=|CONFIG_IO_URING=|CONFIG_KPROBE_EVENTS=|CONFIG_BPF=)' "$f" || true; else echo not_readable; fi
printf 'MEMAVAILABLE: '; grep '^MemAvailable:' /proc/meminfo || true
