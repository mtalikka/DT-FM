# DT-FM for Digitakt

<img width="672" height="352" alt="m4f80cd2dd1a6" src="https://github.com/user-attachments/assets/3540c645-bca7-42d3-bc2d-7a29128d30d7" />

DT-FM is a four-operator FM synth machine for the original Digitakt
(Mk1), OS 1.53 and 1.54. It replaces the earlier Sophie-based voice with a dedicated
FM engine and keeps Digitakt's regular AMP, filter, mixer and effects path.
The source, not a modified Elektron OS, is what this repository distributes.

[![Build checks](https://github.com/mtalikka/DT-FM/actions/workflows/build.yml/badge.svg)](https://github.com/mtalikka/DT-FM/actions/workflows/build.yml)

## Changelog

- **1.2.1:**
  -  Added support for parameter locks on SRC page ([#1](https://github.com/mtalikka/DT-FM/issues/1))
  -  Fixed an issue with visual feedback of operator parameters ([#2](https://github.com/mtalikka/DT-FM/issues/2))
- **1.2.0:** Compatibility changes for cooperation with other Elekloader mods
- **1.1.0:** Added support for OS version 1.54
- **1.0.1:** Fixed operator values changing after a power cycle
- **1.0.0:** Initial release. Replaced the Sophie voice
  code with a dedicated 4-op FM machine

---

<strong><font color="red">USE AT YOUR OWN RISK.</font></strong> This modifies your instrument's firmware; back up your projects and sounds, and keep a stock OS file for recovery.

In hardware testing, four DT-FM tracks playing together used about 95% CPU
without FAST AUDIO and about 85% with it. Eight-track operation is **not**
claimed.

## AI Disclaimer

This fork of digisophie has been created with the assistance of LLM agents. I don't know how digisophie was made, but pretty much all commits after 961c39ce have been "vibe-coded". That is to say, a human provided the prompts, design decisions, and performed testing on the hardware, but all of the heavy lifting, DSP coding, assembly hacking, etc., was done by an AI. Heck, all but this disclaimer have been generated and/or modified by AI. I make no promises to proofread or improve anything, so use this modified firmware at entirely your own risk. I am aware of the ethical implications of using AI and fully deserve any criticism related to that. However, I already am basically forced to use AI at work, so my hands are already dirty, so to speak. I am not proud of outsourcing my thinking and learning to an LLM, but if it were not for these tools, stuff like this probably just wouldn't get made. My idea with releasing this is to minimize the damage caused by this foolish endeavour by sharing my results with others, so that they don't have to go and do the same thing.

I have the utmost respect for people that do low-level hacking and write audio code - you are probably some of the smartest people out there. I am not one of you. I'm just an idiot who wanted a digitone but couldn't afford one at the moment, and seeing the newly-released custom firmware made for the MK1, wondered if it might be possible to make an FM synth on the DT.

## Controls

| SRC knob | Control | What it does |
| --- | --- | --- |
| A | TUNE | Pitch |
| B | ALGO | Operator routing, 1-8; its knob draws the routing (see [Algorithms](#algorithms)) |
| C | RATIO | Selected operator's frequency ratio, 0.25 to 16 |
| D | OP | Which operator (1-4) RATIO, LEVEL, ATTK and DECAY edit; its knob shows the number |
| E | LEVEL | Selected operator's level: volume as a carrier, depth as a modulator |
| F | ATTK | Selected operator's envelope attack |
| G | DECAY | Selected operator's envelope decay; `INF` holds |
| H | CHAR | Operator 4 feedback and overall brightness |

Each sound keeps all four operators: the one OP shows in its normal
parameters, the other three in parameter slots the stock OS leaves unused.
Copying a sound, track or pattern brings all four, while pasting only the
SRC page brings just the one OP shows. They are saved with the project, in
a part of each pattern's stored kit that the stock OS neither writes nor
reads, and survive a power cycle like any unsaved edit. The sound pool
keeps only the operator OP shows: a pool sound's other three start at
defaults, as in a project saved without DT-FM. A project from the earlier
build that kept them per track gives every pattern its tracks' old values.
Every SRC control can be parameter-locked. A trig's RATIO, LEVEL, ATTK and
DECAY locks change the operator OP selects on that trig: lock OP as well
to pick it, or they change the operator OP shows when the trig plays. A
track switched to DT-FM starts with every operator at its default. The
normal AMP page controls the note envelope: set HOLD to `NOTE` for TRIG
LEN to determine when the release begins. Use a finite DEC to hear that
release. DEC `INF` can keep the sound going indefinitely. Retriggering
chokes the previous voice on that track, with a brief transition to
suppress a click.

## Algorithms

ALGO's knob draws the selected algorithm instead of a dial. Each square is
an operator, and modulation flows downwards: an operator modulates the one
it is joined to below it. The operators standing on the bar along the
bottom are the carriers, the ones you hear. The others are modulators,
whose LEVEL sets modulation depth rather than volume. A shared line joins
several modulators into one operator, as in algorithm 4, or spreads one
modulator to several, as in algorithm 6.

The screen does not number the squares. These diagrams show the same
layouts with operator numbers; `>` means "modulates":

```text
1  4>3>2>1     2  (3+4)>2>1   3  (2+4>3)>1   4  (2+3+4)>1

    4
    │
    3            3   4              4
    │            └─┬─┘              │
    2              2            2   3          2 3 4
    │              │            └─┬─┘          └─┼─┘
    1              1              1              1
  ═════          ═════          ═════          ═════

5  2>1, 4>3    6  4>(1,2,3)   7  1,2,4>3     8  1,2,3,4

  2   4            4                4
  │   │          ┌─┼─┐              │
  1   3          1 2 3          1 2 3          1 2 3 4
  ═════          ═════          ═════          ═══════
```

Operator 4 also has feedback, set by CHAR; the diagrams do not draw it.


## Install: no compiler required

You need only the prebuilt DT-FM mod for your OS, `dt-fm-1.2.1-os1.53.elemod`
or `dt-fm-1.2.1-os1.54.elemod` from the
[1.2.1 release](https://github.com/mtalikka/DT-FM/releases/tag/v1.2.1),
[elekloader](https://github.com/irpina/elekloader/releases/latest)
(0.4.0 or later for OS 1.54), and your own original Digitakt Mk1 OS 1.53
or 1.54 `.syx`. DT-FM is also in elekloader's shop, so on
[elekloader's web page](https://irpina.github.io/elekloader/) you can add it
from the library instead of downloading it.
The `.elemod` contains this project's code, **not** Elektron's firmware.
You do not need ColdFire tools, Python, or a source checkout to install it.

1. Open elekloader and select your stock OS using **Add its stock OS file**.
2. Choose **Add from the library** and add DT-FM, or choose **Add your own
   .elemod** and select the .elemod file for your OS. elekloader refuses the
   file made for the other OS.
   Enable DT-FM. elekloader's built-in **core 3.0** should enable with it.
   If your elekloader has an older core or shows a dependency error, update
   elekloader before building.
3. Optional but highly recommended: install and enable the bundled digihealth diagnostic for your OS too. This adds the FAST AUDIO feature available from the settings menu. Enabling this allows several DT-FM tracks to play simultaneously without freezing the Digitakt. 
4. Wait for elekloader's **Ready to build** check, then choose **Build Firmware**.
   Save the generated `.syx` on your computer.
6. Send that `.syx` to the Digitakt with Elektron Transfer

Elekloader builds and verifies the OS; **Elektron Transfer does the actual
upload to the instrument**. Do not power off during the update. Neither
the stock nor modified OS file belongs in this repository.

### Build the mods from source (developers only)

ColdFire tools are needed only to change or recompile the mod. You need
Python 3.9+, a source checkout of
[elekloader](https://github.com/irpina/elekloader), a ColdFire cross-toolchain
(`m68k-linux-gnu-` or `m68k-elf-` assembler, GCC and linker), and your own
stock OS 1.53 or 1.54 file. For 1.54, use elekloader 0.4.0 or later. Git
ignores `firmware/`, a convenient place for the stock file. From this
repository:

```sh
ELEKLOADER_CROSS=m68k-elf- sh scripts/build.sh \
  firmware/Digitakt_OS1.54.syx /path/to/elekloader
```

This builds core 3.0, machine-pages 1.1, DT-FM and the optional diagnostic
from source for the OS of the file you give it, lints the combination, and
writes the verified custom OS to `out/Digitakt_OS1.54_DT-FM_S034.syx`. Your
elekloader checkout must have core 3.0 and machine-pages 1.1; the compatible
source tree is tagged `kit-v0.5.0`. This is how to run 1.2.0 before an
elekloader release can. The mods are
named for the OS too: `out/dt-fm-1.2.0-os1.54.elemod` and
`diagnostics/digihealth/out/digihealth-1.0.1-os1.54.elemod` (`1.53` for a
1.53 file). To test DSP alone, run `make test`;
`make cross-check` additionally compiles for ColdFire. Optional emulator
probes in `tests/` require [digiemu](https://github.com/irpina/digiemu).

On Windows with Docker Desktop, the container provides the compiler and
elekloader checkout. Provide your own stock OS file; it is mounted read-only
and is not included in the image:

```powershell
.\scripts\docker-build.ps1 .\firmware\Digitakt_OS1.54.syx
```

The verified custom OS and `.elemod` files are written under `out/` and
`diagnostics/digihealth/out/` in this checkout. Docker must be running.

## Use and recovery

This changes firmware on the instrument. Back up projects and sounds first,
check that your stock OS is OS 1.53 or 1.54 for the *original* Digitakt, and keep
that stock file for recovery. If you install the optional diagnostic,
FAST AUDIO is off by default; SYSTEM INFO and FAST AUDIO can be enabled
separately in SETTINGS. See [diagnostics](diagnostics/README.md) for the monitor.

DT-FM is independent of, and not endorsed by, Elektron. It contains no
Elektron firmware or samples.

## Source layout

| Path | Contents |
| --- | --- |
| `src/` | The mod: `mod.json` (manifest and patch sites), `digitakt.c` (the machine: its page, render and operators), `glue.s` (the hooks that store the hidden operators), `sophie.c` and `sophie.h` (the FM engine; the names predate DT-FM) |
| `tests/` | The host DSP test `make test` runs, a waveform probe and emulator probes |
| `scripts/build.sh` | Builds, lints and patches everything for one stock OS |
| `diagnostics/` | The optional digihealth diagnostic's source and notes |
| `release/` | Prebuilt digihealth for each OS |
| `firmware/`, `out/` | Your stock OS files and the build output, both ignored by git |

The Digitakt addresses DT-FM uses are kept apart from the code that uses
them: `src/os153.inc` / `src/os154.inc` for `glue.s`, `src/os153.h` /
`src/os154.h` for `digitakt.c`. The OS 1.54 build defines `OS154`
(`src/mod.json`, under `ports`), which picks the 1.54 files and patch
sites. The bundled digihealth is split the same way.

## Source and licenses

The DT-FM implementation is [MIT licensed](LICENSE). Third-party attribution
and the separate GPL-2.0-or-later digihealth source are
documented in [THIRD_PARTY.md](THIRD_PARTY.md). The digihealth license is
also included in its source directory.

The [DSP reference](DSP.md) records the parts of the stock OS that DT-FM
uses and the renderer's design.
