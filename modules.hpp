//===========================================================================//
//
// Author: NULLderef
// Purpose: Portal 2: Multiplayer Mod server plugin
// 
//===========================================================================//

#ifndef MODULES_HPP
#define MODULES_HPP

#include <string>
#include <span>
#include <unordered_map>

namespace Memory {
    class Modules {
    public:
        static std::span<uint8_t> Get(std::string name);

    private:
        static void PopulateModules();
        static std::unordered_map<std::string, std::span<uint8_t>> loadedModules;
    };
};

#endif // MODULES_HPP
