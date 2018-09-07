#include "JSON.h"
#include <iostream>

using namespace JSON;
using namespace fcl;

fcl::Reaction<Null> JSON::null(std::string inp)
{
	const static auto M =
		fcl::string("null") >>
		Null()
		;
	return fcl::parse(M, inp);
}

fcl::Reaction<True> JSON::bool_true(std::string inp)
{
	const static auto M =
		fcl::string("true") >>
		True()
		;
	return fcl::parse(M, inp);
}

fcl::Reaction<False> JSON::bool_false(std::string inp)
{
	const static auto M =
		fcl::string("false") >>
		False()
		;
	return fcl::parse(M, inp);
}

fcl::Reaction<std::string> JSON::integer(std::string inp)
{
	const static auto M =
		maybe_one(fcl::character('-')) >>=
		(
			fcl::string("0") || (
				digit19 >>=
				any(digit) >>=
				Function<std::string, char, std::string>(cons_str)
				)) >>=
		Function<std::string, std::string, std::string>(concat_str);

	return fcl::parse(M, inp);
}

fcl::Reaction<std::string> JSON::decimal(std::string inp)
{
	const static auto M =
		(fcl::character('.') >>=
			some(digit) >>=
			Function<std::string, char, std::string>(cons_str)) ||
		Monad<Parser>::pure<std::string>("");

	return fcl::parse(M, inp);
}

fcl::Reaction<std::string> JSON::exponent(std::string inp)
{
	const static auto M =
		((fcl::character('e') || fcl::character('E')) >>=
			maybe_one(sat([](char c)->bool {return c == '+' || c == '-'; })) >>=
			some(digit) >>=
			Function<std::string, char, std::string, std::string>
			([](char e, std::string p, std::string q)->std::string { return e + p + q; })) ||
		Monad<Parser>::pure<std::string>("");

	return fcl::parse(M, inp);
}

fcl::Reaction<Number> JSON::number(std::string inp)
{
	const static auto M =
		JSON::integer >>=
		JSON::decimal >>=
		JSON::exponent >>=
		Monad<Parser>::pure<Function<std::string, std::string, std::string, std::string>>
		([](std::string s1, std::string s2, std::string s3)->std::string {return s1 + s2 + s3; });

	auto reaction = parse(M, inp);
	if (isNothing(reaction)) return Nothing();
	auto pair = fromJust(reaction);
	return std::make_pair(Number{ std::stof(pair.first) }, std::move(pair.second));
}

char32_t JSON::char_to_uni(char c) { return c; }

char32_t JSON::unicode(char c1, char c2, char c3, char c4) { return strtoul("" + c1 + c2 + c3 + c4, 0, 16); }

