/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2008-2010 CodePoint Ltd, Shift Technology Ltd
 * Copyright (c) 2019 The RmlUi Team, and contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "precompiled.h"
#include "../../Include/RmlUi/Core/DataModel.h"

namespace Rml {
namespace Core {


bool DataModel::GetValue(const String& in_name, Variant& out_value) const
{
	bool success = true;

	String name = in_name;
	String member;

	size_t i_dot = name.find('.');
	if (i_dot != String::npos)
	{
		name = in_name.substr(0, i_dot);
		member = in_name.substr(i_dot + 1);
	}

	auto it = bindings.find(name);
	if (it != bindings.end())
	{
		const Binding& binding = it->second;

		if (binding.type == ValueType::String)
			out_value = *static_cast<const String*>(binding.ptr);
		else if (binding.type == ValueType::Int)
			out_value = *static_cast<const int*>(binding.ptr);
		else if (binding.type == ValueType::Type)
		{
			success = false;
			auto it_type = data_types.find(binding.data_type_name);
			if(it_type != data_types.end())
			{
				const auto& members = it_type->second;
				auto it_member = members.find(member);
				if (it_member != members.end())
				{
					auto member_getset_ptr = it_member->second.get();
					RMLUI_ASSERT(member_getset_ptr);
					success = member_getset_ptr->Get(binding.ptr, out_value);
				}
			}
			if(!success)
				Log::Message(Log::LT_WARNING, "Could not get value from member '%s' in value named '%s' in data model.", member.c_str(), name.c_str());
		}
		else
		{
			RMLUI_ERRORMSG("TODO: Implementation for the provided binding type has not been made yet.");
			success = false;
		}
	}
	else
	{
		Log::Message(Log::LT_WARNING, "Could not find value named '%s' in data model.", name.c_str());
		success = false;
	}

	return success;
}


bool DataModel::SetValue(const String& name, const Variant& value) const
{
	bool result = true;

	auto it = bindings.find(name);
	if (it != bindings.end())
	{
		const Binding& binding = it->second;

		if (binding.writable)
		{
			if (binding.type == ValueType::String)
				result = value.GetInto(*static_cast<String*>(binding.ptr));
			else if (binding.type == ValueType::Int)
				result = value.GetInto(*static_cast<int*>(binding.ptr));
			else
			{
				RMLUI_ERRORMSG("TODO: Implementation for the provided binding type has not been made yet.");
				result = false;
			}
		}
		else
		{
			RMLUI_ERRORMSG("Controller attempted to write to a non-writable binding.");
			result = false;
		}
	}
	else
	{
		Log::Message(Log::LT_WARNING, "Could not find value named '%s' in data model.", name.c_str());
		result = false;
	}
	return false;
}

bool DataModel::IsWritable(const String& name) const
{
	bool result = false;
	auto it = bindings.find(name);
	if (it != bindings.end())
		result = it->second.writable;
	return result;
}

}
}
