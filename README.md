# FM2OP for Digitakt

FM2OP is a four-operator FM synth machine for the original Digitakt
(Mk1), OS 1.53. It replaces the previous Sophie-centric voice model with
a dedicated FM engine and keeps Digitakt's regular AMP,
filter, mixer and effects path. The source, not a modified Elektron OS,
is what this repository distributes.

## Changelog

- **S034:** Replaced the Sophie voice code with a dedicated 4-op FM machine.

---

<strong><font color="red">USE AT YOUR OWN RISK.</font></strong> This modifies your instrument's firmware; back up your projects and sounds, and keep a stock OS file for recovery.

One instance runs without FAST AUDIO; the original hardware
test found two instances practical with FAST AUDIO enabled. Eight-track
operation is **not** claimed.

## Controls

| SRC knob | Control | What it does |
| --- | --- | --- |
| A | TUNE | Pitch |
| B | ALGO | Operator routing (1-8, below) |
| C | RATIO | Selected operator's frequency ratio, 0.25 to 16 |
| D | OP | Which operator (1-4) RATIO, LEVEL, ATTK and DECAY edit; its knob shows the number |
| E | LEVEL | Selected operator's level: volume as a carrier, depth as a modulator |
| F | ATTK | Selected operator's envelope attack |
| G | DECAY | Selected operator's envelope decay; `INF` holds |
| H | CHAR | Operator 4 feedback and overall brightness |

Algorithms, where `>` means "modulates": 1 `4>3>2>1`, 2 `(3+4)>2>1`,
3 `(2 + 4>3)>1`, 4 `(2+3+4)>1`, 5 `2>1, 4>3`, 6 `4>(1, 2, 3)`,
7 `1, 2, 4>3`, 8 `1, 2, 3, 4`.

The sound saves only the operator OP shows; the other three are kept per
track and saved with the project, in a gap of the project's storage block
that the stock OS neither writes nor reads. They also survive a power cycle,
like any unsaved edit. They follow the track across
patterns, but not into the sound pool or saved kits, and a project saved
without FM2OP starts them at defaults. TUNE, ALGO and CHAR can be
parameter-locked; the per-operator controls cannot yet. The normal AMP page controls
the note envelope: set HOLD to `NOTE` for TRIG LEN to determine when the
release begins. Use a finite DEC to hear that release. DEC `INF` can keep
the sound going indefinitely. Retriggering chokes the previous voice on
that track, with a brief transition to suppress a click.


## Install: no compiler required

You need only the prebuilt
[FM2OP mod](release/digisophie-2.0.0.elemod),
[elekloader](https://github.com/irpina/elekloader/releases/latest), and your
own original Digitakt Mk1 OS 1.53 `.syx`
The `.elemod` contains this project's code, **not** Elektron's firmware.
You do not need ColdFire tools, Python, or a source checkout to install it.

1. Open elekloader and select your stock OS using **Change stock firmware**.
2. Choose **Install from file** and select `digisophie-2.0.0.elemod`.
   Enable FM2OP. elekloader's built-in **core 2.1** should enable with it.
   If your elekloader has an older core or shows a dependency error, update
   elekloader before building.
3. Optional: install and enable the bundled
   [digihealth diagnostic](release/digihealth-1.0.1.elemod) too. This is
   the configuration used for the earlier S027 hardware test. It adds SYSTEM INFO
   and an opt-in FAST AUDIO setting; without it FM2OP still works.
4. Wait for elekloader's **Ready to build** check, set the four-character
   OS version to `S034`, then choose **Build Firmware**. Save the generated
   `.syx` on your computer.
5. Send that `.syx` to the Digitakt with Elektron Transfer

Elekloader builds and verifies the OS; **Elektron Transfer does the actual
upload to the instrument**. Do not power off during the update. Neither
the stock nor modified OS file belongs in this repository.

### Build the mods from source (developers only)

ColdFire tools are needed only to change or recompile the mod. You need
Python 3.9+, a source checkout of
[elekloader](https://github.com/irpina/elekloader), a ColdFire cross-toolchain
(`m68k-linux-gnu-` or `m68k-elf-` assembler, GCC and linker), and your own
stock OS 1.53 file. From this repository:

```sh
ELEKLOADER_CROSS=m68k-elf- sh scripts/build.sh \
  /path/to/your/Digitakt_OS1.53.syx /path/to/elekloader
```

This builds core 2.1, FM2OP and the optional diagnostic from source,
lints the combination, and writes the verified custom OS to
`out/Digitakt_OS1.53_FM2OP_S034.syx`. To test DSP alone, run `make test`;
`make cross-check` additionally compiles for ColdFire. Optional emulator
probes in `tests/` require [digiemu](https://github.com/irpina/digiemu).

## Use and recovery

This changes firmware on the instrument. Back up projects and sounds first,
check that your stock OS is OS 1.53 for the *original* Digitakt, and keep
that stock file for recovery. If you install the optional diagnostic,
FAST AUDIO is off by default; SYSTEM INFO and FAST AUDIO can be enabled
separately in SETTINGS. See [diagnostics](diagnostics/README.md) for the monitor.

FM2OP is independent of, and not endorsed by, Elektron. It contains no
Elektron firmware or samples.

## Source and licenses

The FM2OP implementation is [MIT licensed](LICENSE). Third-party attribution
and the separate GPL-2.0-or-later digihealth source are
documented in [THIRD_PARTY.md](THIRD_PARTY.md). The digihealth license is
also included in its source directory.

The [DSP audit](SOPHIE_DSP_AUDIT.md),
[hardware performance notes](SOPHIE_HARDWARE_LAG_AUDIT.md),
[SPICE architecture idea](SPICE_ARCHITECTURE.md) and
[oscilloscope feasibility note](OSCILLOSCOPE_FEASIBILITY.md) record the
development history; the latter two are ideas, not implemented features.
