# Bonus — Patching the binaries

Goal: modify each crackme so it accepts **any** input as valid (prints `Good job.`).
No runtime tricks: no `LD_PRELOAD`, no wrapper, no debugger. The patched ELF is
a stand-alone binary that has been altered byte-for-byte on disk.

The patched binaries live next to their `source.c` / `password`:

```
level1/level1_patched
level2/level2_patched
level3/level3_patched
```

Each patch is small (a handful of bytes) and was applied with `dd
conv=notrunc`. Original binaries are kept untouched in `binary/`.

---

## General method

Every crackme has the same shape: read input, evaluate it, branch to either a
"good" sink or a "nope" sink. The branch is a conditional jump in machine
code, e.g.

```
    call   strcmp
    cmp    eax, 0
    jne    <nope>           ; if not equal → fail
    ...                     ; "Good job."
```

Two byte-level techniques are enough for these three binaries:

1. **NOP a conditional jump** — replace the `jne`/`je` (typically 6 bytes for
   the `0F 8x` near-form) with `90 90 90 90 90 90`. The CPU skips the jump,
   execution falls through to the success branch.
2. **NOP a `call <fail>`** — replace `E8 xx xx xx xx` (5 bytes) with five
   `90`s. The fail handler is never invoked; control simply continues to the
   next instruction.

In a few cases I prefer **forcing a value** instead — e.g. replacing
`mov eax, [rbp-0x10]` (3 bytes `8B 45 F0`) with `xor eax, eax; nop`
(`31 C0 90`). After that, downstream comparisons that test that value fall
into whichever case I want.

For each binary I dump main with
`objdump -d -M intel binary/levelN`, identify the relevant gates, and use
their **file offsets** (not runtime VAs) to patch. For these ELFs the `.text`
section's `Off` and `Addr` are equal (`readelf -S`), so the VA shown in the
disassembly equals the byte offset in the file.

---

## level1 — `level1/level1_patched`

### What the binary does
Reads a string and compares it to `"__stack_check"` with `strcmp`. If equal,
prints `Good job.`; otherwise `Nope.`.

### The gate
```
1241: 83 f8 00              cmp    eax, 0x0          ; eax = strcmp result
1244: 0f 85 16 00 00 00     jne    1260 <main+0xa0>  ; → "Nope." branch
124a: ...                                            ; "Good job." branch
```

### The patch
NOP the 6-byte `jne` so both equal/non-equal results fall through to the
"Good job." branch.

| Offset | Original     | Patched              |
|--------|--------------|----------------------|
| 0x1244 | `0F 85 16 00 00 00` | `90 90 90 90 90 90` |

Command:
```sh
cp binary/level1 level1/level1_patched
printf '\x90\x90\x90\x90\x90\x90' \
  | dd of=level1/level1_patched bs=1 seek=$((0x1244)) count=6 conv=notrunc
chmod +x level1/level1_patched
```

### Verification
```
$ ./level1/level1_patched < level1/password
Please enter key: Good job.
$ echo garbage | ./level1/level1_patched
Please enter key: Good job.
$ echo "" | ./level1/level1_patched
Please enter key: Good job.
```

---

## level2 — `level2/level2_patched`

### What the binary does
Reads up to 23 chars. Validates several conditions, each calling `no()` (which
prints `Nope.` and `exit(1)`) on failure:

1. `scanf` returned exactly 1 (i.e. input was read).
2. `input[0] == '0'`.
3. `input[1] == '0'`.

Then it builds an 8-byte `pass` from 3-digit ASCII triples and compares it
against the literal `"delabere"` with `strcmp`.

### The gates
Each early failure is a direct `call no` (5-byte `E8`-call):

```
1327: e8 f4 fe ff ff       call 1220 <no>   ; after scanf check
1340: e8 db fe ff ff       call 1220 <no>   ; after input[0] != '0'
1359: e8 c2 fe ff ff       call 1220 <no>   ; after input[1] != '0'
```

