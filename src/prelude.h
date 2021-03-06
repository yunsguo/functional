/*
*	prelude.h:
*   emulating the Haskell prelude
*   Language: C++, Visual Studio 2017
*   Platform: Windows 10 Pro
*   Application: recreational
*   Author: Yunsheng Guo, yguo125@syr.edu
*/


/*
*
*   Package Operations:
*	defines some convenient aliases
*	includes type class implenmetations, Maybe type and its type traits implenmentations
*	some static operators for type classes
*	pattern matching snytax
*
*   Public Interface:
*	check Haskell prelude for futher info
*	Eq, Ord, Show all have their operator implenmeted
*	Functor fmap is implenmented
*	Applicative has the same partial apply operator as function type
*	Monad has a unique operator >>= for sequence and operator >> for compose
*	due to the order needed to achieve do Haskell notation
*	Monad is no longer an Applicative
*
*   Build Process:
*   requires function.h and variant.h
*
*   Maintenance History:
*   June 6
*	first draft contains typeclass definitions and function application definition
*   August 12
*   refactor. move Maybe type into this for clarity and move all type class and traits declaration to meta.h
*	change pattern matching syntax and let pattern matching use Maybe
*	August 25
*	final review for publish
*
*
*/

#pragma once
#ifndef _PRELUDE_
#define _PRELUDE_

#include <string>
#include "function.h"
#include "variant.h"

namespace details
{
	//return the index of the first occurance of a type which is equvenlent of an compile time boolean true
	template<typename ...as>
	struct first_true_index;

	template<typename a>
	struct first_true_index<a> { enum { value = a::value ? 0 : 1 }; };

	template<typename a, typename ...as>
	struct first_true_index<a, as...> { enum { value = a::value ? 0 : 1 + first_true_index<as...>::value }; };

	template<std::size_t... Ns, typename... Ts>
	auto tail_impl(std::index_sequence<Ns...>, std::tuple<Ts...>&& t)
	{
		return  std::make_tuple(std::get<Ns + 1u>(t)...);
	}

	//return the last type of a tuple
	template <typename... Ts >
	auto tail(std::tuple<Ts...>&& t)
	{
		return  tail_impl(std::make_index_sequence<sizeof...(Ts) - 1u>(), std::forward<std::tuple<Ts...>>(t));
	}

	//return true if list2 is a legal form of list1 variant wise(does not have to match each element). 
	template<typename list1, typename list2>
	struct are_legal;

	template<typename a, typename b, typename c, typename d>
	struct are_legal<TMP::Cons<a, b>, TMP::Cons<c, d>> :public std::bool_constant<fcl::variant_traits<a>::elem<c>::value && are_legal<b, d>::value> {};

	template<>
	struct are_legal<TMP::Nil, TMP::Nil> :public std::true_type {};

	template<typename a, typename b>
	struct are_legal<TMP::Cons<a, b>, TMP::Nil> :public std::false_type {};

	template<typename a, typename b>
	struct are_legal<TMP::Nil, TMP::Cons<a, b>> :public std::false_type {};

	//return true if all types are of type class Show
	template<typename a, typename ...as>
	struct are_show;

	template<typename a>
	struct are_show<a> :public std::bool_constant<fcl::Show<a>::pertain::value> {};

	template<typename a, typename ...as>
	struct are_show :public std::bool_constant<fcl::Show<a>::pertain::value && are_show<as...>::value> {};

	template<typename a, typename...as>
	struct pattern
	{
		//runtime check if parameters are of matched type
		template<typename b, typename ...bs>
		static bool match(a first, as...args) { return variant_traits<a>::template is_of<b>(first) && pattern<as...>::template match<bs...>(args...); }

		//extract parameters in the template form as a tuple
		template<typename b, typename ...bs>
		static std::tuple<b, bs...> gets(a first, as...args) { return std::tuple_cat(std::make_tuple<b>(variant_traits<a>::template move<b>(first)), pattern<as...>::template gets<bs...>(args...)); }
	};

	template<typename a>
	struct pattern<a>
	{
		//runtime check if the parameter is of matched type
		template<typename b>
		static bool match(a first) { return variant_traits<a>::template is_of<b>(first); }

		//extract the parameter in the template form as a tuple
		template<typename b>
		static std::tuple<b> gets(a first) { return std::make_tuple<b>(variant_traits<a>::template move<b>(first)); }
	};
}

