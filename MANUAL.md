# Manual (English)

*[日本語版はこちら](MANUAL.ja.md)*

How to use the RPN (Reverse Polish Notation) calculator firmware for the
M5Stack Cardputer ADV. Like an HP calculator, a 4-level X/Y/Z/T register
stack is always visible on screen; you push numbers onto it and transform
them with functions and operators. This is not the typed-expression
(algebraic, `2*sin(pi/4)`-style) calculator.

## The basic idea: the stack

The screen always shows four registers, top to bottom: **T, Z, Y, X**. X is
the bottom register (where you operate from). Every new number you enter
shifts X→Y, Y→Z, Z→T (the old T is lost); every operation folds values back
down.

```
T   0
Z   0
Y   3
X   5
```

Pressing `+` here consumes Y(3) and X(5), puts `8` in X, and everything
above drops down by one:

```
T   0
Z   0
Y   0
X   8
```

To compute "3 + 5", type `3 Enter 5 +` — **number, number, operator**. That's
the essence of RPN.

## Entering numbers

| Key | Action |
|---|---|
| a number, decimal point included | Append to X's entry buffer (bottom line), one character at a time -- nothing is computed yet |
| `Enter` | Push the number being typed onto the stack (X→Y→Z→T shift) |
| `Enter` with nothing typed | Duplicate X into Y (the classic "ENTER ENTER" trick, e.g. for squaring) |
| `opt` + `-` (CHS) | Flip the sign of the number being typed, or of X itself |
| `Backspace` (while typing) | Delete the last character; deleting everything cancels the entry |
| `Backspace` (not typing) | Clear X to 0 (CLX) |

**About negative numbers**: the `-` key is **always subtract** here (see
below), exactly like a real RPN/HP calculator. To enter a negative number,
type the positive value first, then flip its sign with `opt+-` (CHS).

```
5 opt+- Enter   ->  pushes -5 onto the stack
```

## Scientific notation

While typing a number, press `e` to switch to the exponent part -- it's
just another character in the number, the same as `.`.

```
6 . 0 2 2 e 2 3 Enter   =>  pushes 6.022e23
```

For a **negative exponent**, a `-` typed right after `e` is the exponent's
sign, not subtraction:

```
1 e - 6 Enter   =>  pushes 1e-6 (0.000001)
```

Once `e` has been typed, `opt+-` (CHS) flips the **exponent's** sign
instead of the mantissa's (before `e` is typed, it still flips the
mantissa's sign as usual).

## Operators (apply immediately, no Enter needed)

| Key | Action |
|---|---|
| `+` | Y + X |
| `-` | Y - X |
| `*` | Y * X |
| `/` | Y / X |
| `^` | Y to the power of X (same as `pow`) |
| `%` | Y modulo X (`fmod`, sign follows Y) |
| `!` | Factorial of X (Y is untouched) |

Operators automatically finalize any number you're mid-typing before
computing. They also work directly on an already-pushed X and Y — `3 Enter
5 +` and `3 Enter 5 Enter +` give the same result.

## Calling functions (no parentheses needed)

Type a function's name with the letter keys, then press `Enter` to run it.
Unlike an algebraic calculator, **arguments are never written in
parentheses — they're already sitting on the stack** before you type the
name.

| Arity | How to use | Example |
|---|---|---|
| 0 (pushes a new value) | `name` Enter | `pi` Enter → pushes π |
| 1 (applies to X) | `name` Enter | `9 Enter sqrt` Enter → sqrt(9) = 3 |
| 2 (applies to Y, X) | `name` Enter | `2 Enter 10 Enter pow` Enter → 2^10 = 1024 |
| 3 (applies to Z, Y, X) | `name` Enter | `15 Enter 0 Enter 10 Enter clamp` Enter → 10 |

For 2-argument functions, the value pushed *first* becomes Y and the value
pushed *second* becomes X. `pow` computes `Y^X`, so `2 Enter 10 Enter pow`
means "2 to the 10th power".

## Tab-completion for function names

Press **Tab** while typing a function name to complete it. Press again to
cycle to another candidate sharing the same prefix.

