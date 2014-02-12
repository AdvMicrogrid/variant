#ifndef UTIL_VARIANT_HPP
#define UTIL_VARIANT_HPP

#include <utility>
#include <typeinfo>
#include <type_traits>
#include <algorithm> // std::move/swap
#include <stdexcept> // runtime_error
#include <new> // operator new
#include <cstddef> // size_t

#ifdef NDEBUG
 #define VARIANT_INLINE inline __attribute__((always_inline))
#else
 #define VARIANT_INLINE __attribute__((noinline))
#endif

namespace util {

namespace detail {

static constexpr std::size_t invalid_value = std::size_t(-1);

template <typename T, typename...Types>
struct type_traits;

template <typename T, typename First, typename...Types>
struct type_traits<T, First, Types...>
{
    static constexpr std::size_t id = std::is_same<T, First>::value ? sizeof...(Types) : type_traits<T, Types...>::id;
};

template <typename T>
struct type_traits<T>
{
    static constexpr std::size_t id = invalid_value;
};

template <typename T, typename...Types>
struct is_valid_type;

template <typename T, typename First, typename... Types>
struct is_valid_type<T,First,Types...>
{
    static constexpr bool value = std::is_same<T,First>::value
        || is_valid_type<T,Types...>::value;
};

template <typename T>
struct is_valid_type<T> : std::false_type {};

}

template <std::size_t arg1, std::size_t ... others>
struct static_max;

template <std::size_t arg>
struct static_max<arg>
{
    static const std::size_t value = arg;
};

template <std::size_t arg1, std::size_t arg2, std::size_t ... others>
struct static_max<arg1, arg2, others...>
{
    static const std::size_t value = arg1 >= arg2 ? static_max<arg1, others...>::value :
        static_max<arg2, others...>::value;
};

template<typename... Types>
struct variant_helper;

template<typename T, typename... Types>
struct variant_helper<T, Types...>
{
    VARIANT_INLINE static void destroy(std::size_t id, void * data)
    {
        if (id == sizeof...(Types))
        {
            reinterpret_cast<T*>(data)->~T();
        }
        else
        {
            variant_helper<Types...>::destroy(id, data);
        }
    }

    VARIANT_INLINE static void move(std::size_t old_t, void * old_v, void * new_v)
    {
        if (old_t == sizeof...(Types))
        {
            new (new_v) T(std::move(*reinterpret_cast<T*>(old_v)));
        }
        else
        {
            variant_helper<Types...>::move(old_t, old_v, new_v);
        }
    }

    VARIANT_INLINE static void copy(std::size_t old_t, const void * old_v, void * new_v)
    {
        if (old_t == sizeof...(Types))
        {
            new (new_v) T(*reinterpret_cast<const T*>(old_v));
        }
        else
        {
            variant_helper<Types...>::copy(old_t, old_v, new_v);
        }
    }
};

template<> struct variant_helper<>
{
    VARIANT_INLINE static void destroy(std::size_t, void *) {}
    VARIANT_INLINE static void move(std::size_t, void *, void *) {}
    VARIANT_INLINE static void copy(std::size_t, const void *, void *) {}
};

namespace detail {

template <typename F, typename V, typename...Types>
struct dispatcher;

template <typename F, typename V, typename T, typename...Types>
struct dispatcher<F,V,T,Types...>
{
    typedef typename std::result_of<F(V const&)>::type result_type;
    VARIANT_INLINE static result_type apply(V const& v, F f)
    {
        if (v.get_type_id() == sizeof...(Types))
        {
            return f(v. template get<T>());
        }
        else
        {
            return dispatcher<F,V,Types...>::apply(v, f);
        }
    }
};

template<typename F,typename V>
struct dispatcher<F,V>
{
    typedef typename std::result_of<F(V const&)>::type result_type;
    VARIANT_INLINE static result_type apply(V const&, F)
    {
        throw std::runtime_error("dispatch: FAIL");
    }
};

} // detail


template<typename... Types>
struct variant
{
private:

    static const std::size_t data_size = static_max<sizeof(Types)...>::value;
    static const std::size_t data_align = static_max<alignof(Types)...>::value;

    using data_t = typename std::aligned_storage<data_size, data_align>::type;

    using helper_t = variant_helper<Types...>;

    std::size_t type_id;
    data_t data;

public:

    VARIANT_INLINE variant()
        : type_id(detail::invalid_value) {}

    template <typename T>
    VARIANT_INLINE explicit variant(T const& val) noexcept
        : type_id(detail::type_traits<T, Types...>::id)
    {
        static_assert(detail::is_valid_type<T,Types...>::value, "Not a valid type for this variant");
        new (&data) T(val);
    }

    template <typename T>
    VARIANT_INLINE variant(T && val) noexcept
        : type_id(detail::type_traits<T,Types...>::id)
    {
        static_assert(detail::is_valid_type<T,Types...>::value, "Not a valid type for this variant");
        new (&data) T(std::forward<T>(val)); // nothrow
    }

    VARIANT_INLINE variant(variant<Types...> const& old)
        : type_id(old.type_id)
    {
        helper_t::copy(old.type_id, &old.data, &data);
    }

    VARIANT_INLINE variant(variant<Types...>&& old) noexcept
        : type_id(old.type_id)
    {
        helper_t::move(old.type_id, &old.data, &data);
    }

    VARIANT_INLINE variant<Types...>& operator= (variant<Types...> old)
    {
        std::swap(type_id, old.type_id);
        std::swap(data, old.data);
        return *this;
    }

    template<typename T>
    VARIANT_INLINE void is()
    {
        return (type_id == detail::type_traits<T, Types...>::id);
    }

    VARIANT_INLINE void valid()
    {
        return (type_id != detail::invalid_value);
    }

    template<typename T, typename... Args>
    VARIANT_INLINE void set(Args&&... args)
    {
        helper_t::destroy(type_id, &data);
        new (&data) T(std::forward<Args>(args)...);
        type_id = detail::type_traits<T,Types...>::id;
    }

    template<typename T>
    VARIANT_INLINE T& get()
    {
        if (type_id == detail::type_traits<T,Types...>::id)
        {
            return *reinterpret_cast<T*>(&data);
        }
        else
        {
            throw std::runtime_error("in get()");
        }
    }

    template<typename T>
    VARIANT_INLINE T const& get() const
    {
        if (type_id == detail::type_traits<T,Types...>::id)
        {
            return *reinterpret_cast<T const*>(&data);
        }
        else
        {
            throw std::runtime_error("in get()");
        }
    }

    VARIANT_INLINE std::size_t get_type_id() const
    {
        return type_id;
    }

    // visitor
    template <typename F, typename V>
    typename std::result_of<F(V const&)>::type
    VARIANT_INLINE static visit(V const& v, F f)
    {
        return detail::dispatcher<F, V, Types...>::apply(v, f);
    }

    ~variant() noexcept
    {
        helper_t::destroy(type_id, &data);
    }
};

// unary visitor interface
template <typename V, typename F>
typename std::result_of<F(V const&)>::type
VARIANT_INLINE static apply_visitor(V const& v, F f)
{
    return V::visit(v,f);
}

}

#endif  // UTIL_VARIANT_HPP
