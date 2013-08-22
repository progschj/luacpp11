# luacpp11

luacpp11 is a minimal C++11 lua binding library that tries to integrate with the
design of the lua C API.

## Summary

luacpp11 is designed to be used along with the lua C API. As such it doesn't
implement higher order abstraction like registration facilities for classes and
doesn't set up elaborate metatables for C++ types (it creates metatables but
only adds a `__gc` metafunction). All the functionality is exposed through
function templates that mimic similar functions in the lua C API.

The library is header only and thus doesn't have to be compiled separately.
Since the library doesn't include any lua headers by itself it has to be
included after those. All public functions and types are placed into the
`luacpp11` namespace.

### `push_callable`

`push_callable` is an overloaded function that is used to push almost any
callable C++ object onto the lua stack. For function pointers (member and
non-member) and `std::function` objects the return and argument types are
deduced automatically otherwise they have to be specified.

```c++
int foo(double arg) { ... }
struct A { void bar(); };
struct Ftor { int operator()(double arg); };
...
luacpp11::push_callable(L, foo);
luacpp11::push_callable(L, &A::bar);
luacpp11::push_callable(L, std::function<int(double)>(Ftor()));
luacpp11::push_callable<int(double)>(L, Ftor());
```

functions that have to extract the arguments themselves from the lua state
should take exactly one argument of type `lua_State*` at the end of their
parameter list. Functions that handle the return values themselves should be
declared with return type `luacpp11::luareturn`. Notice that pushing a
`lua_CFunction` with `push_callable` will not give the desired result since it
has return type `int` (just  keep using `lua_pushcfunction` for that).
Multiple return values can also be returned to lua by returning a `std::tuple`.
```c++
luacpp11::luareturn foo(int k, lua_State *L)
{
    std::cout << lua_gettop(L) << " arguments" << std::endl;
    lua_pushinteger(L, k);
    return 1;
}
...
luacpp11::push_callable(L, foo); // ok
```

### `push` and `emplace`
`push` works the same way as the `lua_pushXYZ` functions. It copy constructs
or moves its second argument into a userdata that is created on top of the lua
stack. The lifetime of these objects is then controlled by lua (a `__gc` method
is added to their metatable). Notice that for pointers this means only that the
pointer itself is "collected" but it is not deleted.
Additionally objects can be constructed in place in userdata memory by use of
`emplace`.

```c++
struct A { A(double pi) {...} };
...
A a(3.14159);
luacpp11::push(L, A); // copy constructs into a userdata
luacpp11::push(L, A(3.14159)); // move constructs into a userdata
luacpp11::emplace<A>(L, 3.14159); // emplace into a userdata
luacpp11::push(L, new A(3.14159)); // the lua garbage collector will not delete this!
luacpp11::push(L, &a); // which means this is ok too
```

### `is`
`is<T>` returns true if the object at a given index is of type `T`. Notice that
that `T`, `const T`, `T*` and `const T*` are different types in this context
(`&T` is interpreted as `T` though).

```c++
luacpp11::emplace<A>(L);
luacpp11::is<A>(L, -1);         // true
luacpp11::is<const A>(L, -1);   // false
luacpp11::is<A*>(L, -1);        // false
luacpp11::is<const A*>(L, -1);  // false
luacpp11::is<B>(L, -1);         // false
```

### `isconvertible`
`isconvertible<T>` returns true if the object at a given index is of type `T` or
can be converted to it by `to`.

```c++
luacpp11::emplace<A>(L);
luacpp11::isconvertible<A>(L, -1);         // true
luacpp11::isconvertible<const A>(L, -1);   // true
luacpp11::isconvertible<A*>(L, -1);        // true
luacpp11::isconvertible<const A*>(L, -1);  // true
luacpp11::isconvertible<B>(L, -1);         // false
```

### `to`
`to<T>` retrieves a object of type `T` from a given index and throws an exception
in case of a type mismatch. This function will "convert" from and to pointer
types as well as to const. Additionally it will also convert shared_ptr to pointer
or value types.

```c++
A a;
luacpp11::push(L, &a);
luacpp11::to<A>(L, -1);         // ok
luacpp11::to<const A>(L, -1);   // ok
luacpp11::to<A*>(L, -1);        // ok
luacpp11::to<const A*>(L, -1);  // ok

luacpp11::emplace<A>(L);
luacpp11::to<A>(L, -1);         // ok
luacpp11::to<const A>(L, -1);   // ok
luacpp11::to<A*>(L, -1);        // ok
luacpp11::to<const A*>(L, -1);  // ok

luacpp11::emplace<const A>(L);
luacpp11::to<A>(L, -1);         // exception
luacpp11::to<const A>(L, -1);   // ok
luacpp11::to<A*>(L, -1);        // exception
luacpp11::to<const A*>(L, -1);  // ok
```

### `toexact`
`toexact<T>` retrieves a object of type `T` from a given index without trying to
perform any conversions.

```c++
A a;
luacpp11::push(L, &a);
luacpp11::toexact<A>(L, -1);         // exception
luacpp11::toexact<const A>(L, -1);   // exception
luacpp11::toexact<A*>(L, -1);        // ok
luacpp11::toexact<const A*>(L, -1);  // exception
```

### `tounchecked`
`tounchecked<T>` retrieves a object of type `T` from a given index without
trying to perform any conversions or type checks. This may result in  undefined
behavior if the object at the given stack index isn't of the correct type.


### `getmetatable`
`getmetatable<T>` pushes the metatable associated with T onto the stack. Again
`T`, `const T`, `T*` and `const T*` are different types in this context and will
all give different metatables. Comparing this metatable with the table returned
by `lua_getmetatable` for a given object can be used to test wether that object
is of type `T` (this is what `is` does).

```c++
luacpp11::getmetatable< std::vector<int> >(L);

// luacpp11 only adds a __gc method to the metatable
lua_getfield(L, -1, "__gc");
if(lua_isfunction(L, -1))
    std::cout << "found __gc function" << std::endl;

lua_pop(L, 2);
```

### `ref`

`luacpp11::ref` holds a reference to any object in lua. `ref` objects
can be obtained by using the `to...` functions on any object on the
stack and pushed back into it without requiring knoweldge of the object
type. It internally uses the `luaL_ref` mechanism, so it can also
be used to prevent collection of lua created objects.

### `newthread`

`luacpp11::newthread` should be used instead of `lua_newthread` if
luacpp11 types are involved. It has the same behavior with the exception
that it allows luacpp11 to associate thread states with their original
states. Otherwise custom types will get their own meta table in each
state and userdata types created in one thread will not be recognized
in other threads that share the same global environment.

### The `register_hook` trait

The `register_hook` trait can be used to execute code whenever luacpp11 internally
creates a metatable for a given type. This happens once per type and lua_State
on first use of the type. This can be used to perform more elaborate type
registration (like adding metamethods etc.) on demand.

```c++
namespace luacpp11 {
    template<class T>
    struct register_hook< std::vector<T> > {
        static void on_register(lua_State *L)
        {
            lua_pushstring(L, "__len");
            // __len can have a second nil argument so we can't use ::size directly
            luacpp11::push_callable<size_t(std::vector<T>*, lua_State*)>(L,
                [](std::vector<T> *v, lua_State*){ return v->size(); }
            );
            lua_rawset(L, -3);
        }
    };
}
//...
luacpp11::emplace< std::vector<int> >(L, 13);
lua_setglobal(L, "vec");
luaL_dostring(L, "print(#vec)\n"); // prints vector size (13)

```