namespace fcl
{

	template<typename a, typename b, typename ...rest>
	using data = variant<a, b, rest...>;

	//generic list haskell conter-part(double linked list)
	template<typename a>
	using list = std::list<a>;

	//generic list cons method
	template<typename a>
	list<a> cons(a first, list<a> as)
	{
		as.push_front(first);
		return as;
	}

	//generic list uncons method
	template<typename a>
	pair<a, list<a>> uncons(list<a> as)
	{
		a f = as.front();
		as.pop_front();
		return std::make_pair<a, list<a>>(std::move(f), std::move(as));
	}

	template<typename a>
	a id(a value) { return value; }

	template<typename a>
	struct Eq
	{
		using pertain = std::bool_constant<std::is_arithmetic<a>::value || Ord<a>::pertain::value>;

		template<typename = std::enable_if_t<std::is_arithmetic<a>::value>>
		static bool equals(const a& one, const a& other) { return one == other; }

		template<typename = std::enable_if_t<!std::is_arithmetic<a>::value && Ord<a>::pertain::value>, size_t = 0>
		static bool equals(const a& one, const a& other) { return Ord<a>::compare(one, other) == Ordering::EQ; }
	};

	template<typename a, typename = std::enable_if_t<Eq<a>::pertain::value && !std::is_arithmetic<a>::value>>
	bool operator==(const a& one, const a& other) { return Eq<a>::equals(one, other); }

	template<typename a, typename = std::enable_if_t<Eq<a>::pertain::value && !std::is_arithmetic<a>::value>>
	bool operator!=(const a& one, const a& other) { return !Eq<a>::equals(one, other); }

	template<typename a>
	struct Ord
	{
		using pertain = std::is_arithmetic<a>;
		static Ordering compare(const a& one, const a& other)
		{
			if (one > other)return Ordering::GT;
			if (one == other) return Ordering::EQ;
			return Ordering::LT;
		}
	};

	template<typename a, typename = std::enable_if_t<Ord<a>::pertain::value>>
	bool operator<=(const a& one, const a& other) { return Ord<a>::compare(one, other) != Ordering::GT; }

	template<typename a, typename = std::enable_if_t<Ord<a>::pertain::value>>
	bool operator>=(const a& one, const a& other) { return Ord<a>::compare(one, other) != Ordering::LT; }

	template<typename a, typename = std::enable_if_t<Ord<a>::pertain::value>>
	bool operator<(const a& one, const a& other) { return Ord<a>::compare(one, other) == Ordering::LT; }

	template<typename a, typename = std::enable_if_t<Ord<a>::pertain::value>>
	bool operator>(const a& one, const a& other) { return Ord<a>::compare(one, other) == Ordering::GT; }

	template<typename a>
	struct Show
	{
		using pertain = std::is_arithmetic<a>;

		template<typename = std::enable_if_t<pertain::value>>
		static std::string show(const a& value) { return std::to_string(value); }
	};

#ifdef _IOSTREAM_
	template<typename a, typename = std::enable_if_t<Show<a>::pertain::value && !std::is_same<a, std::string>::value && !std::is_arithmetic<a>::value>>
	std::ostream& operator<<(std::ostream& out, const a& value) { return out << Show<a>::show(value); }
#endif

	template<typename a>
	struct Monoid {};

	template<template<typename> typename I>
	struct Injector
	{
		using pertain = std::false_type;

		template<typename a>
		static I<a> pure(a&&);
	};

	template<template<typename> typename F>
	struct Functor
	{
		using pertain = std::false_type;

		template<typename f, typename = std::enable_if_t<is_function<f>::value>>
		static F<applied_type<f>> fmap(const f& func, F<head_parameter<f>>&& fa);

		template<typename f, typename = std::enable_if_t<is_function<f>::value>>
		static F<monadic_applied_type<f>> monadic_fmap(const f& func, F<last_parameter<f>>&& fa);
	};

	template<template<typename> typename F, typename f, typename = std::enable_if_t<Functor<F>::pertain::value && is_function<f>::value>>
	F<applied_type<f>> operator<<=(f func, F<head_parameter<f>> ff)
	{
		return Functor<F>::template fmap<f>(std::move(func), std::move(ff));
	}

