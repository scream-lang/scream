#include "lauxlib.h"
#include "hellolib.h"

#ifdef HELLO_USE_SOUP

#include <soup/string.hpp>
#include <soup/base58.hpp>

static int decode(hello_State* L) {
	try {
		hello_pushstring(L, soup::string::bin2hex(soup::base58::decode(helloL_checkstring(L, 1))).c_str());
	}
	catch (...) {
		helloL_error(L, "Attempted to decode non-base58 string.");
	}
	return 1;
}

static int is_valid(hello_State* L) {
	try {
		const auto discarding = soup::base58::decode(helloL_checkstring(L, 1));
		hello_pushboolean(L, true);
	}
	catch (...) {
		hello_pushboolean(L, false);
	}
	return 1;
}

static const helloL_Reg funcs[] = {
	{"is_valid", is_valid},
	{"decode", decode},
	{nullptr, nullptr}
};

HELLOMOD_API int helloopen_base58(hello_State* L) {
	helloL_newlib(L, funcs);
	return 1;
}

#endif