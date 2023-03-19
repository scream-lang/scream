#pragma once
#ifdef HELLO_USE_SOUP
// https://github.com/calamity-inc/Soup-Hello-Bindings/blob/main/soup_hello_bindings.hpp
#include <soup/json.hpp>
#include <soup/JsonInt.hpp>
#include <soup/JsonBool.hpp>
#include <soup/JsonNode.hpp>
#include <soup/JsonFloat.hpp>
#include <soup/JsonArray.hpp>
#include <soup/JsonObject.hpp>
#include <soup/JsonString.hpp>
#include <soup/UniquePtr.hpp>

static bool isIndexBasedTable(hello_State* L, int i)
{
	hello_pushvalue(L, i);
	hello_pushnil(L);
	size_t k = 1;
	for (; hello_next(L, -2); ++k)
	{
		hello_pushvalue(L, -2);
		if (hello_type(L, -1) != HELLO_TNUMBER)
		{
			hello_pop(L, 4);
			return false;
		}
		if (!hello_isinteger(L, -1))
		{
			hello_pop(L, 4);
			return false;
		}
		if (hello_tointeger(L, -1) != k)
		{
			hello_pop(L, 4);
			return false;
		}
		hello_pop(L, 2);
	}
	hello_pop(L, 1);
	return k != 1; // say it's not an index based table if it's empty
}

static soup::UniquePtr<soup::JsonNode> checkJson(hello_State* L, int i)
{
	auto type = hello_type(L, i);
	if (type == HELLO_TBOOLEAN)
	{
		return soup::make_unique<soup::JsonBool>(hello_toboolean(L, i));
	}
	else if (type == HELLO_TNUMBER)
	{
		if (hello_isinteger(L, i))
		{
			return soup::make_unique<soup::JsonInt>(hello_tointeger(L, i));
		}
		else
		{
			return soup::make_unique<soup::JsonFloat>(hello_tonumber(L, i));
		}
	}
	else if (type == HELLO_TSTRING)
	{
		return soup::make_unique<soup::JsonString>(hello_tostring(L, i));
	}
	else if (type == HELLO_TTABLE)
	{
		if (isIndexBasedTable(L, i))
		{
			auto arr = soup::make_unique<soup::JsonArray>();
			hello_pushvalue(L, i);
			hello_pushnil(L);
			while (hello_next(L, -2))
			{
				hello_pushvalue(L, -2);
				arr->children.emplace_back(checkJson(L, -2));
				hello_pop(L, 2);
			}
			hello_pop(L, 1);
			return arr;
		}
		else
		{
			auto obj = soup::make_unique<soup::JsonObject>();
			hello_pushvalue(L, i);
			hello_pushnil(L);
			while (hello_next(L, -2))
			{
				hello_pushvalue(L, -2);
				obj->children.emplace_back(checkJson(L, -1), checkJson(L, -2));
				hello_pop(L, 2);
			}
			hello_pop(L, 1);
			return obj;
		}
	}
	helloL_typeerror(L, i, "JSON-castable type");
}

static void pushFromJson(hello_State* L, const soup::JsonNode& node)
{
	if (node.isBool())
	{
		hello_pushboolean(L, node.asBool().value);
	}
	else if (node.isInt())
	{
		hello_pushinteger(L, node.asInt().value);
	}
	else if (node.isFloat())
	{
		hello_pushnumber(L, node.asFloat().value);
	}
	else if (node.isStr())
	{
		hello_pushstring(L, node.asStr().value);
	}
	else if (node.isArr())
	{
		hello_newtable(L);
		hello_Integer i = 1;
		for (const auto& child : node.asArr().children)
		{
			hello_pushinteger(L, i++);
			pushFromJson(L, *child);
			hello_settable(L, -3);
		}
	}
	else if (node.isObj())
	{
		hello_newtable(L);
		for (const auto& e : node.asObj().children)
		{
			pushFromJson(L, *e.first);
			pushFromJson(L, *e.second);
			hello_settable(L, -3);
		}
	}
	else
	{
		hello_pushnil(L);
	}
}

#endif