fcl::Reaction<char32_t> JSON::character(std::string inp)
{
	const static Parser<char> any_except = fcl::sat
	([](char c)->bool {return c != '\"' && c != '\\' && iscntrl(c) == 0; });

	const static Parser<char> special = fcl::sat
	([](char c)->bool {return c == '\"' || c == '\\' || c == '/' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t'; });

	const static auto C2U = Monad<Parser>::pure<Function<char32_t, char>>(char_to_uni);

	const static Parser<char> hex = fcl::hexdecimal;

	const static auto UNI = Monad<Parser>::pure<Function<char32_t, char, char, char, char>>(unicode);

	const static auto M =
		(any_except >>=
			C2U) ||
			(fcl::character('\\') >>
		((special >>=
			C2U) ||
			(fcl::character('u') >>
				hex >>=
				hex >>=
				hex >>=
				hex >>=
				UNI)));

	return parse(M, inp);
}

fcl::Reaction<String> JSON::string(std::string inp)
{
	const static auto M =
		fcl::character('\"') >>
		fcl::any<char32_t>(JSON::character) >>=
		fcl::character('\"') >>
		Function<fcl::List<char32_t>, fcl::List<char32_t>>(fcl::id<fcl::List<char32_t>>);
	auto reaction = parse(M, inp);
	if (isNothing(reaction)) return Nothing();
	auto pair = fromJust(reaction);
	return std::make_pair<String, std::string>(String{ std::move(pair.first) }, std::move(pair.second));
}

fcl::Reaction<Value> JSON::value(std::string inp)
{
	const static auto M =
		(Parser<String>(JSON::string) >>=
			Monad<Parser>::pure<Function<Value, String>>(JSON::boxing<String>))
		||
		(Parser<Number>(JSON::number) >>=
			Monad<Parser>::pure<Function<Value, Number>>(JSON::boxing<Number>))
		||
		(Parser<Object>(JSON::object) >>=
			Monad<Parser>::pure<Function<Value, Object>>(JSON::boxing<Object>))
		||
		(Parser<Array>(JSON::array) >>=
			Monad<Parser>::pure<Function<Value, Array>>(JSON::boxing<Array>))
		||
		(Parser<True>(JSON::bool_true) >>=
			Monad<Parser>::pure<Function<Value, True>>(JSON::boxing<True>))
		||
		(Parser<False>(JSON::bool_false) >>=
			Monad<Parser>::pure<Function<Value, False>>(JSON::boxing<False>))
		||
		(Parser<Null>(JSON::null) >>=
			Monad<Parser>::pure<Function<Value, Null>>(JSON::boxing<Null>));

	return parse(M, inp);
}

fcl::Reaction<Array> JSON::array(std::string inp)
{
	const static auto M =
		fcl::character('[') >>
		fcl::any<Value>(
			JSON::value >>=
			fcl::maybe_one(fcl::character(',')) >>
			Function<Value, Value>(fcl::id<Value>)
			) >>=
		fcl::character(']') >>
		Function<fcl::List<Value>, fcl::List<Value>>(fcl::id<fcl::List<Value>>);
	auto reaction = parse(M, inp);
	if (isNothing(reaction)) return Nothing();
	auto pair = fromJust(reaction);

	return std::make_pair<Array, std::string>(Array{ std::move(pair.first) }, std::move(pair.second));
}

Pair<String, Value> JSON::pair(String str, Value val)
{
	return std::make_pair<String, Value>(std::move(str), std::move(val));
}

fcl::Reaction<Object> JSON::object(std::string inp)
{
	const static auto M =
		fcl::character('{') >>
		any<Pair<String, Value>>(
			Parser<String>(JSON::string) >>=
			fcl::character(':') >>
			Parser<Value>(JSON::value) >>=
			fcl::maybe_one(fcl::character(',')) >>
			Monad<Parser>::pure<Function<Pair<String, Value>, String, Value>>(JSON::pair)
			) >>=
		fcl::character('}') >>
		Monad<Parser>::pure<Function<fcl::List<Pair<String, Value>>, fcl::List<Pair<String, Value>>>>(fcl::id<fcl::List<Pair<String, Value>>>);

	auto reaction = parse(M, inp);
	if (isNothing(reaction)) return Nothing();
	auto pair = fromJust(reaction);

	return std::make_pair<Object, std::string>(Object{ std::move(pair.first) }, std::move(pair.second));
}

//type inference for JSON types
template<>
struct util::type<JSON::True> { static std::string infer() { return "True"; } };

template<>
struct util::type<JSON::False> { static std::string infer() { return "False"; } };

template<>
struct util::type<JSON::Null> { static std::string infer() { return "Null"; } };

template<>
struct util::type<JSON::String> { static std::string infer() { return "String"; } };

template<>
struct util::type<JSON::Number> { static std::string infer() { return "Number"; } };

template<>
struct util::type<JSON::Array> { static std::string infer() { return "Array"; } };

template<>
struct util::type<JSON::Object> { static std::string infer() { return "Object"; } };

template<>
struct util::type<JSON::Value> { static std::string infer() { return "Value"; } };

// JSON Show implenmentations
template<>
struct fcl::Show<JSON::True>
{
	using pertain = std::true_type;
	inline static std::string show(const JSON::True& value) { return "true"; }
};

template<>
struct fcl::Show<JSON::False>
{
	using pertain = std::true_type;
	inline static std::string show(const JSON::False& value) { return "false"; }
};

template<>
struct fcl::Show<JSON::Null>
{
	using pertain = std::true_type;
	inline static std::string show(const JSON::Null& value) { return "null"; }
};

template<>
struct fcl::Show<JSON::String>
{
	using pertain = std::true_type;
	inline static std::string show(const JSON::String& value) { return fcl::Show<fcl::List<char32_t>>::show(value.value); }
};

template<>
struct fcl::Show<JSON::Number>
{
	using pertain = std::true_type;
	inline static std::string show(const JSON::Number& value) { return std::to_string(value.value); }
};

template<>
struct fcl::Show<JSON::Value>
{
	using pertain = std::true_type;
	static std::string show(const JSON::Value& value);
};

template<>
struct fcl::Show<JSON::Array>
{
	using pertain = std::true_type;
	inline static std::string show(const JSON::Array& value);
};

template<>
struct fcl::Show<JSON::Object>
{
	using pertain = std::true_type;
	inline static std::string show(const JSON::Object& value);
};

std::string fcl::Show<JSON::Value>::show(const JSON::Value & value) { return fcl::Show<JSON::VD>::show(value.value); }

std::string fcl::Show<JSON::Array>::show(const JSON::Array & value) { return fcl::Show<fcl::List<JSON::Value>>::show(value.value); }

std::string fcl::Show<JSON::Object>::show(const JSON::Object & value) { return fcl::Show<fcl::List<Pair<JSON::String, JSON::Value>>>::show(value.value); }

#ifdef JSON_CPP

template<typename a>
void display_parse(Parser<a> p, std::string str)
{
	std::cout << "parse \"" << str << "\" as " << util::type<a>::infer() << ": " << std::endl << parse(p, str) << std::endl << std::endl;
}


int main()
{
	display_parse<False>(bool_false, "false");
	display_parse<True>(bool_true, "true");
	display_parse<Null>(null, "null");
	display_parse<Number>(number, "686.97 365.24");
	display_parse<Number>(number, "null");
	display_parse<String>(JSON::string, "\"null\"");
	display_parse<Array>(JSON::array, "[\"Ford\",\"BMW\",\"Fiat\"]");
	display_parse<Array>(JSON::array, "[]");
	display_parse<Object>(JSON::object, "{\"name\":\"John\",\"age\":30,\"cars\":[\"Ford\",\"BMW\",\"Fiat\"]}");
	display_parse<Object>(JSON::object, "{}");
	display_parse<Value>(JSON::value, "\"null\"");
	display_parse<Value>(JSON::value, "false");
	display_parse<Value>(JSON::value, "true");
	display_parse<Value>(JSON::value, "null");
	display_parse<Value>(JSON::value, "686.97");
	display_parse<Value>(JSON::value, "{\"glossary\":{\"title\":\"exampleglossary\",\"GlossDiv\":{\"title\":\"S\",\"GlossList\":{\"GlossEntry\":{\"ID\":\"SGML\",\"SortAs\":\"SGML\",\"GlossTerm\":\"StandardGeneralizedMarkupLanguage\",\"Acronym\":\"SGML\",\"Abbrev\":\"ISO8879:1986\",\"GlossDef\":{\"para\":\"Ameta-markuplanguage,usedtocreatemarkuplanguagessuchasDocBook.\",\"GlossSeeAlso\":[\"GML\",\"XML\"]},\"GlossSee\":\"markup\"}}}}}");

}

#endif