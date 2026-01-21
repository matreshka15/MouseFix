# MouseFix Startup Configuration Script
# This script adds MouseFix to Windows startup

# Get script directory
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Optimized function to search for MouseFix.exe for end users
function Find-MouseFixExe {
    Write-Host "Searching for MouseFix.exe..." -ForegroundColor Yellow

    # Priority 1: Search in current script directory (most common for end users)
    $CurrentDirPaths = @(
        "$ScriptDir\MouseFix.exe",
        "$ScriptDir\MouseFix\MouseFix.exe"
    )

    foreach ($Path in $CurrentDirPaths) {
        if (Test-Path $Path) {
            Write-Host "Found: $Path" -ForegroundColor Green
            return $Path
        }
    }

    # Priority 2: Search in subdirectories (for users who extracted the archive)
    try {
        $ExeFiles = Get-ChildItem -Path $ScriptDir -Filter "MouseFix.exe" -Recurse -Depth 2 -ErrorAction SilentlyContinue | `
                    Select-Object -First 3
        
        foreach ($Exe in $ExeFiles) {
            Write-Host "Found: $($Exe.FullName)" -ForegroundColor Green
            return $Exe.FullName
        }
    } catch {
        # Ignore errors during recursive search
    }

    # Priority 3: Search in common user directories
    $UserDirs = @(
        "$env:USERPROFILE\Downloads",
        "$env:USERPROFILE\Desktop",
        "$env:USERPROFILE\Documents",
        "$env:USERPROFILE"
    )

    foreach ($UserDir in $UserDirs) {
        if (Test-Path $UserDir) {
            try {
                $ExeFiles = Get-ChildItem -Path $UserDir -Filter "MouseFix.exe" -Recurse -Depth 2 -ErrorAction SilentlyContinue | `
                            Select-Object -First 2
                
                foreach ($Exe in $ExeFiles) {
                    Write-Host "Found: $($Exe.FullName)" -ForegroundColor Green
                    return $Exe.FullName
                }
            } catch {
                # Ignore errors during recursive search
            }
        }
    }

    # Priority 4: Search in parent directories (for users running script from subfolder)
    try {
        $ParentDir = Split-Path -Parent $ScriptDir
        if ($ParentDir) {
            $ExeFiles = Get-ChildItem -Path $ParentDir -Filter "MouseFix.exe" -Recurse -Depth 1 -ErrorAction SilentlyContinue | `
                        Select-Object -First 1
            
            if ($ExeFiles) {
                Write-Host "Found: $($ExeFiles.FullName)" -ForegroundColor Green
                return $ExeFiles.FullName
            }
        }
    } catch {
        # Ignore errors during recursive search
    }

    return $null
}

# Search for MouseFix.exe
$MouseFixExe = Find-MouseFixExe

# If auto-search fails, prompt user for path
if (-not $MouseFixExe) {
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host "   MouseFix.exe not found" -ForegroundColor Red
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "The script searched the following locations:" -ForegroundColor Yellow
    Write-Host "  - Current script directory" -ForegroundColor Gray
    Write-Host "  - Subdirectories of current directory" -ForegroundColor Gray
    Write-Host "  - Downloads folder" -ForegroundColor Gray
    Write-Host "  - Desktop folder" -ForegroundColor Gray
    Write-Host "  - Documents folder" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Please make sure you have downloaded MouseFix from:" -ForegroundColor Cyan
    Write-Host "  https://github.com/matreshka15/MouseFix/releases" -ForegroundColor Cyan
    Write-Host ""

    # Prompt user for path
    Write-Host "Please enter the full path to MouseFix.exe (absolute path):" -ForegroundColor Cyan
    Write-Host "Tip: You can drag and drop the file here" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "Or enter 'q' to exit the script" -ForegroundColor DarkGray
    Write-Host ""

    $UserPath = Read-Host "Path"

    # Check if user wants to exit
    if ($UserPath -eq 'q' -or $UserPath -eq 'Q') {
        Write-Host ""
        Write-Host "Operation cancelled" -ForegroundColor Yellow
        pause
        exit 0
    }

    # Clean path (remove quotes)
    $UserPath = $UserPath.Trim('"', "'")

    # Validate path
    if (-not (Test-Path $UserPath)) {
        Write-Host ""
        Write-Host "Error: The specified path does not exist: $UserPath" -ForegroundColor Red
        pause
        exit 1
    }

    # Check if it's an .exe file
    if (-not $UserPath.EndsWith('.exe', [System.StringComparison]::OrdinalIgnoreCase)) {
        Write-Host ""
        Write-Host "Error: The specified file is not an .exe file" -ForegroundColor Red
        pause
        exit 1
    }

    # Check if filename is MouseFix.exe
    $FileName = Split-Path -Leaf $UserPath
    if ($FileName -ne 'MouseFix.exe') {
        Write-Host ""
        Write-Host "Warning: The filename is not MouseFix.exe, current filename: $FileName" -ForegroundColor Yellow
        Write-Host "Do you want to continue? (Y/N)" -ForegroundColor Yellow
        $Confirm = Read-Host

        if ($Confirm -ne 'Y' -and $Confirm -ne 'y') {
            Write-Host ""
            Write-Host "Operation cancelled" -ForegroundColor Yellow
            pause
            exit 0
        }
    }

    $MouseFixExe = $UserPath
    Write-Host ""
    Write-Host "Using user-specified path: $MouseFixExe" -ForegroundColor Green
}

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "   MouseFix Startup Configuration Tool" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "MouseFix Path: $MouseFixExe" -ForegroundColor Green
Write-Host ""

# Provide two startup methods
Write-Host "Please select a startup method:" -ForegroundColor Yellow
Write-Host "  1. Startup Folder (Recommended - User visible, easy to manage)" -ForegroundColor White
Write-Host "  2. Registry Startup (Hidden method, system-level startup)" -ForegroundColor White
Write-Host "  3. Remove Startup" -ForegroundColor White
Write-Host "  4. Exit" -ForegroundColor White
Write-Host ""

$Choice = Read-Host "Enter option (1-4)"

switch ($Choice) {
    "1" {
        # Method 1: Use startup folder
        Write-Host ""
        Write-Host "Configuring startup folder..." -ForegroundColor Yellow

        $StartupFolder = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup"
        $ShortcutPath = "$StartupFolder\MouseFix.lnk"

        # Create shortcut
        $WshShell = New-Object -ComObject WScript.Shell
        $Shortcut = $WshShell.CreateShortcut($ShortcutPath)
        $Shortcut.TargetPath = $MouseFixExe
        $Shortcut.WorkingDirectory = Split-Path -Parent $MouseFixExe
        $Shortcut.Description = "MouseFix - Mouse Debounce Tool"
        $Shortcut.Save()

        Write-Host "[OK] Successfully created startup shortcut" -ForegroundColor Green
        Write-Host "  Location: $ShortcutPath" -ForegroundColor Gray
        Write-Host ""
        Write-Host "MouseFix will start automatically on next login" -ForegroundColor Green
    }
    "2" {
        # Method 2: Use registry
        Write-Host ""
        Write-Host "Configuring registry startup..." -ForegroundColor Yellow

        $RegPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
        $RegName = "MouseFix"

        # Check if already exists
        $ExistingValue = Get-ItemProperty -Path $RegPath -Name $RegName -ErrorAction SilentlyContinue

        if ($ExistingValue) {
            Write-Host "Found existing startup entry, updating..." -ForegroundColor Yellow
        }

        # Add registry entry
        Set-ItemProperty -Path $RegPath -Name $RegName -Value $MouseFixExe -Force

        Write-Host "[OK] Successfully added registry startup entry" -ForegroundColor Green
        Write-Host "  Registry Path: $RegPath" -ForegroundColor Gray
        Write-Host "  Key Name: $RegName" -ForegroundColor Gray
        Write-Host "  Value: $MouseFixExe" -ForegroundColor Gray
        Write-Host ""
        Write-Host "MouseFix will start automatically on next login" -ForegroundColor Green
    }
    "3" {
        # Remove startup
        Write-Host ""
        Write-Host "Removing startup configuration..." -ForegroundColor Yellow

        $Removed = $false

        # Check startup folder
        $StartupFolder = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup"
        $ShortcutPath = "$StartupFolder\MouseFix.lnk"

        if (Test-Path $ShortcutPath) {
            Remove-Item $ShortcutPath -Force
            Write-Host "[OK] Removed startup folder shortcut" -ForegroundColor Green
            $Removed = $true
        }

        # Check registry
        $RegPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
        $RegName = "MouseFix"

        $ExistingValue = Get-ItemProperty -Path $RegPath -Name $RegName -ErrorAction SilentlyContinue

        if ($ExistingValue) {
            Remove-ItemProperty -Path $RegPath -Name $RegName -Force
            Write-Host "[OK] Removed registry startup entry" -ForegroundColor Green
            $Removed = $true
        }

        if ($Removed) {
            Write-Host ""
            Write-Host "Successfully removed MouseFix startup configuration" -ForegroundColor Green
        } else {
            Write-Host ""
            Write-Host "No MouseFix startup entry found" -ForegroundColor Yellow
        }
    }
    "4" {
        Write-Host ""
        Write-Host "Operation cancelled" -ForegroundColor Yellow
        exit 0
    }
    default {
        Write-Host ""
        Write-Host "Invalid option" -ForegroundColor Red
        pause
        exit 1
    }
}

Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host "Operation Complete!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tips:" -ForegroundColor Yellow
Write-Host "  - Press Win+R and type 'shell:startup' to open the startup folder" -ForegroundColor Gray
Write-Host "  - Press Ctrl+Shift+Esc to open Task Manager, switch to 'Startup' tab to view startup items" -ForegroundColor Gray
Write-Host ""

pause