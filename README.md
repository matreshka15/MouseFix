# MouseFix

MouseFix is a Windows utility that filters repeated mouse input caused by switch bounce. It supports the left, right, middle, X1 and X2 buttons and the mouse wheel, with drag protection for interrupted drags.

## Download and use

1. Download the appropriate executable from the [original project's releases](https://github.com/matreshka15/MouseFix/releases): `MouseFix-x64.exe` for 64-bit Windows or `MouseFix-x86.exe` for 32-bit Windows.
2. Run the executable and find the MouseFix icon in the system tray.
3. Right-click the icon to enable or disable filtering for individual inputs, change thresholds, select a preset, or exit.

Release binaries come from the original project and may differ from this checkout.

### Presets

| Preset | Buttons | Wheel |
| --- | ---: | ---: |
| Default | 50 ms | 30 ms |
| Office | 60 ms | 35 ms |
| Strict | 40 ms | 20 ms |

### Start at login

The repository includes `add_to_startup.ps1`, which can create a startup-folder shortcut or a current-user registry entry, and can remove those entries. Run it in PowerShell and follow its prompts. Its automatic search looks for `MouseFix.exe`; for an architecture-named executable, enter its full path when prompted and confirm the different filename. If it finds another `MouseFix.exe`, it uses that file automatically.

## Build from source

Open `MouseFix/MouseFix.sln` in Visual Studio with the C++ desktop build tools, the v143 toolset, and Windows SDK 10.0.22621.0 installed. Select Debug or Release and Win32 or x64, then build the solution. The project writes executables under `MouseFix/bin/x86/<configuration>/` or `MouseFix/bin/x64/<configuration>/`.

## License and credits

MouseFix is licensed under the [MIT License](LICENSE), copyright © 2017 Marvin Lehmann. Original concept by [marvinlehmann](https://github.com/marvinlehmann), improved by [matreshka15](https://github.com/matreshka15). This repository is a fork of [matreshka15/MouseFix](https://github.com/matreshka15/MouseFix).
