#!/usr/bin/env bash
#
# ec2-bootstrap.sh — one-time host preparation for the Lite-Exchange v2 build.
#
# TWO LAYERS, independent:
#   Layer A  packages    — toolchain + libraries via apt
#   Layer B  host tuning — split by HOW THE KNOB PERSISTS:
#     B1  kernel cmdline  isolcpus / nohz_full / rcu_nocbs
#                         -> kernel parses it ONCE, at boot.
#                         -> change = edit GRUB + update-grub + REBOOT.
#     B2  sysctl          vm.nr_hugepages / net.core.*mem_max
#                         -> live /proc/sys knob; 'sysctl -w' is lost on
#                            reboot; /etc/sysctl.d/*.conf makes it persist.
#     B3  cpufreq         performance governor / turbo
#                         -> live /sys knob, NOT persisted (needs a unit).
#                         -> a virtualized EC2 guest has no cpufreq driver:
#                            the hypervisor owns frequency. Script skips it.
#
# Usage (run as root):
#   sudo ./ec2-bootstrap.sh packages
#   sudo ./ec2-bootstrap.sh tune        # B1 needs a reboot afterwards
#   sudo ./ec2-bootstrap.sh all
#
set -euo pipefail

# --- Tunables ---------------------------------------------------------------
# This box: 4 physical cores, SMT on. Siblings: {0,4} {1,5} {2,6} {3,7}.
# Isolate two WHOLE physical cores (both SMT siblings of each) for matching
# shards; leave cores 0-1 (CPUs 0,1,4,5) for the kernel, IRQs, housekeeping.
ISOLATED_CPUS="2,3,6,7"
HUGEPAGES_2M=512                     # 512 * 2 MiB = 1 GiB reserved pool

log()       { printf '\n=== %s ===\n' "$*"; }
need_root() { [[ ${EUID} -eq 0 ]] || { echo "run as root (sudo)"; exit 1; }; }

# --- Layer A: packages ------------------------------------------------------
layer_a_packages() {
  log "Layer A — packages"
  apt-get update
  # NB: HdrHistogram_c has no Ubuntu package — it is pulled via CMake
  # FetchContent in Phase 7, where it is first needed.
  apt-get install -y --no-install-recommends \
    build-essential clang lld clang-tidy \
    cmake ninja-build pkg-config \
    liburing-dev libgtest-dev libbenchmark-dev \
    libnuma-dev numactl util-linux \
    python3 python3-pip
  # perf ships in a kernel-version-specific package; best-effort so a name
  # mismatch does not abort the whole script.
  apt-get install -y "linux-tools-$(uname -r)" linux-tools-aws \
    || echo "WARN: perf tools not found for $(uname -r) — install manually."
}

# --- Layer B1: kernel command line (BOOT-ONLY) ------------------------------
layer_b1_cmdline() {
  log "Layer B1 — kernel command line (isolcpus / nohz_full / rcu_nocbs)"
  local dropin=/etc/default/grub.d/99-lx-tuning.cfg
  local params="isolcpus=${ISOLATED_CPUS} nohz_full=${ISOLATED_CPUS} rcu_nocbs=${ISOLATED_CPUS}"
  # Ubuntu's grub-mkconfig sources /etc/default/grub THEN grub.d/*.cfg, so a
  # drop-in that appends to $GRUB_CMDLINE_LINUX is idempotent — just overwrite.
  cat > "${dropin}" <<EOF
# Managed by ec2-bootstrap.sh — Lite-Exchange v2 host tuning.
# BOOT-ONLY parameters: the kernel parses these once, at boot. There is no
# way to change isolcpus/nohz_full/rcu_nocbs on a running kernel.
GRUB_CMDLINE_LINUX="\${GRUB_CMDLINE_LINUX} ${params}"
EOF
  update-grub
  echo ">> wrote ${dropin}"
  echo ">> REBOOT required before B1 takes effect:  sudo reboot"
  echo ">> verify after reboot:  cat /proc/cmdline"
}

# --- Layer B2: sysctl (LIVE knob, persisted via /etc/sysctl.d) --------------
layer_b2_sysctl() {
  log "Layer B2 — sysctl (huge pages, socket buffers)"
  local conf=/etc/sysctl.d/99-lx-tuning.conf
  cat > "${conf}" <<EOF
# Managed by ec2-bootstrap.sh — Lite-Exchange v2 host tuning.
# Live /proc/sys knobs. This file makes them persist: at boot
# systemd-sysctl.service replays every /etc/sysctl.d/*.conf.
vm.nr_hugepages = ${HUGEPAGES_2M}
net.core.rmem_max = 16777216
net.core.wmem_max = 16777216
EOF
  # Apply now, no reboot. Reserving huge pages needs CONTIGUOUS physical RAM;
  # doing it at boot (via this file) is more reliable than late, once RAM
  # is fragmented.
  sysctl --system
  echo ">> $(grep HugePages_Total /proc/meminfo)"
}

# --- Layer B3: cpufreq governor (LIVE knob, NOT persisted) ------------------
layer_b3_cpufreq() {
  log "Layer B3 — CPU frequency governor"
  if [[ ! -d /sys/devices/system/cpu/cpu0/cpufreq ]]; then
    echo ">> no cpufreq driver — virtualized EC2 guest; the hypervisor owns"
    echo ">> frequency scaling. Skipped. (Governor pinning is a bare-metal /"
    echo ">> .metal-instance task.)"
    return 0
  fi
  for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo performance > "${g}"
  done
  # cpufreq resets every boot — persist with a oneshot unit.
  cat > /etc/systemd/system/lx-cpufreq.service <<'EOF'
[Unit]
Description=Lite-Exchange: pin CPU governor to performance
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'for g in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do echo performance > "$g"; done'

[Install]
WantedBy=multi-user.target
EOF
  systemctl daemon-reload
  systemctl enable lx-cpufreq.service
  echo ">> governor set to performance now and on every boot"
}

main() {
  need_root
  case "${1:-}" in
    packages) layer_a_packages ;;
    tune)     layer_b1_cmdline; layer_b2_sysctl; layer_b3_cpufreq ;;
    all)      layer_a_packages; layer_b1_cmdline; layer_b2_sysctl; layer_b3_cpufreq ;;
    *) echo "usage: $0 {packages|tune|all}"; exit 1 ;;
  esac
  log "done"
}
main "$@"
