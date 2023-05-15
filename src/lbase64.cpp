#include "lauxlib.h"
#include "masklib.h"

#ifdef MASK_USE_SOUP

#include <string>
#include <soup/base64.hpp>

static int encode(mask_State* L) {
	mask_pushstring(L, soup::base64::encode(maskL_checkstring(L, 1), (bool)mask_toboolean(L, 2)).c_str());
	return 1;
}

static int decode(mask_State* L) {
	mask_pushstring(L, soup::base64::decode(maskL_checkstring(L, 1)).c_str());
	return 1;
}

static int urlEncode(mask_State* L) {
	mask_pushstring(L, soup::base64::urlEncode(maskL_checkstring(L, 1), (bool)mask_toboolean(L, 2)).c_str());
	return 1;
}

static int urlDecode(mask_State* L) {
	mask_pushstring(L, soup::base64::urlDecode(maskL_checkstring(L, 1)).c_str());
	return 1;
}

static const maskL_Reg funcs[] = {
	{"url_encode", urlEncode},
	{"url_decode", urlDecode},
	{"encode", encode},
	{"decode", decode},
	{nullptr, nullptr}
};

MASKMOD_API int maskopen_base64(mask_State* L) {
	maskL_newlib(L, funcs);
	return 1;
}

#endif