	template<template<typename> typename F, typename f, typename = std::enable_if_t<Functor<F>::pertain::value && is_function<f>::value>>
	F<monadic_applied_type<f>> operator>>=(F<last_parameter<f>> ff, f func)
	{
		return Functor<F>::template monadic_fmap<f>(std::move(func), std::move(ff));
	}

	template<template<typename> typename A>
	struct Applicative
	{
		using pertain = std::false_type;

		template<typename f, typename = std::enable_if_t<is_function<f>::value>>
		static A<applied_type<f>> sequence(A<f>&&, A<head_parameter<f>>&&);
	};

	template<template<typename> typename A, typename f, typename = std::enable_if_t<Applicative<A>::pertain::value && is_function<f>::value>>
	A<applied_type<f>> operator<<=(A<f> af, A<head_parameter<f>> aa)
	{
		return Applicative<A>::template sequence<f>(std::move(af), std::move(aa));
	}

	template<template<typename> typename A>
	struct Alternative
	{
		using pertain = std::false_type;

		template<typename a>
		static A<a> empty();

		template<typename a>
		static A<a> alter(A<a>&&, A<a>&&);
	};

	template<template<typename> typename A, typename a, typename = std::enable_if_t<Alternative<A>::pertain::value>>
	A<a> operator||(A<a> p, A<a> q)
	{
		return Alternative<A>::alter(std::move(p), std::move(q));
	}

	template<template<typename> typename M>
	struct Monad
	{
		using pertain = std::false_type;

		template<typename f, typename = std::enable_if_t<is_function<f>::value>>
		static M<monadic_applied_type<f>> sequence(M<f>&&, M<last_parameter<f>>&&);

		template<typename a, typename b>
		static M<b> compose(M<a>&&, M<b>&&);
	};

	template<template<typename> typename M, typename f, typename = std::enable_if_t<is_function<f>::value && Monad<M>::pertain::value>>
	M<monadic_applied_type<f>> operator>>=(M<last_parameter<f>> ma, M<f> func)
	{
		return Monad<M>::template sequence<f>(std::move(func), std::move(ma));
	}

	template<typename a, template<typename> typename M, typename f, typename = std::enable_if_t<is_function<f>::value && Monad<M>::pertain::value && std::is_convertible<a, M<last_parameter<f>>>::value>>
	M<monadic_applied_type<f>> operator>>=(a ma, M<f> func)
	{
		const static auto convert = [](a var)->M<last_parameter<f>> {return var; };
		return Monad<M>::template sequence<f>(std::move(func), convert(ma));
	}

	template<template<typename> typename M, typename a, typename b, typename = std::enable_if_t<Monad<M>::pertain::value>>
	M<b> operator>>(M<a> p, M<b> q)
	{
		return Monad<M>::compose(std::move(p), std::move(q));
	}

	template<template<typename> typename M, typename a, typename f, typename = std::enable_if_t<is_function<f>::value && Monad<M>::pertain::value>>
	M<f> operator>>(M<a> p, f q)
	{
		return Monad<M>::compose(std::move(p), Injector<M>::template pure<f>(std::move(q)));
	}

	template<typename a>
	struct Just
	{
		a value;
	};

	struct Nothing {};

	//maybe implenmentation
	template<typename a>
	struct Maybe
	{
		Maybe() :value(Nothing()) {}

		Maybe(Just<a> just) :value(just) {}
		Maybe(Nothing n) :value(n) {}

		Maybe(a v_) :value(Just<a>{v_}) {}

		friend variant_traits<Maybe<a>>;

	private:
		data<Nothing, Just<a>> value;
	};

	template<typename a, typename b>
	b maybe(b default_r, const function<b, a>& f, const Maybe<a>& ma)
	{
		if (isNothing(ma)) return default_r;
		return f(fromJust(ma));
	}

	template<typename a>
	bool isJust(const Maybe<a>& ma) { return variant_traits<Maybe<a>>::template is_of<Just<a>>(ma); }

	template<typename a>
	bool isNothing(const Maybe<a>& ma) { return variant_traits<Maybe<a>>::template is_of<Nothing>(ma); }

	template<typename a>
	a fromJust(const Maybe<a>& ma) { return variant_traits<Maybe<a>>::template get<Just<a>>(ma).value; }

