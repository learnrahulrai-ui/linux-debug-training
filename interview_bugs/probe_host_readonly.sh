#!/bin/sh
# Read-only system facts. No sudo. No persistent changes.
printf '=== KERNEL ===\n'; uname -a
printf '=== ARCH ===\n'; uname -m; getconf LONG_BIT
printf '=== CPU ===\n'; grep -m1 -E '^model name|^Hardware' /proc/cpuinfo; grep -m1 '^flags' /proc/cpuinfo | tr ' ' '\n' | grep -E '^(vmx|svm)$' || true
printf '=== COMPILER ===\n'; gcc --version | head -1
printf '=== KVM ===\n'; if [ -e /dev/kvm ]; then ls -l /dev/kvm; else printf 'not found\n'; fi
printf '=== CGROUP FS ===\n'; stat -fc '%T' /sys/fs/cgroup 2>/dev/null || true
printf '=== CGROUP SELF ===\n'; cat /proc/self/cgroup
printf '=== V2 FREEZER ===\n'; ls -l /sys/fs/cgroup/cgroup.freeze 2>/dev/null || true; cat /sys/fs/cgroup/cgroup.freeze 2>/dev/null || true
printf '=== V1 FREEZER ===\n'; grep 'freezer' /proc/cgroups || true; grep 'cgroup' /proc/mounts | grep 'freezer' || true
printf '=== CONFIG ===\n';
KERNEL_CFG="/boot/config-$(uname -r)"
if [ -r "$KERNEL_CFG" ]; then grep -E '^(CONFIG_KVM=|CONFIG_KVM_AMD=|CONFIG_KVM_INTEL=|CONFIG_CGROUP_FREEZER=|CONFIG_PROVE_LOCKING=|CONFIG_DETECT_HUNG_TASK=|CONFIG_X86_32=)' "$KERNEL_CFG" || true
elif [ -r /proc/config.gz ]; then zcat /proc/config.gz | grep -E '^(CONFIG_KVM=|CONFIG_KVM_AMD=|CONFIG_KVM_INTEL=|CONFIG_CGROUP_FREEZER=|CONFIG_PROVE_LOCKING=|CONFIG_DETECT_HUNG_TASK=|CONFIG_X86_32=)' || true
else printf 'kernel config not readable\n'; fi
printf '=== 32-BIT TOOLCHAIN CHECK ===\n'
printf 'int main(void){return 0;}\n' | gcc -m32 -x c - -o /dev/null 2>&1
printf 'multilib_status=%s\n' "$?"
printf '=== END ===\n'
