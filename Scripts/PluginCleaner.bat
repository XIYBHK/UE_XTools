@ECHO OFF
SETLOCAL

ECHO.
ECHO =================================================================
ECHO             UE Plugin Cleaner (Final Corrected Version)
ECHO =================================================================
ECHO.

REM --- Calculate size before cleanup ---
ECHO Calculating size before cleanup. This may take a moment...
SET sizeBefore=0
FOR /R . %%F IN (*) DO SET /A sizeBefore+=%%~zF

SET /A sizeBeforeMB=%sizeBefore% / 1024 / 1024
ECHO Total size before cleanup: %sizeBeforeMB% MB (%sizeBefore% bytes)
ECHO.

ECHO.
ECHO -----------------------------------------------------------------
ECHO WARNING:
ECHO This script will delete all .PDB files and the Intermediate folder.
ECHO -----------------------------------------------------------------
ECHO.

PAUSE
ECHO.

REM --- Perform deletion ---
ECHO Deleting all .pdb debug files...
del /s /f /q "%~dp0*.pdb" > nul
ECHO .pdb file deletion command executed.
ECHO.

ECHO Deleting Intermediate folder...
rmdir /s /q "%~dp0Intermediate" > nul
ECHO Intermediate folder deletion command executed.
ECHO.

REM --- Calculate size after cleanup ---
ECHO Calculating size after cleanup...
SET sizeAfter=0
FOR /R . %%F IN (*) DO SET /A sizeAfter+=%%~zF

SET /A sizeAfterMB=%sizeAfter% / 1024 / 1024
ECHO Total size after cleanup: %sizeAfterMB% MB (%sizeAfter% bytes)
ECHO.

REM --- Display results ---
SET /A savedMB=%sizeBeforeMB% - %sizeAfterMB%
ECHO =================================================================
ECHO Cleanup complete!
ECHO.
ECHO Before: %sizeBeforeMB% MB
ECHO After:  %sizeAfterMB% MB
ECHO.
ECHO Total space saved: %savedMB% MB
ECHO =================================================================
ECHO.

ENDLOCAL
PAUSE
