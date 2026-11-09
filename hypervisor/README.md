# Hypervisor stub

Boots **mini-OS as an EL1 guest**. Own `Makefile` + `linker.ld` (mini-OS sources untouched).

## Memory layout

```text
0x40000000  .guest      mini-os.bin  (EL1 guest)
0x44000000  hypervisor  EL2 code, VBAR_EL2, stacks
```

## Run

```sh
# from repo root (builds mini-os.bin if needed):
make -C hypervisor run
```

Expect:

```text
[hyp] running at EL2
[hyp] layout: guest@0x40000000 hyp@0x44000000
[hyp] eret -> mini-OS guest
[hyp] PSCI CPU_ON
[hyp] PSCI CPU_ON
[hyp] PSCI CPU_ON
<6>SMP: boot CPU is CPU0
<6>CPU1: online
...
~ #
```

QEMU `-smp 4` with `virtualization=on` uses **PSCI via SMC** and leaves CPUs 1–3 powered off. Guest `hvc` CPU_ON is handled at EL2: save the guest entry, then **`smc` the same CPU_ON** so QEMU starts that vCPU at an EL2 wait stub, which `eret`s into `secondary_startup`.

## HVC / PSCI

Guest `psci_cpu_on(cpu, secondary_startup)` is handled at EL2: store the entry, `sev`, return `0`. Other HVCs still print `[hyp] HVC received` and return (so `halt`/`POWER_OFF` is not emulated yet).
