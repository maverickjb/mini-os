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
<6>SMP: ...
~ #
```

Use `-smp 1` for now (secondaries would skip this EL2 entry).

## HVC

Guest PSCI (`psci_cpu_on`) uses `hvc`, so you may see `[hyp] HVC received` during SMP bring-up. With `-smp 1` secondary bring-up fails harmlessly; a later step can emulate or forward PSCI.
