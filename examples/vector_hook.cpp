#include <iostream>
#include <vector>

#include <lua.hpp>
#include <lualib.h>
#include <lauxlib.h>

#include "luacpp11.hpp"

template<class T>
luacpp11::luareturn vector_index_metamethod(lua_State *L)
{
    if(lua_isnumber(L, -1))
    {
        int index = lua_tointeger(L, -1);
        T &vec = luacpp11::tounchecked<T>(L, -2);
        if(index<0 || index >= vec.size())
        {
            lua_pushfstring(L, "index out of bounds.");
            lua_error(L);
        }
        luacpp11::push(L, vec[index]);
    }
    else
    {
        lua_getmetatable(L, -2);
        lua_pushvalue(L, -2);
        lua_rawget(L, -2);
        lua_remove(L, -2);
    }
    return 1;
}

template<class T>
luacpp11::luareturn vector_newindex_metamethod(lua_State *L)
{
    if(lua_isnumber(L, -2))
    {
        int index = lua_tointeger(L, -2);
        T &vec = luacpp11::tounchecked<T>(L, -3);
        if(index<0 || index >= vec.size())
        {
            lua_pushfstring(L, "index out of bounds.");
            lua_error(L);
        }
        vec[index] = luacpp11::to<typename T::value_type>(L, -1);
    }
    else
    {
        lua_pushfstring(L, "expected numerical index.");
        lua_error(L);
    }
    return 0;
}

template<class T>
luacpp11::luareturn vectorptr_index_metamethod(lua_State *L)
{
    if(lua_isnumber(L, -1))
    {
        int index = lua_tointeger(L, -1);
        auto &vec = *luacpp11::tounchecked<T>(L, -2);
        if(index<0 || index >= vec.size())
        {
            lua_pushfstring(L, "index out of bounds.");
            lua_error(L);
        }
        luacpp11::push(L, vec[index]);
    }
    else
    {
        lua_getmetatable(L, -2);
        lua_pushvalue(L, -2);
        lua_rawget(L, -2);
        lua_remove(L, -2);
    }
    return 1;
}

template<class T>
luacpp11::luareturn vectorptr_newindex_metamethod(lua_State *L)
{
    if(lua_isnumber(L, -2))
    {
        int index = lua_tointeger(L, -2);
        auto &vec = *luacpp11::tounchecked<T>(L, -3);
        if(index<0 || index >= vec.size())
        {
            lua_pushfstring(L, "index out of bounds.");
            lua_error(L);
        }
        vec[index] = luacpp11::to<decltype(vec[index])>(L, -1);
    }
    else
    {
        lua_pushfstring(L, "expected numerical index.");
        lua_error(L);
    }
    return 0;
}

namespace luacpp11 {
    template<class T>
    struct register_hook< std::vector<T> > {
        static void on_register(lua_State *L)
        {
            lua_pushstring(L, "__len");
            luacpp11::push_callable<size_t(std::vector<T>*, lua_State*)>(L,
                [](std::vector<T> *v, lua_State*){ return v->size(); }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__index");
            luacpp11::push_callable(L, vector_index_metamethod< std::vector<T> >);
            lua_rawset(L, -3);

            lua_pushstring(L, "__newindex");
            luacpp11::push_callable(L, vector_newindex_metamethod< std::vector<T> >);
            lua_rawset(L, -3);

            lua_pushstring(L, "clear");
            luacpp11::push_callable(L, &std::vector<T>::clear);
            lua_rawset(L, -3);
        }
    };
    template<class T>
    struct register_hook< const std::vector<T> > {
        static void on_register(lua_State *L)
        {
            lua_pushstring(L, "__len");
            luacpp11::push_callable<size_t(const std::vector<T>*, lua_State*)>(L,
                [](const std::vector<T> *v, lua_State*){ return v->size(); }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__index");
            luacpp11::push_callable(L, vector_index_metamethod< const std::vector<T> >);
            lua_rawset(L, -3);
        }
    };
    
