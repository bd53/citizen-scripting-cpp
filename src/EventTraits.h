#pragma once

#include <tuple>
#include <type_traits>

namespace fx::detail
{

template<typename T>
struct fn_traits : fn_traits<decltype(&std::decay_t<T>::operator())> {};

template<typename C, typename R, typename... Args>
struct fn_traits<R(C::*)(Args...) const> { using tuple_type = std::tuple<Args...>; };

template<typename C, typename R, typename... Args>
struct fn_traits<R(C::*)(Args...)> { using tuple_type = std::tuple<Args...>; };

template<typename C, typename R, typename... Args>
struct fn_traits<R(C::*)(Args...) const noexcept> { using tuple_type = std::tuple<Args...>; };

template<typename C, typename R, typename... Args>
struct fn_traits<R(C::*)(Args...) noexcept> { using tuple_type = std::tuple<Args...>; };

template<typename R, typename... Args>
struct fn_traits<R(*)(Args...)> { using tuple_type = std::tuple<Args...>; };

template<typename R, typename... Args>
struct fn_traits<R(*)(Args...) noexcept> { using tuple_type = std::tuple<Args...>; };

template<typename F, typename ArgsTuple, size_t... Is>
void invoke_typed_handler(F& fn, const std::string& source, const EventArgs& args, std::index_sequence<Is...>)
{
    fn(source, args.get<std::decay_t<std::tuple_element_t<Is + 1, ArgsTuple>>>(Is)...);
}

template<typename F>
EventHandler wrap_typed_handler(F&& fn)
{
    using args_tuple = typename fn_traits<std::decay_t<F>>::tuple_type;
    constexpr size_t n = std::tuple_size_v<args_tuple>;
    static_assert(n >= 1, "Something went wrong");
    return [f = std::forward<F>(fn)](const std::string& source, const EventArgs& args) mutable {
        invoke_typed_handler<decltype(f), args_tuple>(f, source, args, std::make_index_sequence<n - 1>{});
    };
}

}