	template<typename a>
	a fromJust(Maybe<a>&& ma) { return variant_traits<Maybe<a>>::template move<Just<a>>(ma).value; }

	template<typename a>
	a fromMaybe(a default_r, const Maybe<a>& ma) { isNothing(ma) ? default_r : variant_traits<Maybe<a>>::template get<Just<a>>(ma).value; }

	//Maybe variant traits implenmentation
	template<typename a>
	struct variant_traits<Maybe<a>>
	{
		using possess = std::true_type;
		using has_default = std::true_type;
		using default_type = a;

		using def = Maybe<a>;

		template<typename b>
		using elem = TMP::elem<TMP::list<Nothing, Just<a>>, b>;

		using D = data<Nothing, Just<a>>;

		template<typename b>
		static bool is_of(const def& maybe) { return variant_traits<D>::template is_of<b>(maybe.value); }

		template<typename b>
		static b&& move(def& maybe) { return variant_traits<D>::template move<b>(maybe.value); }

		template<typename b>
		static const b& get(const def& maybe) { return variant_traits<D>::template get<b>(maybe.value); }
	};

	//Maybe Eq typeclass implenmentation
	template<typename a>
	struct Eq<Maybe<a>>
	{
		using pertain = typename Eq<a>::pertain;

		template<typename = std::enable_if_t<pertain::value>>
		static bool equals(const Maybe<a>& one, const  Maybe<a>& other)
		{
			if (isJust(one) && isJust(other)) return Eq<a>::equals(fromJust(one), fromJust(other));
			if (isNothing(one) && isNothing(other)) return true;
			return false;
		}
	};

	//Maybe Ord typeclass implenmentation
	template<typename a>
	struct Ord<Maybe<a>>
	{
		using pertain = typename Ord<a>::pertain;

		template<typename = std::enable_if_t<pertain::value>>
		static Ordering compare(const Maybe<a>& one, const Maybe<a>& other)
		{
			if (isJust(one))
			{
				if (isJust(other))return Ord<a>::compare(fromJust(one), fromJust(other));
				return Ordering::GT;
			}
			if (isNothing(other))return Ordering::EQ;
			return Ordering::LT;
		}
	};

	//Maybe Show typeclass implenmentation
	template<typename a>
	struct Show<Maybe<a>>
	{
		using pertain = std::true_type;

		static std::string show(const Maybe<a>& value)
		{
			static_assert(Show<a>::pertain::value, "Maybe<a> is not of Show because a is not of Show.");
			if (isNothing(value)) return "Nothing";
			return "Just " + Show<a>::show(variant_traits<Maybe<a>>::template get<Just<a>>(value).value);
		}
	};

	//Maybe Injector typeclass implenmentation
	template<>
	struct Injector<Maybe>
	{
		using pertain = std::true_type;

		template<typename a>
		static Maybe<a> pure(a&& ma) { return Maybe<a>(ma); }

	};

	//Maybe Functor typeclass implenmentation
	template<>
	struct Functor<Maybe>
	{
		using pertain = std::true_type;

		template<typename f, typename = std::enable_if_t<is_function<f>::value>>
		static Maybe<applied_type<f>> fmap(const f& func, Maybe<head_parameter<f>>&& ma)
		{
			if (isNothing(ma)) return Nothing();
			return function_traits<f>::apply(func, fromJust(std::forward<Maybe<head_parameter<f>>>(ma)));
		}

		template<typename f, typename = std::enable_if_t<is_function<f>::value>>
		static Maybe<monadic_applied_type<f>> monadic_fmap(const f& func, Maybe<last_parameter<f>>&& ma)
		{
			if (isNothing(ma)) return Nothing();
			return function_traits<f>::monadic_apply(func, fromJust(std::forward<Maybe<last_parameter<f>>>(ma)));
		}
	};

	//Maybe Applicative typeclass implenmentation
	template<>
	struct Applicative<Maybe>
	{
		using pertain = std::true_type;

		template<typename f, typename = std::enable_if_t<is_function<f>::value>>
		static Maybe<applied_type<f>> sequence(Maybe<f>&& mfunc, Maybe<head_parameter<f>>&& ma)
		{
			if (isNothing(mfunc) || isNothing(ma)) return Nothing();
			return function_traits<f>::apply(fromJust(std::forward<Maybe<f>>(mfunc)), fromJust(std::forward<Maybe<head_parameter<f>>>(ma)));
		}
	};

