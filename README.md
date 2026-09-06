# NFSMW Fair Busts

Get boxed in mid-pursuit, hit reset, and Most Wanted busts you before you've
even got control of the car back.

That isn't the cops out-driving you. The moment you press reset the game stacks
the deck: the busted meter fills four times faster, cops way off down the street
start counting toward it, and it stops caring how fast you're going. Nothing
drains the meter while that's true, so the bust is already decided — you just
sit there and watch it land.

This mod takes that away. A reset costs you your momentum and nothing else, and
then you're in a normal pursuit again.

![Boxed in by cops at heat 6 with the busted meter barely started](images/pursuit.jpg)

*Boxed in at heat 6 — while car is in reset*

## What you need

- Most Wanted (2005) on PC, **version 1.3** — the 6,029,312 byte `speed.exe`
- An ASI loader, usually `dinput8.dll` from [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)

A patched executable is fine. Large address aware, widescreen and no-CD builds
all work.

## Installing

Grab the zip from [Releases](../../releases), drop `NFSMWFairBusts.asi` and
`NFSMWFairBusts.ini` into your game's `scripts` folder, and launch.

Nothing to press, nothing to set up. It gets along with Unlimiter, Bartender,
ExtraOptions, Takedown N2O and the Widescreen Fix. To uninstall, delete the two
files.

## Settings

Open `NFSMWFairBusts.ini` in Notepad. Changes apply next launch.

**`ResetAutoBust`** — the instant bust itself. Leave it at `0` and a reset puts
you back under the normal rules, so outrunning the cops works again. `1` gives
you the stock behaviour back.

**`ResetBustMultiplier`** — how fast the meter climbs after a reset. `1.0` is
the same as any other bust; `4.0` is what the game does.

The two are separate on purpose. If a completely free reset feels too soft,
leave `ResetAutoBust` at `0` and set the multiplier to `2.0` — the meter fills
quickly, but you can still drive your way out of it.

**`Enabled`** — set to `0` to switch the mod off without removing it.

```
Mod injected and applied to v1.3 and C0516B485065FABDD69579816B5DF763
```

That hash is your own `speed.exe`, so don't worry if it doesn't match the one
above — a patched exe reports something different and that's normal. If the mod
didn't load, the line says why instead.

## Building

Only if you want to compile it yourself. Open `NFSMWFairBusts.sln` and build
Release|Win32, or run `build.bat` with a 32-bit MinGW-w64 g++. The CRT is linked
statically, so there's no redistributable to chase.

## License

MIT — see [LICENSE](LICENSE).
