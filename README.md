<h1 align="center">MouseFix</h1>

<p align="center">
  Your mouse isn't broken. The switch is.
</p>

<p align="center">
  <a href="https://apps.microsoft.com/detail/9PD5WFXK17TT">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="assets/store-badge-en-light.svg">
      <img src="assets/store-badge-en-dark.svg" alt="Get MouseFix from Microsoft Store" height="56">
    </picture>
  </a>
</p>

<p align="center"><a href="README.zh-CN.md">中文说明 →</a></p>

<img src="assets/hero.jpg" alt="MouseFix" width="100%">

## Signs of a worn mouse

- Click a file name once, and it jumps into rename mode.

- Scroll down, and the page springs back up.

- Drag a file or select some text, and halfway through, the button lets go.

These usually come down to one cause. After a few years, the contacts in a switch or wheel encoder oxidize, and a single press starts producing several rapid contact bounces. Windows reads each one as a separate click.

A new mouse fixes it for good, but it costs money. If yours is otherwise fine, MouseFix keeps it working: nothing to disassemble, nothing to solder.

## How it works

MouseFix intercepts mouse input before Windows sees it. The extra chatter gets dropped; real clicks go through untouched.

There's nothing to set up after install. It sits in the system tray and stays out of your way.

Everyday clicks feel instant.

Drag protection is the one exception. When you release a drag, it can take up to 150 ms to register, and that delay is how MouseFix tells a drag from a bounce. It's on for the left button by default.

## Install

Requires Windows 10 or later, 64-bit (x64 or ARM64).

| | [Microsoft Store](https://apps.microsoft.com/detail/9PD5WFXK17TT) | Build from source |
| :--- | :--- | :--- |
| Price | $2.99, trial available | Free |
| Signing | Microsoft-signed, so SmartScreen stays quiet | Unsigned; SmartScreen may warn |
| Install / remove | Handled by Windows | Drop it in a folder, delete it when you're done |
| Updates | Automatic | Rebuild for each release |
| Startup | On by default; turn it off in `Settings > Apps > Startup` | Not registered; run `add_to_startup.ps1` once |
| Best for | Installing once and forgetting about it | Reading the code, running your own build |

Both columns come from the same codebase, and the Store build is compiled from this repository. Currently 1.0.9; see [CHANGELOG.md](CHANGELOG.md) for older changes.

<details>
<summary>Build from source</summary>

1. Open `MouseFix/MouseFix.sln` in Visual Studio 2022.
2. Choose `Release` and build `x64`, `x86`, or `ARM64`. `build_release.bat` builds all three at once.
3. Drop `add_to_startup.ps1` next to the exe and run it once.

Settings live in `HKCU\Software\MouseFix`.

</details>

There's also a legacy build: v1.0.4 on the [Releases](https://github.com/matreshka15/MouseFix/releases) page, a standalone exe you download and run.

## Thresholds and presets

| Preset | Buttons | Wheel |
| :--- | ---: | ---: |
| Default | 50 ms | 30 ms |
| Office | 60 ms | 35 ms |
| Strict | 40 ms | 20 ms |

Every mouse ages differently, so left, right, middle, X1, X2, and the wheel each get their own threshold, adjustable from 1 to 200 ms.

Since 1.0.9, Smart Drag protection can be switched on or off per button. By default it's on for the left button only.

<img src="assets/presets-en.jpg" alt="MouseFix threshold presets" width="100%">

Start with one of the three presets. If the double-clicks stop, you're done. Raise the value and filtering gets stricter, though a deliberate fast double-click may get swallowed. Lower it and clicks stay snappy, but some chatter slips through. Fine-tune from there if you need to.

## Supporting MouseFix

If MouseFix got you a few more years out of your mouse, a copy from the Store is the simplest way to say thanks ❤️

The source stays open regardless, and you're welcome to build it yourself.

## Privacy

MouseFix doesn't connect to the network, write logs, or collect telemetry. Everything stays on your machine. Release builds leave the logging code out of the binary entirely.

It only reads which button you press, where the pointer is, and which way the wheel turns. It never records what you click or what you type.

## License

MIT License. Originally by [marvinlehmann](https://github.com/marvinlehmann), now maintained by me, [matreshka15](https://github.com/matreshka15).

If something misbehaves, open an [issue](https://github.com/matreshka15/MouseFix/issues). The mouse model, Windows version and threshold you used make it reproducible.
