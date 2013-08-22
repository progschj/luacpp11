#ifndef LUACPP11_H
#define LUACPP11_H

#include <functional>
#include <type_traits>
#include <tuple>
#include <unordered_map>
#include <string>
#include <stdexcept>
#include <memory>

namespace luacpp11 {

namespace detail {
template<class T, class Enable = void>
struct StackHelper;
}

// return type for functions pushing their own return values
struct luareturn {
    luareturn(int count) : count(count) {}
    int count;
};

template<class T>
struct register_hook {
    static void on_register(lua_State *L) { }
};

class ref {
public:
    ref(const ref &that) : L(that.L)
    {
        if(that.L != 0)
        {
            lua_rawgeti(L, LUA_REGISTRYINDEX, that.r);
            r = luaL_ref(L, LUA_REGISTRYINDEX);
        }
    }
    ref(ref &&that) : L(that.L), r(that.r)
    {
        that.L = 0;
    }
    ref& operator=(const ref &that)
    {
        ref tmp(that);
        std::swap(tmp.L, L);
        std::swap(tmp.r, r);
        return *this;
    }
    ref& operator=(ref &&that)
    {
        L = that.L;
        r = that.r;
        that.L = 0;
        return *this;
    }
    ~ref()
    {
        if(L != 0)
            luaL_unref(L, LUA_REGISTRYINDEX, r);
    }
private:
    friend struct detail::StackHelper<ref>;

    ref(lua_State *L, int r) : L(L), r(r) { }