	//Maybe Alternative typeclass implenmentation
	template<>
	struct Alternative<Maybe>
	{
		using pertain = std::true_type;

		template<typename a>
		static Maybe<a> empty() { return Nothing(); }

		template<typename a>
		static Maybe<a> alter(Maybe<a>&& p, Maybe<a>&& q) { if (isJust(p)) return p; return q; }
	};

	//Maybe Monad typeclass implenmentation
	template<>
	struct Monad<Maybe>
	{
		using pertain = std::true_type;

		template<typename f, typename = std::enable_if_t<is_function<f>::value>>
		static Maybe<monadic_applied_type<f>> sequence(Maybe<f>&& mfunc, Maybe<last_parameter<f>>&& ma)
		{
			if (isNothing(ma) || isNothing(mfunc)) return Nothing();
			return function_traits<f>::monadic_apply(fromJust(std::forward<Maybe<f>>(mfunc)), fromJust(std::forward<Maybe<last_parameter<f>>>(ma)));
		}

		template<typename a, typename b>
		static Maybe<b> compose(Maybe<a>&& p, Maybe<b>&& q)
		{
			return q;
		}
	};

	//Show typeclass common implenmentations

	template<>
	struct Show<na>
	{
		using pertain = std::true_type;

		static std::string show(const na& value) { return "()"; }
	};

	template<typename a>
	struct Show<tuple<a>>
	{
		using pertain = std::true_type;

		static std::string content(tuple<a> value)
		{
			static_assert(Show<a>::pertain::value, "tuple<a> is not of Show because a is not of Show.");
			return Show<a>::show(std::get<0>(value));
		}

		static std::string show(const tuple<a>& value)
		{
			static_assert(Show<a>::pertain::value, "tuple<a> is not of Show because a is not of Show.");
			return "(" + content(value) + ")";
		}
	};

	template<typename a, typename b>
	struct Show<pair<a, b>>
	{
		using pertain = std::true_type;

		static std::string show(const pair<a, b>& value)
		{
			static_assert(details::are_show<a, b>::value, "Pair<a,b> is not of Show because a or b are not all of Show.");
			return "(" + Show<a>::show(value.first) + ", " + Show<b>::show(value.second) + ")";
		}
	};

	template<typename a, typename ...as>
	struct Show<tuple<a, as...>>
	{
		using pertain = std::true_type;
		static std::string content(tuple<a, as...> value)
		{
			static_assert(details::are_show<a, as...>::value, "tuple<a,as...> is not of Show because a,as... are not all of Show.");
			auto first = Show<a>::show(std::get<0>(value));
			return first + ", " + Show<tuple<as...>>::content(details::tail(std::move(value)));
		}

		static std::string show(const tuple<a, as...>& value)
		{
			static_assert(details::are_show<a, as...>::value, "tuple<a,as...> is not of Show because a,as... are not all of Show.");
			return "(" + content(value) + ")";
		}
	};

	template<>
	struct Show<char>
	{
		using pertain = std::true_type;
		static std::string show(const char& value) { return "\'" + value + '\''; }
	};

	template<>
	struct Show<std::string>
	{
		using pertain = std::true_type;
		static std::string show(const std::string& value) { return '\"' + value + '\"'; }
	};

	template<typename a, typename b, typename ...rest>
	struct Show<variant<a, b, rest...>>
	{
		using pertain = std::true_type;
		using V = variant<a, b, rest...>;

		template<size_t I, typename = std::enable_if_t<I == TMP::length<V>::value>>
		static std::string show_impl(const V& value)
		{
			static_assert(details::are_show<a, b, rest...>::value, "variant<a, b, rest...> is not of Show because a, b, rest... are not all of Show.");
			throw std::exception("error: type mismatch.");
		}

		template<size_t I, typename = std::enable_if_t<I != TMP::length<V>::value>, size_t = 0>
		static std::string show_impl(const V& value)
		{
			static_assert(details::are_show<a, b, rest...>::value, "variant<a, b, rest...> is not of Show because a, b, rest... are not all of Show.");
			using T = typename TMP::index<V, I>::type;
			if (variant_traits<V>::template is_of<T>(value))
				return util::type<T>::infer() + " " + Show<T>::show(variant_traits<V>::template get<T>(value));
			return show_impl<I + 1>(value);
		}

