# Faulty Kernel Module Oops

The oops resulting from the echo call to the char device...

```console
# echo "hello_world" >> /dev/faulty 
```

...leads to the following Linux kernel Oops:

```console
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=000000004262b000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 156 Comm: sh Tainted: G           O      5.15.18 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xa8/0x2b0
sp : ffffffc008d13d80
x29: ffffffc008d13d80 x28: ffffff80025e3fc0 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000040000000 x22: 000000000000000c x21: 00000055936033c0
x20: 00000055936033c0 x19: ffffff800263b100 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc0006f7000 x3 : ffffffc008d13df0
x2 : 000000000000000c x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x40/0xa0
 el0_svc+0x20/0x60
 el0t_64_sync_handler+0xe8/0xf0
 el0t_64_sync+0x1a0/0x1a4
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace 2638657df177a865 ]---
```

The Oops information states that the modules tries to dereference a pointer at
address 0x0, which leads to a null pointer exception (`Unable to handle kernel
NULL pointer dereference at virtual address 0000000000000000`). After some
context information of the current kernel state, the linked modules are stated
as a first indication:

```console
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
```

Of special interest is the value of the PC (program counter), which is
logged as well:

```console
pc : faulty_write+0x14/0x20 [faulty]
```

The information in the line is vital to identify the call which caused
the Oops:

- `faulty_write` - the function where the error occured
- `+0x14` - the address of the instruction that caused the error as offset to the
memory location of the `faulty_write`function (dec. 20)
- `/0x20` - the total length of the function (dec. 32)
- `[faulty]` - the module in which the error occured

To inspect the fault location further, `objdump` can be used to show the
assembly of the resp. module:


```console
$ aarch64-none-linux-gnu-objdump -d buildroot/output/target/lib/modules/5.15.18/extra/faulty.ko 

buildroot/output/target/lib/modules/5.15.18/extra/faulty.ko:     file format elf64-littleaarch64


Disassembly of section .text:

0000000000000000 <faulty_write>:
   0:	d503245f 	bti	c
   4:	d2800001 	mov	x1, #0x0                   	// #0
   8:	d2800000 	mov	x0, #0x0                   	// #0
   c:	d503233f 	paciasp
  10:	d50323bf 	autiasp
  14:	b900003f 	str	wzr, [x1]
  18:	d65f03c0 	ret
  1c:	d503201f 	nop

[...]
```

Note only the relevant parts have been extracted above. The assembly of the `faulty_write`
has the statement `str wzr, [x1]` at the location mention in the kernel Oops, which
tries to use `store` command to put the contents of the `wzr` into the memory location
referenced by `x1`. Some lines above, the location has been set to 0x0: `mov x1, #0x0`.
This causes the error.