```
si<Tab>   ->  sin   (Tab again: sinh, then: sqrt, then: sq, then: sto( ...)
sq<Tab>   ->  sq
sa<Tab>   ->  save
```

Candidates are ordered so that commonly-used math functions come first, and
utility commands like `save`/`sleeptime` come last. From a single letter you
may need several Tab presses to reach a utility command — typing 2-3 letters
narrows it down in one press instead.

Names that need an argument (`sto`, `rcl`, `timeset`, `dateset`) automatically
get a trailing `(` appended when completed. Every other function/constant
completes bare, with no parentheses.

## Stack manipulation keys

| Key | Action |
|---|---|
| `fn` + `;` | Roll up (R↑): T→X, X→Y, Y→Z, Z→T. **Brings T into X in a single press** |
| `fn` + `.` | Roll down (R↓): X→T, Y→X, Z→Y, T→Z (the reverse rotation) |
| `fn` + `S` | Swap X and Y |
| `fn` + `,` | Move the edit cursor left while typing |
| `fn` + `/` | Move the edit cursor right while typing |
| `fn` + `Backspace` | Clear the whole stack (X, Y, Z, T all to 0) |

The `;` key has an up-arrow printed on it, `.` a down-arrow, and `,`/`/` a
left/right arrow respectively.

## Memory registers (STO/RCL)

Separately from the stack, there are 10 numbered memory registers (0-9) for
stashing intermediate results.

| Command | Action |
|---|---|
| `sto(n)` Enter | Store X into register n (X itself is unchanged) |
| `rcl(n)` Enter | Push register n's value onto the stack, like a new number |

```
5 Enter sto(0)     ->  stores the 5 in X into register 0 (X stays 5)
...(other work)...
rcl(0)             ->  recalls that 5 back onto the stack
```

Memory registers are auto-saved to internal flash too, and survive power
loss.

## Complex numbers and polar coordinates (for AC circuit math)

There's no full "complex mode" like a real HP calculator (where every
register secretly carries a real and imaginary part). Instead, this uses a
lightweight convention: **two ordinary registers, read together, are one
complex number**.

- A single complex number: **Y=real, X=imaginary** (rectangular), or
  **Y=magnitude, X=angle** (polar)
- Two complex numbers span the whole stack: whichever was entered first
  ends up in **T,Z**, the one entered second in **Y,X**

| Function | Description | In/out |
|---|---|---|
| `r2p` | Rectangular to polar | 2-in (Y=x,X=y) -> 2-out (Y=r,X=theta); theta follows DEG/RAD |
| `p2r` | Polar to rectangular | 2-in (Y=r,X=theta) -> 2-out (Y=x,X=y) |
| `cadd`/`csub`/`cmul`/`cdiv` | Complex arithmetic | 4-in ((T,Z)=A, (Y,X)=B, each (real,imag)) -> 2-out (Y,X) = A op B |

**AC circuit example**: series combination of Z1=3+j4 ohm and Z2=1+j2 ohm
(add directly in rectangular form):

```
3 Enter -> 4 Enter -> 1 Enter -> 2 Enter -> cadd Enter
```

Result: Y=4 (real), X=6 (imaginary) -- a combined impedance of 4+j6 ohm.

For multiplying/dividing magnitude-and-phase values (parallel combinations,
gain calculations), convert to polar with `r2p` first, then use
`cmul`/`cdiv`.

**About vectors**: a 2D vector's magnitude and angle are exactly what `r2p`
already gives you (magnitude=r, angle=theta). Operations that need 3+
values at once (dot/cross products) don't fit in the 4-level stack, so use
the memory registers (`sto`/`rcl`) as scratch space, the same way a real HP
calculator would.

## DEG / RAD toggle

`opt` + `D` toggles the angle unit for every trig function (`sin cos tan
asin acos atan atan2`) between degrees and radians. The current mode is
shown top-left as `[DEG]` or `[RAD]`.

## On-device help

Type `help` and press Enter for an on-screen key/function reference.

- `fn` + `;` / `fn` + `.` — flip pages
- `Enter` or `Backspace` — return to the calculator