    lua_State *L;
    int r;
};

namespace detail {

// sequence handling tools
template<typename... T>
struct type_seq { };

template<int... I>
struct int_seq { };

template<int L, class seq>
struct build_int_seq {};

template<int L, int... S>
struct build_int_seq<L, int_seq<S...> > {
    typedef typename build_int_seq<L-1, int_seq<L-1, S...> >::value value;
};

template<int... S>
struct build_int_seq<0, int_seq<S...> > {
    typedef int_seq<S...> value;
};

template<int L>
struct make_int_seq {
    typedef typename build_int_seq<L, int_seq< > >::value value;
};

template<class seq, class T>
struct count;

template<class T, class U, class... Rest>
struct count<type_seq<U, Rest...>, T> {
    static const size_t value = count<type_seq<Rest...>, T >::value + (std::is_same<U, T>::value?1:0);
};

template<class T>
struct count<type_seq< >, T> {
    static const size_t value = 0;
};

template<class seq, class T>
struct contains;

template<class T, class U, class... Rest>
struct contains<type_seq<U, Rest...>, T> {
    static const bool value = count<type_seq<Rest...>, T >::value || std::is_same<T, U>::value;
};

template<class T>
struct contains<type_seq< >, T> {
    static const bool value = false;
};

template<class seq>
struct first;

template<class T, class... Rest>
struct first< type_seq<T, Rest...> > {
    typedef T type;
};

template<>
struct first< type_seq< > > {
    typedef void type;
};

template<class seq>
struct last;

template<class T, class... Rest>
struct last< type_seq<T, Rest...> > {
    typedef typename last< type_seq<Rest...> >::type type;
};

template<class T>
struct last< type_seq<T> > {
    typedef T type;
};

template<>
struct last< type_seq< > > {
    typedef void type;
};


template<class T>
struct is_const_or_refers_to_const { static const bool value = false; };

template<class T>
struct is_const_or_refers_to_const<const T> { static const bool value = true; };
template<class T>
struct is_const_or_refers_to_const<const T*> { static const bool value = true; };
template<class T>
struct is_const_or_refers_to_const<const T&> { static const bool value = true; };

template<class T>
struct remove_const_or_refers_to_const { typedef T type; };

template<class T>
struct remove_const_or_refers_to_const<const T> { typedef T type; };
template<class T>
struct remove_const_or_refers_to_const<const T*> { typedef T* type; };
template<class T>
struct remove_const_or_refers_to_const<const T&> { typedef T& type; };

template<class T>
bool userdataIs(lua_State *L, int index)
{
    if(lua_getmetatable(L, index) == 0)
        return false;
    StackHelper<T>::getmetatable(L);
    bool equal = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);
    return equal;
}

template<class T>
T* getPointer(lua_State *L, int index)
{
    if(!lua_isuserdata(L, index))
        return nullptr;

    if(userdataIs<T*>(L, index))
    {
        return *static_cast<T**>(lua_touserdata(L, index));
    }
    else if(userdataIs<T>(L, index))
    {
        return static_cast<T*>(lua_touserdata(L, index));
    }
    else if(userdataIs< std::shared_ptr<T> >(L, index))
    {
        return static_cast<std::shared_ptr<T>*>(lua_touserdata(L, index))->get();
    }
    else if(is_const_or_refers_to_const<T>::value)
    {
        return getPointer<typename remove_const_or_refers_to_const<T>::type>(L, index);
    }
    else
    {
        return nullptr;
    }
}

template<class T>
bool canGetPointer(lua_State *L, int index)
{
    if(!lua_isuserdata(L, index))
        return false;

    if(userdataIs<T*>(L, index))
    {
        return true;
    }
    else if(userdataIs<T>(L, index))
    {
        return true;
    }
    else if(userdataIs< std::shared_ptr<T> >(L, index))
    {
        return true;
    }
    else if(is_const_or_refers_to_const<T>::value)
    {
        return canGetPointer<typename remove_const_or_refers_to_const<T>::type>(L, index);
    }
    else
    {
        return false;
    }
}

template<class T>
struct GetHelper {
    template<int Index>
    static T& get(lua_State *L)
    {
        T *ptr = getPointer<T>(L, Index);
        if(ptr == nullptr)
        {
            lua_pushfstring(L, "expected userdata in argument %d", Index);
            lua_error(L);
        }
        return *ptr;
    }
    static T& get(lua_State *L, int index)
    {
        T *ptr = getPointer<T>(L, index);
        if(ptr == nullptr)
        {
            throw std::runtime_error("type mismatch");
        }
        return *ptr;
    }
};

template<class U>
struct GetHelper<U*> {
    typedef U* T;
    template<int Index>
    static T get(lua_State *L)
    {
        if(lua_isnil(L, Index)) return nullptr;
        T ptr = getPointer<U>(L, Index);
        if(ptr == nullptr)
        {
            lua_pushfstring(L, "expected userdata in argument %d", Index);
            lua_error(L);
        }
        return ptr;
    }
    static T get(lua_State *L, int index)
    {
        if(lua_isnil(L, index)) return nullptr;
        T ptr = getPointer<U>(L, index);
        if(ptr == nullptr)
        {
            throw std::runtime_error("type mismatch");
        }
        return ptr;
    }
};

inline lua_State* parent_state(lua_State *L, lua_State *L2 = nullptr) {
    static std::unordered_map<lua_State*, lua_State*> parent_map;
    if(L2 == nullptr)
    {
        auto iter = parent_map.find(L);
        if(iter == parent_map.end())
            return L;
        else
            return iter->second;
    }
    else
    {
        return parent_map[L2] = parent_state(L);
    }
}

template<class T, class Enable>
struct StackHelper {
    template<int Index>
    static typename std::conditional< std::is_pointer<T>::value, T, T&>::type get(lua_State *L)
    {
        return GetHelper<T>::template get<Index>(L);
    }
    static typename std::conditional< std::is_pointer<T>::value, T, T&>::type get(lua_State *L, int index)
    {
        return GetHelper<T>::get(L, index);
    }
    static bool isconvertible(lua_State *L, int index)
    {
        return canGetPointer<typename std::remove_pointer<T>::type>(L, index);
    }
    static typename std::conditional< std::is_pointer<T>::value, T, T&>::type getexact(lua_State *L, int index)
    {
        if(!is(L, index))
            throw std::runtime_error("type mismatch");
        return *static_cast<T*>(lua_touserdata(L, index));
    }
    static typename std::conditional< std::is_pointer<T>::value, T, T&>::type getunchecked(lua_State *L, int index)
    {
        return *static_cast<T*>(lua_touserdata(L, index));
    }
    static bool is(lua_State *L, int index)
    {
        if(!lua_isuserdata(L, index))
            return false;
        return userdataIs<T>(L, index);
    }
    static void push(lua_State *L, const T& value)
    {
        new (lua_newuserdata(L, sizeof(T))) T(value);
        getmetatable(L);
        lua_setmetatable(L, -2);
    }
    static void push(lua_State *L, T&& value)
    {
        new (lua_newuserdata(L, sizeof(T))) T(std::move(value));
        getmetatable(L);
        lua_setmetatable(L, -2);
    }
    template<class... Args>
    static void emplace(lua_State *L, Args&&... args)
    {
        new (lua_newuserdata(L, sizeof(T))) T(std::forward<Args>(args)...);
        getmetatable(L);
        lua_setmetatable(L, -2);
    }
    static void getmetatable(lua_State *L)
    {
        typedef typename std::unordered_map<lua_State*, int>::value_type map_entry_t;
        static std::unordered_map<lua_State*, int> index_table;
        auto iter = index_table.find(parent_state(L));
        if(iter == index_table.end())
        {
            lua_newtable(L);
            lua_pushstring(L, "__gc");
            lua_pushcfunction(L, destroy_T);
            lua_rawset(L, -3);
            iter = index_table.insert(map_entry_t(L, luaL_ref(L, LUA_REGISTRYINDEX))).first;

            lua_rawgeti(L, LUA_REGISTRYINDEX, iter->second);
            register_hook<T>::on_register(L);
            return;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, iter->second);
    }
private:
    static int destroy_T(lua_State *L)
    {
        T *userdata = static_cast <T*> (lua_touserdata (L, -1));
        userdata->~T();
        return 0;
    }
};

template<class T>
struct StackHelper<T, typename std::enable_if<std::is_lvalue_reference<T>::value>::type>
    : public StackHelper<typename  std::remove_reference<T>::type >
{
};


template<class T>
struct StackHelper<T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<typename std::remove_cv<T>::type, bool>::value >::type> {
    template<int Index>
    static T get(lua_State *L)
    {
        if(!lua_isnumber(L, Index))
        {
            lua_pushfstring(L, "expected number in argument %d", Index);
            lua_error(L);
        }
        return lua_tointeger(L, Index);
    }
    static bool is(lua_State *L, int index)
    {
        return lua_isnumber(L, index);
    }
    static bool isconvertible(lua_State *L, int index)
    {
        return is(L, index);
    }
    static T get(lua_State *L, int index)
    {
        return getexact(L, index);
    }
    static T getexact(lua_State *L, int index)
    {
        if(!is(L, index))
            throw std::runtime_error("type mismatch");
        return getunchecked(L, index);
    }
    static T getunchecked(lua_State *L, int index)
    {
        return lua_tointeger(L, index);
    }
    static void push(lua_State *L, T value)
    {
        lua_pushinteger(L, value);
    }
};

template<class T>
struct StackHelper<T, typename std::enable_if<std::is_same<typename std::remove_cv<T>::type, bool>::value >::type> {
    template<int Index>
    static T get(lua_State *L)
    {
        if(!lua_isboolean(L, Index))
        {
            lua_pushfstring(L, "expected boolean in argument %d", Index);
            lua_error(L);
        }
        return lua_toboolean(L, Index);
    }
    static bool is(lua_State *L, int index)
    {
        return lua_isboolean(L, index);
    }
    static bool isconvertible(lua_State *L, int index)
    {
        return is(L, index);
    }
    static T get(lua_State *L, int index)
    {
        return getexact(L, index);
    }
    static T getexact(lua_State *L, int index)
    {
        if(!is(L, index))
            throw std::runtime_error("type mismatch");
        return getunchecked(L, index);
    }
    static T getunchecked(lua_State *L, int index)
    {
        return lua_toboolean(L, index);
    }
    static void push(lua_State *L, T value)
    {
        lua_pushboolean(L, value);
    }
};

template<class T>
struct StackHelper<T, typename std::enable_if<std::is_floating_point<T>::value >::type> {
    template<int Index>
    static T get(lua_State *L)
    {
        if(!lua_isnumber(L, Index))
        {
            lua_pushfstring(L, "expected number in argument %d", Index);
            lua_error(L);
        }
        return lua_tonumber(L, Index);
    }
    static bool is(lua_State *L, int index)
    {
        return lua_isnumber(L, index);
    }
    static bool isconvertible(lua_State *L, int index)
    {
        return is(L, index);
    }
    static T get(lua_State *L, int index)
    {
        return getexact(L, index);
    }
    static T getexact(lua_State *L, int index)
    {
        if(!is(L, index))
            throw std::runtime_error("type mismatch");
        return getunchecked(L, index);
    }
    static T getunchecked(lua_State *L, int index)
    {
        return lua_tonumber(L, index);
    }
    static void push(lua_State *L, T value)
    {
        lua_pushnumber(L, value);
    }
};

template<class T>
struct StackHelper<T, typename std::enable_if<std::is_same<typename std::remove_const<T>::type, std::string>::value >::type> {
    template<int Index>
    static T get(lua_State *L)
    {
        if(!lua_isstring(L, Index))
        {
            lua_pushfstring(L, "expected string in argument %d", Index);
            lua_error(L);
        }
        return T(lua_tostring(L, Index));
    }
    static bool is(lua_State *L, int index)
    {
        return lua_isstring(L, index);
    }
    static bool isconvertible(lua_State *L, int index)
    {
        return is(L, index);
    }
    static T get(lua_State *L, int index)
    {
        return getexact(L, index);
    }
    static T getexact(lua_State *L, int index)
    {
        if(!is(L, index))
            throw std::runtime_error("type mismatch");
        return getunchecked(L, index);
    }
    static T getunchecked(lua_State *L, int index)
    {
        return T(lua_tostring(L, index));
    }
    static void push(lua_State *L, const T &value)
    {
        lua_pushstring(L, value.c_str());
    }
};

template<class T>
struct StackHelper<T, typename std::enable_if<std::is_same<T, const char*>::value >::type> {
    template<int Index>
    static T get(lua_State *L)
    {
        if(!lua_isstring(L, Index))
        {
            lua_pushfstring(L, "expected string in argument %d", Index);
            lua_error(L);
        }
        return lua_tostring(L, Index);
    }
    static bool is(lua_State *L, int index)
    {
        return lua_isstring(L, index);
    }
    static bool isconvertible(lua_State *L, int index)
    {
        return is(L, index);
    }
    static T get(lua_State *L, int index)
    {
        return getexact(L, index);
    }
    static T getexact(lua_State *L, int index)
    {
        if(!is(L, index))
            throw std::runtime_error("type mismatch");
        return getunchecked(L, index);
    }
    static T getunchecked(lua_State *L, int index)
    {
        return lua_tostring(L, index);
    }
    static void push(lua_State *L, T value)
    {
        lua_pushstring(L, value);
    }
};

template<class T>
struct StackHelper<T, typename std::enable_if<std::is_same<T, ref>::value >::type> {
    template<int Index>
    static T get(lua_State *L)
    {
        lua_pushvalue(L, Index);
        return ref(L, luaL_ref(L, LUA_REGISTRYINDEX));
    }
    static bool is(lua_State *L, int index)
    {
        return true;
    }
    static bool isconvertible(lua_State *L, int index)
    {
        return true;
    }
    static T get(lua_State *L, int index)
    {
        return getexact(L, index);
    }
    static T getexact(lua_State *L, int index)
    {
        return getunchecked(L, index);
    }
    static T getunchecked(lua_State *L, int index)
    {
        lua_pushvalue(L, index);
        return ref(L, luaL_ref(L, LUA_REGISTRYINDEX));
    }
    static void push(lua_State *L, const T &value)
    {
        if(L != value.L)
            std::runtime_error("lua_State mismatch");
        lua_rawgeti(L, LUA_REGISTRYINDEX, value.r);
    }
};

template<class T>
struct StackHelper<T, typename std::enable_if<std::is_same<T, lua_State*>::value >::type> {
    template<int Index>
    static T get(lua_State *L)
    {
        return L;
    }
};

template<class T, size_t I>
struct TupleStackHelper {
    static void push(lua_State *L, const T &values)
    {
        TupleStackHelper<T, I-1>::push(L, values);
        StackHelper<typename std::tuple_element<I-1, T>::type>::push(L, std::get<I-1>(values));
    }
};

template<class T>
struct TupleStackHelper<T, 0> {
    static void push(lua_State*, const T&)
    {
    }
};

template<class... Args>
struct StackHelper<std::tuple<Args...>, void> {
    static void push(lua_State *L, const std::tuple<Args...> &values)
    {
        TupleStackHelper<std::tuple<Args...>, sizeof...(Args) >::push(L, values);
    }
};

template<class T>
struct return_value_count {
    static const size_t value = 1;
};

template<>
struct return_value_count<void> {
    static const size_t value = 0;
};

template<class... Args>
struct return_value_count< std::tuple<Args...> > {
    static const size_t value = sizeof...(Args);
};


template<class T, class Sig>
class CallHelper;

template<class T, class R, class... Args>
class CallHelper< T, R(Args...) > {
public:
    typedef R return_type;
    typedef type_seq<Args...> Arguments;
    typedef typename make_int_seq<sizeof...(Args)>::value Indices;

