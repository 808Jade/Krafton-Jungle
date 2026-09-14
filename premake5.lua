workspace "EngineProject"
	architecture "x64"
	startproject "EngineProject"
 
	configurations
	{
		"Debug",
		"Release",
	}
 
	multiprocessorcompile "On"
 
outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
 
IncludeDir = {}
IncludeDir["ImGui"] = "EngineProject/ThirdParty/ImGui"
IncludeDir["stb"]   = "EngineProject/ThirdParty/stb"
IncludeDir["json"]  = "EngineProject/ThirdParty/json"
 
project "EngineProject"
	location "EngineProject"
	kind "WindowedApp"         
	language "C++"
	cppdialect "C++20"
	staticruntime "off"
	characterset "Unicode"
 
	targetdir ("Build/Bin/" .. outputdir)
	objdir    ("Build/Intermediate/" .. outputdir)
	debugdir  ("%{prj.location}")  
 
	pchheader "EnginePCH.h"
	pchsource "%{prj.name}/Src/EnginePCH.cpp"
 
	files
	{

		"%{prj.name}/Src/**.h",
		"%{prj.name}/Src/**.hpp",
		"%{prj.name}/Src/**.cpp",
 
		"%{prj.name}/ThirdParty/ImGui/*.h",
		"%{prj.name}/ThirdParty/ImGui/*.cpp",
		"%{prj.name}/ThirdParty/ImGui/backends/imgui_impl_win32.*",
		"%{prj.name}/ThirdParty/ImGui/backends/imgui_impl_dx11.*",
 
		"%{prj.name}/ThirdParty/stb/**.h",
		"%{prj.name}/ThirdParty/json/**.hpp",
	}
 
	includedirs
	{
		"%{prj.name}/Src",
		"%{IncludeDir.ImGui}",
		"%{IncludeDir.ImGui}/backends",
		"%{IncludeDir.stb}",
		"%{IncludeDir.json}",
	}
 
	links
	{
		"d3d11.lib",
		"dxgi.lib",
		"dxguid.lib",
		"d3dcompiler.lib",
	}
 
	defines
	{
		"WIN32_LEAN_AND_MEAN",
		"NOMINMAX",
		"UNICODE",
		"_UNICODE",
	}
 
	filter "files:EngineProject/ThirdParty/**.cpp"
		enablepch "Off"
 
	filter "files:**.hlsl"
		excludefrombuild "On"
 
	filter "system:windows"
		systemversion "latest"
		buildoptions { "/utf-8" }
		defines { "ENGINE_PLATFORM_WINDOWS" }
 
	filter "configurations:Debug"
		defines { "ENGINE_DEBUG", "_DEBUG" }
		runtime "Debug"
		symbols "on"
 
	filter "configurations:Release"
		defines { "ENGINE_RELEASE", "NDEBUG" }
		runtime "Release"
		optimize "on"
		symbols "on"  