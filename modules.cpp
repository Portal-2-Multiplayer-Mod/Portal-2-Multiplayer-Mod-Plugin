//===========================================================================//
//
// Author: NULLderef
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//

#include "modules.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#else
#endif
#include <filesystem>
#include <stdexcept>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace Memory {
	std::unordered_map<std::string, std::span<uint8_t>> Modules::loadedModules;

	void Modules::PopulateModules() {
#ifdef _WIN32
		HMODULE modules[1024];
		auto processHandle = GetCurrentProcess();
		DWORD modulesNeeded;
		if (EnumProcessModules(processHandle, modules, sizeof(modules), &modulesNeeded)) {
			for (DWORD i = 0; i < (modulesNeeded / sizeof(HMODULE)); i++) {
				char pathBuffer[MAX_PATH];
				if (!GetModuleFileNameA(modules[i], pathBuffer, sizeof(pathBuffer))) continue;
				MODULEINFO moduleInfo = {};
				if (!GetModuleInformation(processHandle, modules[i], &moduleInfo, sizeof(MODULEINFO))) continue;
				std::filesystem::path modulePath(pathBuffer);
				loadedModules.insert(
					std::make_pair(
						modulePath.stem().string(),
						std::span<uint8_t>(
							reinterpret_cast<uint8_t*>(moduleInfo.lpBaseOfDll),
							static_cast<size_t>(moduleInfo.SizeOfImage)
						)
					)
				);
			}
		}
#else
#endif
	}

	std::span<uint8_t> Modules::Get(std::string name) {
		if (loadedModules.empty()) {
			PopulateModules();
		}

		if (loadedModules.contains(name)) {
			return loadedModules[name];
		}
		else {
			throw std::runtime_error("Failed to Get a required module");
		}
	}
};