    template<class F>
    CallHelper(F&& f)
    : fun(std::forward<F>(f))
    {
    }

    R operator()(lua_State *L)
    {
        return exec(L, Arguments(), Indices());
    }

    int static cfunction_call(lua_State *L)
    {
        static_assert(count<Arguments, lua_State* >::value <= 1, "to many lua_State* arguments");
        static_assert((count<Arguments, lua_State* >::value != 1) ||
                    (std::is_same<typename last<Arguments>::type, lua_State*>::value),
                    "lua_State* has to be last argument");

        if(count<Arguments, lua_State* >::value == 0)
        {
            if(lua_gettop(L) != sizeof...(Args))
            {
                lua_pushfstring(L, "expected %d arguments but got %d", sizeof...(Args), lua_gettop(L));
                lua_error(L);
            }
        }
        else if(count<Arguments, lua_State* >::value == 1)
        {
            if(lua_gettop(L) < sizeof...(Args)-1)
            {
                lua_pushfstring(L, "expected at least %d arguments but got %d", sizeof...(Args), lua_gettop(L));
                lua_error(L);
            }
        }

        CallHelper &helper = *static_cast <CallHelper*> (lua_touserdata (L, lua_upvalueindex (1)));
        StackHelper<R>::push(L, helper(L));
        return return_value_count<R>::value;
    }
private:
    template<class... A, int... I>
    R exec(lua_State *L, type_seq<A...>, int_seq<I...>)
    {
        return fun(StackHelper<A>::template get<I+1>(L)...);
    }
    T fun;
};

template<class T, class... Args>
class CallHelper< T, void(Args...) > {
public:
    typedef void return_type;
    typedef type_seq<Args...> Arguments;
    typedef typename make_int_seq<sizeof...(Args)>::value Indices;

