$ErrorActionPreference = 'Stop'
$repo = Resolve-Path (Join-Path $PSScriptRoot '../..')
Push-Location $repo
try {
    $compiler = (Get-Command g++.exe).Source
    New-Item -ItemType Directory -Force .pio | Out-Null
    foreach ($test in @('load_cell_sync', 'stall_recovery', 'run_supervisor', 'tmc_uart_frame', 'hall_tracker', 'hall_sensor', 'commissioning_fit', 'resume_profile')) {
        $sources = @("tests/native/${test}_test.cpp")
        if ($test -eq 'run_supervisor') { $sources += 'src/run_supervisor.cpp' }
        if ($test -eq 'hall_sensor') { $sources += 'src/hall_sensor.cpp' }
        & $compiler -std=c++17 -Wall -Wextra -Werror -I tests/native -I include @sources -o ".pio/${test}_test.exe"
        if ($LASTEXITCODE -ne 0) { throw "$test compilation failed" }
        & ".pio/${test}_test.exe"
        if ($LASTEXITCODE -ne 0) { throw "$test failed" }
    }
    & $compiler -std=c++17 -Wall -Wextra -Werror -Wno-class-memaccess -I tests/native/load_cell_support -I include tests/native/load_cells_test.cpp src/load_cells.cpp -o .pio/load_cells_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Load-cell module compilation failed' }
    foreach ($mask in @(0,1,3,15)) {
        & .pio/load_cells_test.exe $mask
        if ($LASTEXITCODE -ne 0) { throw "Load-cell module test failed for mask $mask" }
    }
    & $compiler -std=c++17 -Wall -Wextra -Werror -I tests/native/wifi_support -I include tests/native/firmware_update_test.cpp src/firmware_update.cpp -o .pio/firmware_update_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi updater compilation failed' }
    & .pio/firmware_update_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Wi-Fi updater tests failed' }
    & $compiler -std=c++17 -Wall -Wextra -Werror -I tests/native/settings_support -I tests/native/wifi_support -I include tests/native/settings_test.cpp src/settings.cpp -o .pio/settings_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Settings compilation failed' }
    & .pio/settings_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Settings tests failed' }
    & $compiler -std=c++17 -Wall -Wextra -Werror -I tests/native/driver_support -I include tests/native/tmc_driver_poll_test.cpp src/tmc_driver.cpp -o .pio/tmc_driver_poll_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'UART polling compilation failed' }
    & .pio/tmc_driver_poll_test.exe
    if ($LASTEXITCODE -ne 0) { throw 'UART polling tests failed' }
} finally { Pop-Location }
