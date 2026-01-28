project "test"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"
	staticruntime "on"

	targetdir ("%{wks.location}/build/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/build/obj/" .. outputdir .. "/%{prj.name}")

	files
	{
		"include/**.h",
		"source/**.cpp",
	}
	links
	{
		"engine"
	}
	includedirs
	{
		"include",
		"../engine/include",
		"../vendor/surge/include",
		"../vendor/doctest/include"
	}
	filter "system:windows"
		systemversion "latest"
		defines{ "PLATFORM_WINDOWS" }
		
	filter "system:linux"
		systemversion "latest"
		defines{ "PLATFORM_LINUX" }
	
	filter "configurations:Debug"
		defines "DEBUG"
		runtime "Debug"
		symbols "on"

	filter "configurations:Release"
		defines "NDEBUG"
		runtime "Release"
		optimize "on"