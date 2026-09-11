# Hypervisor stub (stage 1)

Independent from mini-OS. Own `Makefile` + `linker.ld`.

## What this stage does

1. QEMU loads `hypervisor.elf` at `0x40000000`
2. If CPU is in **EL2**: print a line, configure a few EL2 regs, `eret` to EL1
3. In **EL1**: print a second line, then `wfi`
4. If firmware already starts in **EL1**: print that and park (no EL2 work)

mini-OS is **not** loaded yet.

## Build / run

```sh
cd hypervisor
make
make run
```

Expect something like:

```text
[hyp] running at EL2
[hyp] eret -> EL1 OK
```

## Next steps (later)

- EL2 exception vectors + `HVC`
- Embed or load `mini-os.bin` as Guest
- Stage-2 MMU, then traps / virt devices
