#pragma once
#ifdef MASK_USE_SOUP
// https://github.com/calamity-inc/Soup-Mask-Bindings/blob/main/soup_mask_bindings.hpp
#include <soup/json.hpp>
#include <soup/JsonInt.hpp>
#include <soup/JsonBool.hpp>
#include <soup/JsonNode.hpp>
#include <soup/JsonFloat.hpp>
#include <soup/JsonArray.hpp>
#include <soup/JsonObject.hpp>
#include <soup/JsonString.hpp>
#include <soup/UniquePtr.hpp>

static bool isIndexBasedTable(mask_State* L, int i)
{
	mask_pushvalue(L, i);
	mask_pushnil(L);
	size_t k = 1;
	for (; mask_next(L, -2); ++k)
	{
		mask_pushvalue(L, -2);
		if (mask_type(L, -1) != MASK_TNUMBER)
		{
			mask_pop(L, 4);
			return false;
		}
		if (!mask_isinteger(L, -1))
		{
			mask_pop(L, 4);
			return false;
		}
		if (mask_tointeger(L, -1) != k)
		{
			mask_pop(L, 4);
			return false;
		}
		mask_pop(L, 2);
	}
	mask_pop(L, 1);
	return k != 1; // say it's not an index based table if it's empty
}

static soup::UniquePtr<soup::JsonNode> checkJson(mask_State* L, int i)
{
	auto type = mask_type(L, i);
	if (type == MASK_TBOOLEAN)
	{
		return soup::make_unique<soup::JsonBool>(mask_toboolean(L, i));
	}
	else if (type == MASK_TNUMBER)
	{
		if (mask_isinteger(L, i))
		{
			return soup::make_unique<soup::JsonInt>(mask_tointeger(L, i));
		}
		else
		{
			return soup::make_unique<soup::JsonFloat>(mask_tonumber(L, i));
		}
	}
	else if (type == MASK_TSTRING)
	{
		return soup::make_unique<soup::JsonString>(mask_tostring(L, i));
	}
	else if (type == MASK_TTABLE)
	{
		if (isIndexBasedTable(L, i))
		{
			auto arr = soup::make_unique<soup::JsonArray>();
			mask_pushvalue(L, i);
			mask_pushnil(L);
			while (mask_next(L, -2))
			{
				mask_pushvalue(L, -2);
				arr->children.emplace_back(checkJson(L, -2));
				mask_pop(L, 2);
			}
			mask_pop(L, 1);
			return arr;
		}
		else
		{
			auto obj = soup::make_unique<soup::JsonObject>();
			mask_pushvalue(L, i);
			mask_pushnil(L);
			while (mask_next(L, -2))
			{
				mask_pushvalue(L, -2);
				obj->children.emplace_back(checkJson(L, -1), checkJson(L, -2));
				mask_pop(L, 2);
			}
			mask_pop(L, 1);
			return obj;
		}
	}
	maskL_typeerror(L, i, "JSON-castable type");
}

static void pushFromJson(mask_State* L, const soup::JsonNode& node)
{
	if (node.isBool())
	{
		mask_pushboolean(L, node.asBool().value);
	}
	else if (node.isInt())
	{
		mask_pushinteger(L, node.asInt().value);
	}
	else if (node.isFloat())
	{
		mask_pushnumber(L, node.asFloat().value);
	}
	else if (node.isStr())
	{
		mask_pushstring(L, node.asStr().value);
	}
	else if (node.isArr())
	{
		mask_newtable(L);
		mask_Integer i = 1;
		for (const auto& child : node.asArr().children)
		{
			mask_pushinteger(L, i++);
			pushFromJson(L, *child);
			mask_settable(L, -3);
		}
	}
	else if (node.isObj())
	{
		mask_newtable(L);
		for (const auto& e : node.asObj().children)
		{
			pushFromJson(L, *e.first);
			pushFromJson(L, *e.second);
			mask_settable(L, -3);
		}
	}
	else
	{
		mask_pushnil(L);
	}
}

#endif