#include "lauxlib.h"
#include "hellolib.h"

#ifdef HELLO_USE_SOUP

#include <string>
#include <soup/base64.hpp>

static int encode(hello_State* L) {
	hello_pushstring(L, soup::base64::encode(helloL_checkstring(L, 1), (bool)hello_toboolean(L, 2)).c_str());
	return 1;
}

static int decode(hello_State* L) {
	hello_pushstring(L, soup::base64::decode(helloL_checkstring(L, 1)).c_str());
	return 1;
}

static int urlEncode(hello_State* L) {
	hello_pushstring(L, soup::base64::urlEncode(helloL_checkstring(L, 1), (bool)hello_toboolean(L, 2)).c_str());
	return 1;
}

static int urlDecode(hello_State* L) {
	hello_pushstring(L, soup::base64::urlDecode(helloL_checkstring(L, 1)).c_str());
	return 1;
}

static const helloL_Reg funcs[] = {
	{"url_encode", urlEncode},
	{"url_decode", urlDecode},
	{"encode", encode},
	{"decode", decode},
	{nullptr, nullptr}
};

HELLOMOD_API int helloopen_base64(hello_State* L) {
	helloL_newlib(L, funcs);
	return 1;
}

#endif