    template<class F>
    CallHelper(F&& f)
    : fun(std::forward<F>(f))
    {
    }

    void operator()(lua_State *L)
    {
        exec(L, Arguments(), Indices());
    }

    int static cfunction_call(lua_State *L)
    {
        static_assert(count<Arguments, lua_State* >::value <= 1, "to many lua_State* arguments");
        static_assert((count<Arguments, lua_State* >::value != 1) ||
                    (std::is_same<typename last<Arguments>::type, lua_State*>::value),
                    "lua_State* has to be last argument");

        if(count<Arguments, lua_State* >::value == 0)
        {
            if(lua_gettop(L) != sizeof...(Args))
            {
                lua_pushfstring(L, "expected %d arguments but got %d", sizeof...(Args), lua_gettop(L));
                lua_error(L);
            }
        }
        else if(count<Arguments, lua_State* >::value == 1)
        {
            if(lua_gettop(L) < sizeof...(Args)-1)
            {
                lua_pushfstring(L, "expected at least %d arguments but got %d", sizeof...(Args), lua_gettop(L));
                lua_error(L);
            }
        }

        CallHelper &helper = *static_cast <CallHelper*> (lua_touserdata (L, lua_upvalueindex (1)));
        helper(L);
        return 0;
    }
private:
    template<class... A, int... I>
    void exec(lua_State *L, type_seq<A...>, int_seq<I...>)
    {
        return fun(StackHelper<A>::template get<I+1>(L)...);
    }
    T fun;
};

template<class T, class... Args>
class CallHelper< T, luareturn(Args...) > {
public:
    typedef luareturn return_type;
    typedef type_seq<Args...> Arguments;
    typedef typename make_int_seq<sizeof...(Args)>::value Indices;