## Saving the stack

**Auto-save (internal flash)**: after every calculation, the four X/Y/Z/T
registers, the 10 memory registers, and the DEG/RAD setting are saved to the
ESP32's internal flash (NVS), and restored on boot. No SD card is needed,
and the stack survives a full power cycle.

**Manual export (microSD card)**: type `save` and press Enter. If a microSD
card is inserted, a snapshot of the current stack is appended to
`/rpn_log.txt` as plain text (a different filename from the algebraic
calculator's `calc_log.txt`, so the same SD card can be shared between both
calculators without one overwriting the other).

```
---- save #1 (RAD) ----
T = 0
Z = 0
Y = 3
X = 8

```

Each `save` **appends** a new block rather than overwriting. If there's no
SD card, or it fails to initialize, you'll see `SD ERR` and nothing else is
affected.

## Setting the time and date

This board has no RTC chip, so there's no clock or calendar unless you set
one by hand or via Wi-Fi (below).

- `timeset(H,M,S)` — set the current time (24h, e.g. `timeset(9,30,0)`)
- `time` — show the current time
- `dateset(Y,M,D)` — set the current date (e.g. `dateset(2026,9,17)`)
- `date` — show the current date as `YYYY-MM-DD`

Both reset to "unset" on every power cycle — re-run `timeset`/`dateset`
after each boot if you want them.

## Accessing the SD card without removing it

Type `usbdrive` and press Enter to expose the microSD card to a computer
over the same USB-C cable. **The calculator stops accepting key input**
once this runs; only a reset or power cycle brings it back.

If mounting fails on the host, reset the calculator and type `usbdebug` to
see the raw SD error that was recorded.

## Auto-sleep

Cardputer / Cardputer ADV has no real power-management chip, so **deep
sleep** stands in for power-off. By default it sleeps after **10 minutes**
of no key input.

- `sleeptime(n)` — set the idle timeout to `n` minutes (0 disables it)
- `sleeptime` (no args) — show the current setting

The physical **G0/BtnA button** on top of the device does double duty:
press it during normal use to sleep immediately, or press it while asleep
to wake up. The stack and settings are saved to flash right before
sleeping, and if Wi-Fi credentials are saved, waking via this button also
silently retries an NTP sync (a plain power-on never does this).

## Syncing time/date via Wi-Fi

- `wifi(ssid,pass)` — saves credentials to flash and immediately syncs the
  time and date via NTP (hardcoded to JST/UTC+9). Hyphens are fine in the
  SSID/password.
- `wifi()` — retries with the saved credentials
- `wifi` (no args) — shows the saved SSID, no side effects

The calculator never connects to Wi-Fi automatically on boot — it's
entirely opt-in.

## Battery and uptime

- `battery` — battery level (%) and voltage, e.g. `battery: 82% (4.05V)`.
  There's no dedicated fuel-gauge chip, so this is an estimate.
- `uptime` — time since the last boot or wake from sleep, e.g. `2d
  03:12:45`.

## Function reference

Every function reads its arguments from the stack and places its result in
X. The "call sequence" column shows exactly what to push before typing the
function name.

### Trigonometric (affected by the DEG/RAD setting)

| Function | Description | Call sequence (RAD) | Result |
|---|---|---|---|
| `sin` | Sine | `pi Enter 2 Enter / sin` | `1` |
| `cos` | Cosine | `pi Enter cos` | `-1` |
| `tan` | Tangent | `pi Enter 4 Enter / tan` | `1` |
| `asin` | Arcsine | `1 Enter asin` | `1.570796327` |
| `acos` | Arccosine | `0 Enter acos` | `1.570796327` |
| `atan` | Arctangent | `1 Enter atan` | `0.7853981634` |
| `atan2` | Angle from Y,X | `1 Enter 1 Enter atan2` | `0.7853981634` |

### Hyperbolic

