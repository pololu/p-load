"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild" /t:build /p:Configuration=Release /p:TreatWarningsAsErrors=True ..\p-load.vcxproj
IF ERRORLEVEL 1 GOTO :EOF

copy ..\Release\p-load.exe .
IF ERRORLEVEL 1 GOTO :EOF

"C:\Program Files (x86)\Windows Kits\8.1\bin\x64\signtool" sign /v /n "Pololu Corporation" /tr http://tsa.starfieldtech.com p-load.exe
IF ERRORLEVEL 1 GOTO :EOF

"C:\Program Files (x86)\MSBuild\12.0\Bin\MSBuild" /t:build /p:Configuration=Release /p:TreatWarningsAsErrors=True p-load.wixproj

"C:\Program Files (x86)\Windows Kits\8.1\bin\x64\signtool" sign /v /n "Pololu Corporation" /tr http://tsa.starfieldtech.com bin/Release/en-us/p-load*.msi
IF ERRORLEVEL 1 GOTO :EOF
