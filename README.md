# DT-FM for Digitakt

DT-FM is a four-operator FM synth machine for the original Digitakt
(Mk1), OS 1.53. It replaces the earlier Sophie-based voice with a dedicated
FM engine and keeps Digitakt's regular AMP, filter, mixer and effects path.
The source, not a modified Elektron OS, is what this repository distributes.

## Changelog

- **1.0.0 (firmware S034):** First DT-FM release. Replaced the Sophie voice
  code with a dedicated 4-op FM machine (formerly FM2OP; its projects keep
  working). OP shows the selected operator's number, ALGO draws its routing,
  and the hidden operators survive a power cycle.

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

The sound saves only the operator OP shows; the other three are kept per
track and saved with the project, in a gap of the project's storage block
that the stock OS neither writes nor reads. They also survive a power cycle,
like any unsaved edit. They follow the track across
patterns, but not into the sound pool or saved kits, and a project saved
without DT-FM starts them at defaults. TUNE, ALGO and CHAR can be
parameter-locked; the per-operator controls cannot yet. The normal AMP page controls
the note envelope: set HOLD to `NOTE` for TRIG LEN to determine when the
release begins. Use a finite DEC to hear that release. DEC `INF` can keep
the sound going indefinitely. Retriggering chokes the previous voice on
that track, with a brief transition to suppress a click.

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

You need only the prebuilt
[DT-FM mod](release/dt-fm-1.0.0.elemod),
[elekloader](https://github.com/irpina/elekloader/releases/latest), and your
own original Digitakt Mk1 OS 1.53 `.syx`
The `.elemod` contains this project's code, **not** Elektron's firmware.
You do not need ColdFire tools, Python, or a source checkout to install it.

1. Open elekloader and select your stock OS using **Change stock firmware**.
2. Choose **Install from file** and select `dt-fm-1.0.0.elemod`.
   Enable DT-FM. elekloader's built-in **core 2.1** should enable with it.
   If elekloader still lists the earlier `digisophie` mod (FM2OP), disable
   it: both add machine 7.
   If your elekloader has an older core or shows a dependency error, update
   elekloader before building.
3. Optional: install and enable the bundled
   [digihealth diagnostic](release/digihealth-1.0.1.elemod) too. This is
   the configuration used for the earlier S027 hardware test. It adds SYSTEM INFO
   and an opt-in FAST AUDIO setting; without it DT-FM still works.
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

This builds core 2.1, DT-FM and the optional diagnostic from source,
lints the combination, and writes the verified custom OS to
`out/Digitakt_OS1.53_DT-FM_S034.syx`. To test DSP alone, run `make test`;
`make cross-check` additionally compiles for ColdFire. Optional emulator
probes in `tests/` require [digiemu](https://github.com/irpina/digiemu).

## Use and recovery

This changes firmware on the instrument. Back up projects and sounds first,
check that your stock OS is OS 1.53 for the *original* Digitakt, and keep
that stock file for recovery. If you install the optional diagnostic,
FAST AUDIO is off by default; SYSTEM INFO and FAST AUDIO can be enabled
separately in SETTINGS. See [diagnostics](diagnostics/README.md) for the monitor.

DT-FM is independent of, and not endorsed by, Elektron. It contains no
Elektron firmware or samples.

## Source and licenses

The DT-FM implementation is [MIT licensed](LICENSE). Third-party attribution
and the separate GPL-2.0-or-later digihealth source are
documented in [THIRD_PARTY.md](THIRD_PARTY.md). The digihealth license is
also included in its source directory.

The [DSP reference](DSP.md) records the parts of the stock OS that DT-FM
uses and the renderer's design.
