@REM project file name
set PROJECT_NAME=%1
@REM build configuration
set BUILD_CONFIG=%2

ECHO "BUIlD project:        %PROJECT_NAME%"
ECHO "BUILD Configuration:  %BUILD_CONFIG%"

iarBuild.exe %PROJECT_NAME% -build %BUILD_CONFIG% -log warnings