    template<class T>
    struct register_hook< std::vector<T>* > {
        static void on_register(lua_State *L)
        {
            lua_pushstring(L, "__len");
            luacpp11::push_callable<size_t(std::vector<T>*, lua_State*)>(L,
                [](std::vector<T> *v, lua_State*){ return v->size(); }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__index");
            luacpp11::push_callable(L, vectorptr_index_metamethod< std::vector<T>* >);
            lua_rawset(L, -3);

            lua_pushstring(L, "__newindex");
            luacpp11::push_callable(L, vectorptr_newindex_metamethod< std::vector<T>* >);
            lua_rawset(L, -3);

            lua_pushstring(L, "clear");
            luacpp11::push_callable(L, &std::vector<T>::clear);
            lua_rawset(L, -3);
            
            lua_pushstring(L, "erase");
            luacpp11::push_callable<void(std::vector<T>*, int)>(L, 
                [](std::vector<T> *v, int index)
                {
                    v->erase(v->begin()+index);
                }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "insert");
            luacpp11::push_callable<void(std::vector<T>*, int, const T&)>(L, 
                [](std::vector<T> *v, int index, const T& e)
                {
                    v->insert(v->begin()+index, e);
                }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "push_back");
            luacpp11::push_callable<void(std::vector<T>*, const T&)>(L, 
                [](std::vector<T> *v, const T& e)
                {
                    v->push_back(e);
                }
            );
            lua_rawset(L, -3);
        }
    };
    template<class T>
    struct register_hook< const std::vector<T>* > {
        static void on_register(lua_State *L)
        {
            lua_pushstring(L, "__len");
            luacpp11::push_callable<size_t(const std::vector<T>*, lua_State*)>(L,
                [](const std::vector<T> *v, lua_State*){ return v->size(); }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__index");
            luacpp11::push_callable(L, vectorptr_index_metamethod< const std::vector<T>* >);
            lua_rawset(L, -3);
        }
    };
    
    template<class T>
    struct register_hook< std::shared_ptr< std::vector<T> > > {
        static void on_register(lua_State *L)
        {
            lua_pushstring(L, "__len");
            luacpp11::push_callable<size_t(std::vector<T>*, lua_State*)>(L,
                [](std::vector<T> *v, lua_State*){ return v->size(); }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__index");
            luacpp11::push_callable(L, vectorptr_index_metamethod< std::shared_ptr< std::vector<T> > >);
            lua_rawset(L, -3);

            lua_pushstring(L, "__newindex");
            luacpp11::push_callable(L, vectorptr_newindex_metamethod< std::shared_ptr< std::vector<T> > >);
            lua_rawset(L, -3);

            lua_pushstring(L, "clear");
            luacpp11::push_callable(L, &std::vector<T>::clear);
            lua_rawset(L, -3);

            lua_pushstring(L, "erase");
            luacpp11::push_callable<void(std::vector<T>*, int)>(L, 
                [](std::vector<T> *v, int index)
                {
                    v->erase(v->begin()+index);
                }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "insert");
            luacpp11::push_callable<void(std::vector<T>*, int, const T&)>(L, 
                [](std::vector<T> *v, int index, const T& e)
                {
                    v->insert(v->begin()+index, e);
                }
            );
            lua_rawset(L, -3);

            lua_pushstring(L, "push_back");
            luacpp11::push_callable<void(std::vector<T>*, const T&)>(L, 
                [](std::vector<T> *v, const T& e)
                {
                    v->push_back(e);
                }
            );
            lua_rawset(L, -3);
        }
    };
    template<class T>
    struct register_hook< std::shared_ptr< const std::vector<T> > > {
        static void on_register(lua_State *L)
        {
            lua_pushstring(L, "__len");
            luacpp11::push_callable<size_t(const std::vector<T>*, lua_State*)>(L,
                [](const std::vector<T> *v, lua_State*){ return v->size(); }
            );
            lua_rawset(L, -3);
            
            lua_pushstring(L, "__index");
            luacpp11::push_callable(L, vectorptr_index_metamethod< std::shared_ptr< const std::vector<T> > >);
            lua_rawset(L, -3);
        }
    };
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    luacpp11::emplace< std::vector<int> >(L, 10, 23);
    lua_setglobal(L, "vec");

    std::vector<int> v(10, 42);
    luacpp11::push(L, &v);
    lua_setglobal(L, "vec2");

    luacpp11::push(L, std::make_shared< std::vector<int> >(10, 63));
    lua_setglobal(L, "vec3");

    int result = luaL_dostring(L,
        "print(#vec)\n"
        "vec[4] = 24;\n"
        "print(vec[3])\n"
        "print(vec[4])\n"

        "print(#vec2)\n"
        "vec2[4] = 43;\n"
        "print(vec2[3])\n"
        "print(vec2[4])\n"

        "print(#vec3)\n"
        "vec3[3] = 64;\n"
        "print(vec3[3])\n"
        "print(vec3[4])\n"
    
        "vec3:erase(2)\n"
        "print(#vec3)\n"
        "vec3:insert(3, 3)\n"
        "print(vec3[3])\n"
        "print(#vec3)\n"
    );
    if (result) {
        std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
    }

    lua_close(L);

    return 0;
}
