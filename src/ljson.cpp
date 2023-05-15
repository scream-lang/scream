#include "lauxlib.h"
#include "masklib.h"

#ifdef MASK_USE_SOUP

#include "ljson.hpp"

static int encode(mask_State* L) {
	auto root = checkJson(L, 1);
	if (mask_gettop(L) >= 2 && mask_toboolean(L, 2))
	{
		mask_pushstring(L, root->encodePretty());
	}
	else
	{
		mask_pushstring(L, root->encode());
	}
	return 1;
}

static int decode(mask_State* L)
{
	auto root = soup::json::decodeForDedicatedVariable(maskL_checkstring(L, 1));
	pushFromJson(L, *root);
	return 1;
}

static const maskL_Reg funcs[] = {
	{"encode", encode},
	{"decode", decode},
	{nullptr, nullptr}
};

MASK_API int maskopen_json(mask_State *L) {
	maskL_newlib(L, funcs);
	return 1;
}

#endif