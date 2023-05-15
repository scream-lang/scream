#include "lauxlib.h"
#include "masklib.h"

#ifdef MASK_USE_SOUP

#include <soup/base32.hpp>

static int encode(mask_State* L) {
	mask_pushstring(L, soup::base32::encode(maskL_checkstring(L, 1), (bool)mask_toboolean(L, 2)).c_str());
	return 1;
}

static int decode(mask_State* L) {
	mask_pushstring(L, soup::base32::decode(maskL_checkstring(L, 1)).c_str());
	return 1;
}

static const maskL_Reg funcs[] = {
	{"encode", encode},
	{"decode", decode},
	{nullptr, nullptr}
};

MASKMOD_API int maskopen_base32(mask_State* L) {
	maskL_newlib(L, funcs);
	return 1;
}

#endif