:: 명령어	옵션	원본 위치									사본 저장 위치

xcopy		/y		.\Engine\Default\epch.h						.\Reference\Header\
xcopy		/y		.\Engine\Header\*.h							.\Reference\Header\
xcopy		/y		.\Engine\Header\*.hpp						.\Reference\Header\

xcopy		/y		.\Engine\Bin\Engine.dll						.\Client\Bin\
xcopy		/y		.\Engine\Bin\Engine.lib						.\Reference\Library\
xcopy		/y		.\Engine\EngineResources\Icon\*.ico			.\Client\EngineResources\Icon\
xcopy		/y		.\Engine\EngineResources\Shader\*.hlsl		.\Client\EngineResources\Shader\
xcopy		/y		.\Engine\EngineResources\Image\*.*			.\Client\EngineResources\Image\