    template<class F>
    CallHelper(F&& f)
    : fun(std::forward<F>(f))
    {
    }

    int operator()(lua_State *L)
    {
        return exec(L, Arguments(), Indices());
    }

    int static cfunction_call(lua_State *L)
    {
        static_assert(count<Arguments, lua_State* >::value <= 1, "to many lua_State* arguments");
        static_assert((count<Arguments, lua_State* >::value != 1) ||
                    (std::is_same<typename last<Arguments>::type, lua_State*>::value),
                    "lua_State* has to be last argument");

        if(count<Arguments, lua_State* >::value == 0)
        {
            if(lua_gettop(L) != sizeof...(Args))
            {
                lua_pushfstring(L, "expected %d arguments but got %d", sizeof...(Args), lua_gettop(L));
                lua_error(L);
            }
        }
        else if(count<Arguments, lua_State* >::value == 1)
        {
            if(lua_gettop(L) < sizeof...(Args)-1)
            {
                lua_pushfstring(L, "expected at least %d arguments but got %d", sizeof...(Args), lua_gettop(L));
                lua_error(L);
            }
        }

        CallHelper &helper = *static_cast <CallHelper*> (lua_touserdata (L, lua_upvalueindex (1)));
        return helper(L);
    }
private:
    template<class... A, int... I>
    int exec(lua_State *L, type_seq<A...>, int_seq<I...>)
    {
        return fun(StackHelper<A>::template get<I+1>(L)...).count;
    }
    T fun;
};

template<class C, class R, class... Args>
struct mem_fun_wrap {
    mem_fun_wrap(R (C::*fun)(Args...)) : fun(fun) { }
    template<class... Args2>
    R operator()(C *c, Args2&&... args) const
    {
        return (c->*fun)(std::forward<Args>(args)...);
    }
    template<class... Args2>
    R operator()(C &c, Args2&&... args) const
    {
        return (c.*fun)(std::forward<Args2>(args)...);
    }
private:
    R (C::*fun)(Args...);
};

template<class C, class R, class... Args>
struct const_mem_fun_wrap {
    const_mem_fun_wrap(R (C::*fun)(Args...) const) : fun(fun) { }
    template<class... Args2>
    R operator()(const C *c, Args2&&... args) const
    {
        return (c->*fun)(std::forward<Args2>(args)...);
    }
    template<class... Args2>
    R operator()(const C &c, Args2&&... args) const
    {
        return (c.*fun)(std::forward<Args2>(args)...);
    }
private:
    R (C::*fun)(Args...) const;
};

}

