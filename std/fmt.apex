local function format(str, ...)
	assert(type(str) == 'string', 'Expected string for argument 1, got '..type(str)..' ('..tostring(str)..')')
	local args = table.pack(str, ...)
	local formatters = {}
	local rawOptions = {}
	local options = {}
	local nextFormatter
	local convertToString = false
	for i = 1, args.n do
		local arg = args[i]
		if nextFormatter then
			formatters[#formatters + 1] = nextFormatter
			args[i] = nextFormatter
			nextFormatter = nil
			local option
			if convertToString then
				option = tostring(arg)
			else
				option = arg
			end
			rawOptions[#rawOptions + 1] = option
			options[#options + 1] = option
		elseif type(arg) == 'string' then
			local base
			do
				local escapes, formatter, specifier
				base, escapes, formatter, specifier = arg:match('^(.*)(%%*)(%%[%-%+ #0]?%d*%.?[%d%*]*[hljztL]?[hl]?)([diuoxXfFeEgGaAcspn])$')
				if base and #escapes%2 == 0 then
					nextFormatter = formatter..specifier
					convertToString = specifier == 's'
				else
					base = arg
				end
			end
			if not base then
				base = arg
			end
			for escapes, formatter, specifier in base:gmatch('(%%+)([%-%+ #0]?%d*%.?[%d%*]*[hljztL]?[hl]?)([diuoxXfFeEgGaAcspn])') do
				assert(#escapes%2 == 0, 'Unexpected formatter "%'..formatter..specifier..'": formatters should only be at the end of non-option strings')
			end
			base = base:gsub('(%%+)(%d+)', function(escapes, digits)
				if #escapes%2 == 0 then
					return
				end
				local optionIndex = tonumber(digits)
				local formatter = assert(formatters[optionIndex], 'Option '..digits..' must be defined before it can be reused')
				local option = rawOptions[optionIndex]
				options[#options + 1] = option
				return formatter
			end)
			args[i] = base
		else
			args[i] = '%s'
			options[#options + 1] = tostring(arg)
		end
	end

	return table.concat(args), options, rawOptions, formatters
end

---@class Formatter
local Formatter = {}
Formatter.__index = Formatter

---@return string @Formatted string
function Formatter:__call()
	return string.format(self.FormatString, unpack(self.Options))
end

---@return string @Formatted string
function Formatter:__tostring()
	return string.format(self.FormatString, unpack(self.Options))
end

local fmt = {}
setmetatable(fmt, fmt)

---@param str string @Formatter string
---@vararg any @Formatter strings or option values
---@return Formatter @String formatter
function fmt.new(str, ...)
	local formatStr, options, rawOptions_, formatters_ = format(str, ...)
	local self = {
		FormatString = formatStr,
		Options = options,
	}
	return setmetatable(self, Formatter)
end

---@param str string @Formatter string
---@vararg any @Formatter strings or option values
---@return string @Formatted string
function fmt:__call(str, ...)
	local formatStr, options = format(str, ...)
	return string.format(formatStr, unpack(options))
end

fmt.raw = format

return fmt