The final decision after `strcmp`:

```
146a: 83 f8 00              cmp    eax, 0x0
146d: 0f 85 0d 00 00 00     jne    1480 <main+0x1b0>  ; → "Nope." branch
1473: e8 25 fe ff ff        call   12a0 <ok>          ; → "Good job."
```

### The patch
NOP each early `call no` (so the early checks become harmless) and NOP the
final `jne` (so every input reaches `call ok`).

| Offset | Size | Original                   | Patched                    | Effect |
|--------|------|----------------------------|----------------------------|--------|
| 0x1327 | 5    | `E8 F4 FE FF FF`           | `90 90 90 90 90`           | skip first `call no` |
| 0x1340 | 5    | `E8 DB FE FF FF`           | `90 90 90 90 90`           | skip second `call no` |
| 0x1359 | 5    | `E8 C2 FE FF FF`           | `90 90 90 90 90`           | skip third `call no` |
| 0x146d | 6    | `0F 85 0D 00 00 00`        | `90 90 90 90 90 90`        | always reach `call ok` |

Commands:
```sh
cp binary/level2 level2/level2_patched
for off in 0x1327 0x1340 0x1359; do
  printf '\x90\x90\x90\x90\x90' \
    | dd of=level2/level2_patched bs=1 seek=$((off)) count=5 conv=notrunc
done
printf '\x90\x90\x90\x90\x90\x90' \
  | dd of=level2/level2_patched bs=1 seek=$((0x146d)) count=6 conv=notrunc
chmod +x level2/level2_patched
```

### Verification
```
$ ./level2/level2_patched < level2/password
Please enter key: Good job.
$ echo garbage | ./level2/level2_patched
Please enter key: Good job.
$ echo "" | ./level2/level2_patched
Please enter key: Good job.
$ echo 99999999999999999999999 | ./level2/level2_patched
Please enter key: Good job.
```

---

## level3 — `level3/level3_patched`

### What the binary does
Same shape as level2 but obfuscated: instead of named `no`/`ok` helpers, the
"Nope" sink is `___syscall_malloc` at `0x12e0` and the "Good job" sink is
`____syscall_malloc` at `0x1300` (one extra underscore — same name pattern,
deliberately confusing). The check sequence is:

1. `scanf` returned 1.
2. `input[1] == '2'`.
3. `input[0] == '4'`.
4. Build an 8-byte `pass` (initialised with `'*'`) by decoding 3-digit ASCII
   triples; expected target is `"********"` (eight asterisks). Note `'*' == 42`
   in ASCII, which is why the input must start with `42` and each triple
   must be `042`.
5. The strcmp result is then dispatched through a switch over values
   `-2, -1, 0, 1, 2, 3, 4, 5, 0x73, default`. **Only the `== 0` arm calls the
   "Good job" function** (`call 1300`); all others call the "Nope" function
   (`call 12e0`).

### The gates
Three early `call ___syscall_malloc` (5 bytes each):

```
1360: e8 7b ff ff ff       call 12e0  ; after scanf check
1376: e8 65 ff ff ff       call 12e0  ; after input[1] != '2'
138c: e8 4f ff ff ff       call 12e0  ; after input[0] != '4'
```

The strcmp result is saved and re-loaded at `0x147d`:

```
1475: e8 f6 fb ff ff        call   strcmp@plt
147a: 89 45 f0              mov    [rbp-0x10], eax  ; save result
147d: 8b 45 f0              mov    eax, [rbp-0x10]  ; reload result
1480: 89 45 ac              mov    [rbp-0x54], eax  ; stash for dispatch
1483: 83 e8 fe              sub    eax, 0xfffffffe  ; case == -2
1486: 0f 84 aa 00 00 00     je     1536            ; → Nope
148c: e9 00 00 00 00        jmp    1491
...
14a2: 8b 45 ac              mov    eax, [rbp-0x54]
14a5: 85 c0                 test   eax, eax        ; case == 0
14a7: 0f 84 b1 00 00 00     je     155e             ; → ____syscall_malloc (Good job)
...
```

