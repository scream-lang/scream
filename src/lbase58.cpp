#include "lauxlib.h"
#include "masklib.h"

#ifdef MASK_USE_SOUP

#include <soup/string.hpp>
#include <soup/base58.hpp>

static int decode(mask_State* L) {
	try {
		mask_pushstring(L, soup::string::bin2hex(soup::base58::decode(maskL_checkstring(L, 1))).c_str());
	}
	catch (...) {
		maskL_error(L, "Attempted to decode non-base58 string.");
	}
	return 1;
}

static int is_valid(mask_State* L) {
	try {
		const auto discarding = soup::base58::decode(maskL_checkstring(L, 1));
		mask_pushboolean(L, true);
	}
	catch (...) {
		mask_pushboolean(L, false);
	}
	return 1;
}

static const maskL_Reg funcs[] = {
	{"is_valid", is_valid},
	{"decode", decode},
	{nullptr, nullptr}
};

MASKMOD_API int maskopen_base58(mask_State* L) {
	maskL_newlib(L, funcs);
	return 1;
}

#endif