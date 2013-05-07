#include <iostream>
#include <vector>

#include <lua.hpp>
#include <lualib.h>
#include <lauxlib.h>

#include "luacpp11.hpp"

// the index metamethod simply forwards to metatable entries
luacpp11::luareturn index_metamethod(lua_State *L)
{
    lua_getmetatable(L, -2);
    lua_pushvalue(L, -2);
    lua_rawget(L, -2);
    lua_remove(L, -2);
    return 1;
}

// adds the index metamethod to the table at the top of the stack
void add_index_metamethod(lua_State *L)
{
    lua_pushstring(L, "__index");
    luacpp11::push_callable(L, index_metamethod);
    lua_rawset(L, -3);
}

// add a function to the metatable at the top of the stack
template<class F>
void add_function(lua_State *L, const std::string &name, F &&f)
{
    lua_pushstring(L, name.c_str());
    luacpp11::push_callable(L, std::forward<F>(f));
    lua_rawset(L, -3);
}

// setup registration hook. on_register will be called after creation of the
// metatable and placing it on top of the stack. Registration for std::vector<T>*
// shared_ptr< std::vector<T> > and the const version has to be done separately
// with additional register_hook specalizations.
namespace luacpp11 {
    template<class T>
    struct register_hook< std::vector<T> > {
        static void on_register(lua_State *L)
        {
            add_index_metamethod(L);
            add_function(L, "size", &std::vector<T>::size);
            add_function(L, "resize", static_cast<void (std::vector<T>::*)(size_t)>(&std::vector<T>::resize));
        }
    };
    template<class T>
    struct register_hook< const std::vector<T> > {
        static void on_register(lua_State *L)
        {
            add_index_metamethod(L);
            add_function(L, "size", &std::vector<T>::size);
        }
    };
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    luacpp11::emplace< std::vector<int> >(L);
    lua_setglobal(L, "vec");

    int result = luaL_dostring(L,
        "print(vec:size())\n"
        "vec:resize(12)\n"
        "print(vec:size())\n"
    );
    if (result) {
        std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
    }

    lua_close(L);

    return 0;
}