template<class T, class F>
void push_callable(lua_State *L, F&& f)
{
    detail::StackHelper< detail::CallHelper< std::function<T>, T > >::emplace(L, std::forward<F>(f));
    lua_pushcclosure (L, detail::CallHelper< std::function<T>, T >::cfunction_call, 1);
}

template<class T>
void push_callable(lua_State *L, const std::function<T> &f)
{
    detail::StackHelper< detail::CallHelper< std::function<T>, T > >::emplace(L, f);
    lua_pushcclosure (L, detail::CallHelper< std::function<T>, T >::cfunction_call, 1);
}

template<class T>
void push_callable(lua_State *L, std::function<T> &&f)
{
    detail::StackHelper< detail::CallHelper< std::function<T>, T > >::emplace(L, std::move(f));
    lua_pushcclosure (L, detail::CallHelper< std::function<T>, T >::cfunction_call, 1);
}

template<class R, class... Args>
void push_callable(lua_State *L, R (*f)(Args...))
{
    detail::StackHelper< detail::CallHelper< R(*)(Args...), R(Args...) > >::emplace(L, f);
    lua_pushcclosure (L, detail::CallHelper< R(*)(Args...), R(Args...) >::cfunction_call, 1);
}

template<class C, class R, class... Args>
void push_callable(lua_State *L, R (C::*f)(Args...))
{
    detail::StackHelper< detail::CallHelper< detail::mem_fun_wrap<C, R, Args...>, R(C*, Args...) > >::emplace(L, f);
    lua_pushcclosure (L, detail::CallHelper< detail::mem_fun_wrap<C, R, Args...>, R(C*, Args...) >::cfunction_call, 1);
}

