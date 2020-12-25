
workspace("Thoth")
	-- Configuration.
	configurations({ "Release", "Dev"})
	location("build")
	targetdir ("build/%{prj.name}/%{cfg.longname}")
	debugdir ("build/%{prj.name}/%{cfg.longname}")
	architecture("x64")

	-- Configuration specific settings.
	filter("configurations:Release")
		defines({ "NDEBUG" })
		optimize("On")

	filter("configurations:Dev")
		defines({ "DEBUG" })
		symbols("On")

	filter({})
	startproject("Thoth")

	project("Thoth")
		kind("ConsoleApp")

		language("C++")
		cppdialect("C++17")
		systemversion("latest")
		-- Compiler flags
		filter("toolset:not msc*")
			buildoptions({ "-Wall", "-Wextra" })
		filter("toolset:msc*")
			buildoptions({ "-W3"})
		filter({})
		-- Common include dirs
		-- System headers are used to support angled brackets in Xcode.
		includedirs({"src/"})
		sysincludedirs({ "libs/" })

		-- common files
		files({"src/**", "libs/**", "premake5.lua"})
		removefiles({"**.DS_STORE", "**.thumbs"})
		
		filter("system:macosx")
			sysincludedirs({ "libs/libssh/macos/include" })
			libdirs({"libs/libssh/macos/lib/"})
			links({"ssh", "ssl", "z", "crypto", "Security.framework"})
		filter("system:windows")
			sysincludedirs({ "libs/libssh/win/include" })
			libdirs({"libs/libssh/win/lib/"})
			links({"ssh", "mbedcrypto", "mbedtls", "mbedx509", "wsock32", "ws2_32", "pthreadVC3"})
			
		-- visual studio filters
		filter("action:vs*")
			defines({ "_CRT_SECURE_NO_WARNINGS" })  
		filter({})

newaction {
   trigger     = "clean",
   description = "Clean the build directory",
   execute     = function ()
      print("Cleaning...")
      os.rmdir("./build")
      print("Done.")
   end
}
