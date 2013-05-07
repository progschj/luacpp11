#include <iostream>
#include <vector>

#include <lua.hpp>
#include <lualib.h>
#include <lauxlib.h>

#include "luacpp11.hpp"

void hello()
{
    std::cout << "hello world" << std::endl;
}

struct Foo {
    int operator()(int x)
    {
        return x*x;
    }
};

// luareturn signifies that the function handles passing return values to lua
// itself. Pushing a CFunction with push_callable will be treated as a function
// returning an int! (just use lua_pushcfunction for that...)
// if the function is supposed to handle arguments itself there has to be
// exactly one lua_State* argument. If there are other arguments it will treat
// the lua_State just as a regular pointer argument.
luacpp11::luareturn bar(int, lua_State *L)
{
    std::cout << "bar called with " << lua_gettop(L) << " arguments" << std::endl;
    lua_pushinteger(L, 23);
    return 1;
}

// returning tuples results in multiple return values in lua
std::tuple<int, double> baz()
{
    return std::make_tuple(1, 3.14159);
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    lua_State *L = luaL_newstate();

    luaL_openlibs(L);

    // function type is be deduced if possible
    luacpp11::push_callable(L, hello);
    lua_setglobal(L, "hello");

    luacpp11::push_callable(L, &std::vector<int>::size);
    lua_setglobal(L, "size");

    luacpp11::push_callable(L, bar);
    lua_setglobal(L, "bar");

    luacpp11::push_callable(L, baz);
    lua_setglobal(L, "baz");

    Foo foo;

    // explicit function type since it cannot be deduced from the type Foo
    luacpp11::push_callable<int(int)>(L, foo);
    lua_setglobal(L, "foo");

    // it can in the case of std::function though
    luacpp11::push_callable(L, std::function<int(int)>(foo));
    lua_setglobal(L, "foo2");

    // construct a vector directly in userdata
    luacpp11::emplace< std::vector<int> >(L, 13);
    lua_setglobal(L, "vec");

    std::vector<int> v(4);

    // pushing by value will move or copy construct
    luacpp11::push(L, v);
    lua_setglobal(L, "valuevec");

    luacpp11::push(L, &v);
    lua_setglobal(L, "ptrvec");

    auto sharedptr = std::make_shared< std::vector<int> >(23);
    luacpp11::push(L, sharedptr);
    lua_setglobal(L, "sharedptrvec");


    int result = luaL_dostring(L,
        "hello()\n"
        "print(bar(1,2,3))\n"
        "print(baz())\n"
        "print(foo(11))\n"
        "print(foo2(12))\n"
        "print(size(vec))\n"
        "print(size(ptrvec))\n"
        "print(size(valuevec))\n"
        "print(size(sharedptrvec))\n"
    );
    if (result) {
        std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
    }

    lua_getglobal(L, "vec");

    // to<T> tries to convert between pointer and "value" types (returned by reference)
    // and will also return non const objects as const if requested
    luacpp11::to< std::vector<int> >(L, -1).push_back(42);
    luacpp11::to< std::vector<int>* >(L, -1)->push_back(42);
    luacpp11::to< const std::vector<int> >(L, -1).size();

    // toexact<T> only works on exact type matches
    luacpp11::toexact< std::vector<int> >(L, -1).push_back(42);

    // tounchecked<T> only works on exact type matches and doesn't check for
    // correctness
    luacpp11::tounchecked< std::vector<int> >(L, -1).push_back(42);


    // is<T> tests for the exact type though
    std::cout << std::boolalpha;
    std::cout << luacpp11::is< std::vector<int> >(L, -1) << ' ';
    std::cout << luacpp11::is< std::vector<int>* >(L, -1) << ' ';
    std::cout << luacpp11::is< const std::vector<int> >(L, -1) << std::endl;

    // isconvertible<T> tests if we can get the value by using to<T>
    std::cout << luacpp11::isconvertible< std::vector<int> >(L, -1) << ' ';
    std::cout << luacpp11::isconvertible< std::vector<int>* >(L, -1) << ' ';
    std::cout << luacpp11::isconvertible< const std::vector<int> >(L, -1) << std::endl;

    lua_pop(L, 1);

    result = luaL_dostring(L,
        "print(size(vec))\n"
    );
    if (result) {
        std::cerr << "Error: " << lua_tostring(L, -1) << std::endl;
    }

    // getmetatable pushes the metatable used for the userdata objects of type
    // T onto the stack
    luacpp11::getmetatable< std::vector<int> >(L);

    // luacpp11 only adds a __gc method to the metatable
    lua_getfield(L, -1, "__gc");
    if(lua_isfunction(L, -1))
        std::cout << "found __gc function" << std::endl;

    lua_pop(L, 2);


    lua_close(L);

    return 0;
}
