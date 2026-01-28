workspace "BlackQuartz"
    architecture "x64"
    startproject "Test"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    
    configurations
    {
        "Debug",
        "Release"
    }    
    group "Core"
        include "Engine"
        include "Test"
        include "Uci"
    group ""

