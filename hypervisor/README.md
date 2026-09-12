# Hypervisor stub

Independent from mini-OS. Own `Makefile` + `linker.ld`.

## Current stage: HVC round-trip

```text
EL2 boot
  → eret → EL1
  → hvc #0
  → EL2 handler prints "[hyp] HVC received"
  → eret
  → EL1 continues
```

```sh
cd hypervisor
make run
```

Expect:

```text
[hyp] running at EL2
[hyp] now at EL1, issuing HVC
[hyp] HVC received
[hyp] back at EL1 after HVC
```

## Later

- Load mini-OS as EL1 guest
- Stage-2 MMU, traps, virt devices
