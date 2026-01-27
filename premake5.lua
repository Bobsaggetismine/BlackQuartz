workspace "bq_game_lib"
    architecture "x64"
    startproject "sandbox"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
    
    configurations
    {
        "Debug",
        "Release"
    }    
    group "Core"
        include "engine"
        include "test"
        include "uci"
    group ""

