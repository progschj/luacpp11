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

// adds the index metamethod to a type T
template<class T>
void add_index_metamethod(lua_State *L)
{
    luacpp11::getmetatable<T>(L);
    lua_pushstring(L, "__index");
    luacpp11::push_callable(L, index_metamethod);
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

// setup index metamethods for all "variants" of type T
template<class T>
void register_class(lua_State *L)
{
    add_index_metamethod<T>(L);
    add_index_metamethod<const T>(L);
    add_index_metamethod<T*>(L);
    add_index_metamethod<const T*>(L);
    add_index_metamethod< std::shared_ptr<T> >(L);
    add_index_metamethod<std::shared_ptr<const T> >(L);
}

// add a function to the metatable of T
template<class T, class F>
void add_function_single(lua_State *L, const std::string &name, F &&f)
{
    luacpp11::getmetatable< T >(L);
    lua_pushstring(L, name.c_str());
    luacpp11::push_callable(L, std::forward<F>(f));
    lua_rawset(L, -3);
    lua_pop(L, 1);
}

// add non const functions only to the metatables of non const types
template<class T, class R, class... Args>
void add_function(lua_State *L, const std::string &name, R (T::*f)(Args...))
{
    add_function_single<T>(L, name, f);
    add_function_single<T*>(L, name, f);
    add_function_single< std::shared_ptr<T> >(L, name, f);
}

// add const functions to all metatables
template<class T, class R, class... Args>
void add_function(lua_State *L, const std::string &name, R (T::*f)(Args...) const)
{
    add_function_single<T>(L, name, f);
    add_function_single<T*>(L, name, f);
    add_function_single< std::shared_ptr<T> >(L, name, f);
    add_function_single<const T>(L, name, f);
    add_function_single<const T*>(L, name, f);
    add_function_single< std::shared_ptr<const T> >(L, name, f);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    register_class< std::vector<int> >(L);
    add_function< std::vector<int> >(L, "size", &std::vector<int>::size);
    add_function< std::vector<int> >(L, "resize", static_cast<void (std::vector<int>::*)(size_t)>(&std::vector<int>::resize));

    // lua controlled lifetime
    luacpp11::emplace< std::vector<int> >(L);
    lua_setglobal(L, "vec");

    // C++ controlled lifetime
    std::vector<int> vec;
    luacpp11::push(L,  &vec);
    lua_setglobal(L, "ptrvec");

    // shared lifetime
    auto sharedvec = std::make_shared< std::vector<int> >();
    luacpp11::push(L,  sharedvec);
    lua_setglobal(L, "sharedvec");

    // const vector (shouldn't have resize)
    const std::vector<int> vec2(42);
    luacpp11::push(L,  &vec2);
    lua_setglobal(L, "vec2");


    int result = luaL_dostring(L,
        "print(\"vector:\")\n"
        "print(vec:size())\n"
        "vec:resize(12)\n"
        "print(vec:size())\n"

        "print(\"pointer to vector:\")\n"
        "print(ptrvec:size())\n"
        "ptrvec:resize(24)\n"
        "print(ptrvec:size())\n"

        "print(\"shared_pointer to vector:\")\n"
        "print(sharedvec:size())\n"
        "sharedvec:resize(36)\n"
        "print(sharedvec:size())\n"

        "print(\"\\nconst vector:\")\n"
        "print(vec2:size())\n"
        "vec2:resize(12)\n" // should error since vec2 is const
        "print(vec2:size())\n"
    );
    if (result) {
        std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
    }

    lua_close(L);

    return 0;
}