### The patch
- NOP the three early `call`s as in level2.
- Replace the 3-byte reload `mov eax, [rbp-0x10]` at `0x147d` with
  `xor eax, eax; nop` (`31 C0 90`). This forces `eax = 0`, which propagates
  to `[rbp-0x54]` and makes the dispatch land on the `test eax, eax / je 155e`
  arm — i.e. "Good job."

I deliberately did **not** patch the conditional jumps inside the dispatch
table. Forcing the saved value to zero is a single, surgical change and
leaves the rest of the function intact.

| Offset | Size | Original             | Patched           | Effect |
|--------|------|----------------------|-------------------|--------|
| 0x1360 | 5    | `E8 7B FF FF FF`     | `90 90 90 90 90`  | skip first `call ___syscall_malloc` |
| 0x1376 | 5    | `E8 65 FF FF FF`     | `90 90 90 90 90`  | skip second `call ___syscall_malloc` |
| 0x138c | 5    | `E8 4F FF FF FF`     | `90 90 90 90 90`  | skip third `call ___syscall_malloc` |
| 0x147d | 3    | `8B 45 F0`           | `31 C0 90`        | force strcmp-result-in-eax to 0 |

Commands:
```sh
cp binary/level3 level3/level3_patched
for off in 0x1360 0x1376 0x138c; do
  printf '\x90\x90\x90\x90\x90' \
    | dd of=level3/level3_patched bs=1 seek=$((off)) count=5 conv=notrunc
done
printf '\x31\xc0\x90' \
  | dd of=level3/level3_patched bs=1 seek=$((0x147d)) count=3 conv=notrunc
chmod +x level3/level3_patched
```

### Verification
```
$ ./level3/level3_patched < level3/password
Please enter key: Good job.
$ echo garbage | ./level3/level3_patched
Please enter key: Good job.
$ echo "" | ./level3/level3_patched
Please enter key: Good job.
$ echo 99999999999999999999999 | ./level3/level3_patched
Please enter key: Good job.
```

---

## Re-applying the patches from scratch

If you want to regenerate everything from the originals:

```sh
# level1
cp binary/level1 level1/level1_patched
printf '\x90\x90\x90\x90\x90\x90' \
  | dd of=level1/level1_patched bs=1 seek=$((0x1244)) count=6 conv=notrunc
chmod +x level1/level1_patched

# level2
cp binary/level2 level2/level2_patched
for off in 0x1327 0x1340 0x1359; do
  printf '\x90\x90\x90\x90\x90' \
    | dd of=level2/level2_patched bs=1 seek=$((off)) count=5 conv=notrunc
done
printf '\x90\x90\x90\x90\x90\x90' \
  | dd of=level2/level2_patched bs=1 seek=$((0x146d)) count=6 conv=notrunc
chmod +x level2/level2_patched

# level3
cp binary/level3 level3/level3_patched
for off in 0x1360 0x1376 0x138c; do
  printf '\x90\x90\x90\x90\x90' \
    | dd of=level3/level3_patched bs=1 seek=$((off)) count=5 conv=notrunc
done
printf '\x31\xc0\x90' \
  | dd of=level3/level3_patched bs=1 seek=$((0x147d)) count=3 conv=notrunc
chmod +x level3/level3_patched
```

## Why this is allowed under the project rules

The PDF forbids `LD_PRELOAD` and any "trick" that overrides library functions
at runtime. None of the patches above change the dynamic loader, intercept
`strcmp`, or rely on environment variables. Each patched ELF is a normal
executable that, when run with no special environment, prints `Good job.` for
any input. The original `strcmp` / `atoi` / `scanf` calls all still happen —
their *outcome* is just no longer used to decide success.
