set VK_LAYER_PATH=%VULKAN_SDK%\Source\lib
set PATH=%PATH%;%VULKAN_SDK%\Third-Party\Bin
set VK_INSTANCE_LAYERS=VK_LAYER_LUNARG_api_dump
set VK_APIDUMP_NO_ADDR=true
set VK_APIDUMP_LOG_FILENAME=C:\Wafflecat\spatial_progression\SpatialProgression_vs\TestCube\apidump_wite.txt
..\x64\Debug\TestCube.exe
set VK_APIDUMP_LOG_FILENAME=C:\Wafflecat\spatial_progression\SpatialProgression_vs\TestCube\apidump_baseline.txt
"C:\Users\sid\Documents\Visual Studio 2017\Projects\VS_Cube\x64\Debug\VS_Cube"
