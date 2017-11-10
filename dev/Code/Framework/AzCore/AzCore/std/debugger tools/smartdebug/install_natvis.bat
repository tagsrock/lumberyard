@echo off
REM
REM  All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
REM  its licensors.
REM
REM  REM  For complete copyright and license terms please see the LICENSE at the root of this
REM  distribution (the "License"). All use of this software is governed by the License,
REM  or, if provided, by the license below or the license accompanying this file. Do not
REM  remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
REM  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM
REM

echo Installing Visual Studio Visualizers.

SET DOCUMENTS="%USERPROFILE%\Documents"
set FOUND=0

REM Set current folder
cd /d %~dp0

:INSTALL_VS14
SET FOLDER="%DOCUMENTS%\Visual Studio 2015"
IF EXIST %FOLDER% (
    echo     Visual Studio 2015
    SET VISUALIZERFOLDER="%FOLDER%\Visualizers"
    IF NOT EXIST "%VISUALIZERFOLDER%\NUL" (
        mkdir "%VISUALIZERFOLDER%"
    )
    copy AZCore.natvis "%VISUALIZERFOLDER%"
	IF NOT %ERRORLEVEL% == 0 (
		echo "Failed to find Visual Studio 2015 user folder."
	) ELSE (
		SET FOUND=1
	)
)

:INSTALL_VS12
SET FOLDER="%DOCUMENTS%\Visual Studio 2013"
IF EXIST %FOLDER% (
    echo     Visual Studio 2013
    SET VISUALIZERFOLDER="%FOLDER%\Visualizers"
    IF NOT EXIST "%VISUALIZERFOLDER%\NUL" (
        mkdir "%VISUALIZERFOLDER%"
    )
    copy AZCore.natvis "%VISUALIZERFOLDER%"
    IF NOT %ERRORLEVEL% == 0 (
		echo "Failed to find Visual Studio 2013 user folder."
	) ELSE (
		SET FOUND=1
	)
)

IF %FOUND% == 1 goto SUCCESS
echo Failed to find any Visual Studio user folder.

:SUCCESS
echo.
echo Done!
goto END

:FAILED
echo Failed to install visualization file

:END