| Function | Description | Call sequence | Result |
|---|---|---|---|
| `sinh` | Hyperbolic sine | `1 Enter sinh` | `1.175201194` |
| `cosh` | Hyperbolic cosine | `0 Enter cosh` | `1` |
| `tanh` | Hyperbolic tangent | `1 Enter tanh` | `0.7615941560` |
| `asinh` | Inverse hyperbolic sine | `1 Enter asinh` | `0.8813735870` |
| `acosh` | Inverse hyperbolic cosine (X≥1) | `1 Enter acosh` | `0` |
| `atanh` | Inverse hyperbolic tangent (-1<X<1) | `0.5 Enter atanh` | `0.5493061443` |

### Powers, roots, logarithms

| Function | Description | Call sequence | Result |
|---|---|---|---|
| `sqrt` | Square root | `16 Enter sqrt` | `4` |
| `cbrt` | Cube root | `27 Enter cbrt` | `3` |
| `inv` | Reciprocal (1/X) | `4 Enter inv` | `0.25` |
| `sq` | Square (X²) | `7 Enter sq` | `49` |
| `pow` | Y to the power of X | `2 Enter 10 Enter pow` | `1024` |
| `exp` | e to the power of X | `1 Enter exp` | `2.718281828` |
| `log` | Base-10 logarithm | `1000 Enter log` | `3` |
| `ln` | Natural logarithm | `2.718281828 Enter ln` | `1` |
| `log2` | Base-2 logarithm | `1024 Enter log2` | `10` |

### Comparison, range, number theory

| Function | Description | Call sequence | Result |
|---|---|---|---|
| `min` | Smaller of Y, X | `3 Enter 7 Enter min` | `3` |
| `max` | Larger of Y, X | `3 Enter 7 Enter max` | `7` |
| `clamp` | Consumes Z,Y,X: clamps Z into [Y (min), X (max)] | `15 Enter 0 Enter 10 Enter clamp` | `10` |
| `gcd` | Greatest common divisor | `12 Enter 18 Enter gcd` | `6` |
| `lcm` | Least common multiple | `4 Enter 6 Enter lcm` | `12` |
| `mod` | Mathematical modulo (sign follows X, the divisor -- unlike `%`) | `7 opt+- Enter 3 Enter mod` | `2` |

### Combinatorics and random numbers

| Function | Description | Call sequence | Result |
|---|---|---|---|
| `ncr` | Combinations (choose X from Y) | `5 Enter 2 Enter ncr` | `10` |
| `npr` | Permutations (choose X from Y) | `5 Enter 2 Enter npr` | `20` |
| `rand` | Push a random number in [0, 1) | `rand` | e.g. `0.4213` |
| `randint` | Random integer in [Y, X], inclusive | `1 Enter 6 Enter randint` | a dice roll, `1`-`6` |

### Rounding and factorial

| Function/key | Description | Call sequence | Result |
|---|---|---|---|
| `abs` | Absolute value | `5 opt+- Enter abs` | `5` |
| `floor` | Round down | `3.7 Enter floor` | `3` |
| `ceil` | Round up | `3.2 Enter ceil` | `4` |
| `round` | Round to nearest | `3.5 Enter round` | `4` |
| `int` | Truncate toward zero | `3.9 Enter int` | `3` |
| `!` (key) | Factorial | `5 Enter !` | `120` |
| `fact` (typed name) | Factorial, same as `!` | `5 Enter fact` | `120` |

### Memory and constants

| Name | Description | Call sequence |
|---|---|---|
| `pi` | Push π | `pi` |
| `e` | Push e | `e` |
| `sto(n)` | Store X into register n | `5 Enter sto(0)` |
| `rcl(n)` | Push register n onto the stack | `rcl(0)` |

## Error messages

An operation that can't be computed (division by zero, sqrt of a negative
number, acosh/atanh out of range, etc.) shows `ERR: ...` in red at the
bottom of the screen. The stack is left unchanged, so just re-enter the
correct value. A mistyped function name that gets Entered shows `ERR:
unknown '...'`.

## Why T comes after X, Y, Z

This follows the classic HP calculator naming convention. X, Y, Z evoke the
three spatial axes; the fourth register is named T for "Top of stack". It
suits the register's behavior too: even after a 2-argument operation
consumes X and Y, T's value is duplicated into the new Z — it stays put at
the top.
