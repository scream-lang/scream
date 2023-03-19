#include "lauxlib.h"
#include "hellolib.h"

#ifdef HELLO_USE_SOUP

#include <soup/base32.hpp>

static int encode(hello_State* L) {
	hello_pushstring(L, soup::base32::encode(helloL_checkstring(L, 1), (bool)hello_toboolean(L, 2)).c_str());
	return 1;
}

static int decode(hello_State* L) {
	hello_pushstring(L, soup::base32::decode(helloL_checkstring(L, 1)).c_str());
	return 1;
}

static const helloL_Reg funcs[] = {
	{"encode", encode},
	{"decode", decode},
	{nullptr, nullptr}
};

HELLOMOD_API int helloopen_base32(hello_State* L) {
	helloL_newlib(L, funcs);
	return 1;
}

#endif