template<class C, class R, class... Args>
void push_callable(lua_State *L, R (C::*f)(Args...) const)
{
    detail::StackHelper< detail::CallHelper< detail::const_mem_fun_wrap<C, R, Args...>, R(const C*, Args...) > >::emplace(L, f);
    lua_pushcclosure (L, detail::CallHelper< detail::const_mem_fun_wrap<C, R, Args...>, R(const C*, Args...) >::cfunction_call, 1);
}

template<class T>
void push(lua_State *L, T&& value)
{
    detail::StackHelper<T>::push(L, std::forward<T>(value));
}

template<class T, class... Args>
void emplace(lua_State *L, Args&& ...args)
{
    detail::StackHelper<T>::emplace(L, std::forward<Args>(args)...);
}

template<class T>
bool is(lua_State *L, int index)
{
    return detail::StackHelper<T>::is(L, index);
}

template<class T>
bool isconvertible(lua_State *L, int index)
{
    return detail::StackHelper<T>::isconvertible(L, index);
}

template<class T>
auto to(lua_State *L, int index) -> decltype(detail::StackHelper<T>::get(L, index))
{
    return detail::StackHelper<T>::get(L, index);
}

template<class T>
auto toexact(lua_State *L, int index) -> decltype(detail::StackHelper<T>::getexact(L, index))
{
    return detail::StackHelper<T>::getexact(L, index);
}

template<class T>
auto tounchecked(lua_State *L, int index) -> decltype(detail::StackHelper<T>::getexact(L, index))
{
    return detail::StackHelper<T>::getunchecked(L, index);
}

template<class T>
void getmetatable(lua_State *L)
{
    detail::StackHelper<T>::getmetatable(L);
}

inline lua_State* newthread(lua_State *L)
{
    lua_State *L2 = lua_newthread(L);
    detail::parent_state(L, L2);
    return L2;
}

}
#endif