		static std::string show(const V& value)
		{
			static_assert(details::are_show<a, b, rest...>::value, "variant<a, b, rest...> is not of Show because a, b, rest... are not all of Show.");
			return show_impl<0>(value);
		}
	};

	template<typename a>
	struct Show<list<a>>
	{
		using pertain = std::true_type;

		static std::string show(const list<a>& value)
		{
			static_assert(Show<a>::pertain::value, "list<a> is not of Show because a is not of Show.");
			if (value.size() == 0) return "[]";
			if (value.size() == 1)
				return "[" + Show<a>::show(value.front()) + "]";
			std::string buffer = "[";
			auto it = value.cbegin();
			buffer += Show<a>::show(*it);
			it++;
			while (it != value.cend())
			{
				buffer += "," + Show<a>::show(*it);
				it++;
			}
			return buffer + "]";
		}
	};

	//pattern matching syntax
	template<typename r>
	struct of
	{
		template<typename ...as>
		struct pattern
		{
			using tuples = std::tuple<as...>;

			pattern(as...arg) :var_(std::make_tuple(arg...)), r_() {}

			template<typename...bs>
			pattern<as...>& match(r result)
			{
				static_assert(details::are_legal<TMP::list<as...>, TMP::list<bs...>>::value, "error: type mismatch.");
				if (isJust(r_)) return *this;
				if (std::apply(details::pattern<as...>::template match<bs...>, var_))
					r_ = result;
				return *this;
			}

			template<typename...bs>
			pattern<as...>& match(const function<r, bs...>& f)
			{
				static_assert(details::are_legal<TMP::list<as...>, TMP::list<bs...>>::value, "error: type mismatch.");
				if (isJust(r_)) return *this;
				if (std::apply(details::pattern<as...>::template match<bs...>, var_))
					r_ = std::apply(f, std::apply(details::pattern<as...>::template gets<bs...>, std::move(var_)));
				return *this;
			}

			operator r()
			{
				if (isJust(r_)) return fromJust(std::move(r_));
				throw std::exception("error: pattern exhausted without match.");
			}

		private:
			tuples var_;
			Maybe<r> r_;
		};

		template<>
		struct pattern<>;

		template<typename a>
		struct pattern<a>
		{
			pattern(a var) :var_(var), r_() { static_assert(variant_traits<a>::possess::value, "type has no variant traits."); }

			template<typename b>
			pattern& match(r result)
			{
				static_assert(variant_traits<a>::template elem<b>::value, "error: type mismatch.");
				if (isJust(r_)) return *this;
				if (variant_traits<a>::template is_of<b>(var_))
					r_ = result;
				return *this;
			}

			template<typename b>
			pattern& match(const function<r, b>& f)
			{
				static_assert(variant_traits<a>::template elem<b>::value, "error: type mismatch.");
				if (isJust(r_)) return *this;
				if (variant_traits<a>::template is_of<b>(var_))
					r_ = std::apply(f,var_);
				return *this;
			}

			template<template<typename> typename ctor, typename = std::enable_if_t<variant_traits<a>::has_default::value>>
			pattern& match(r result)
			{
				using b = ctor<typename variant_traits<a>::default_type>;
				return match<b>(result);
			}

			template<template<typename> typename ctor, typename = std::enable_if_t<variant_traits<a>::has_default::value>>
			pattern& match(const function<r, ctor<typename variant_traits<a>::default_type>>& f)
			{
				using b = ctor<typename variant_traits<a>::default_type>;
				return match<b>(f);
			}

			operator r()
			{
				if (isJust(r_)) return fromJust(std::move(r_));
				throw std::exception("error: pattern exhausted without match.");
			}


		private:
			a var_;
			Maybe<r> r_;
		};

	};
}

//type inference for Maybes
namespace util
{
	template<typename a>
	struct type<fcl::Maybe<a>> { static std::string infer() { return "Maybe " + type<a>::infer(); } };

	template<typename a>
	struct type<fcl::Just<a>> { static std::string infer() { return "Just " + type<a>::infer(); } };

	template<>
	struct type<fcl::Nothing> { static std::string infer() { return "Nothing"; } };
}

#endif