#include "lauxlib.h"
#include "hellolib.h"

#ifdef HELLO_USE_SOUP

#include "ljson.hpp"

static int encode(hello_State* L) {
	auto root = checkJson(L, 1);
	if (hello_gettop(L) >= 2 && hello_toboolean(L, 2))
	{
		hello_pushstring(L, root->encodePretty());
	}
	else
	{
		hello_pushstring(L, root->encode());
	}
	return 1;
}

static int decode(hello_State* L)
{
	auto root = soup::json::decodeForDedicatedVariable(helloL_checkstring(L, 1));
	pushFromJson(L, *root);
	return 1;
}

static const helloL_Reg funcs[] = {
	{"encode", encode},
	{"decode", decode},
	{nullptr, nullptr}
};

HELLO_API int helloopen_json(hello_State *L) {
	helloL_newlib(L, funcs);
	return 1;
}

#endif