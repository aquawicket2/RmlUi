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

#ifndef RMLUICOREDATAMODEL_H
#define RMLUICOREDATAMODEL_H

#include "Header.h"
#include "Types.h"
#include "Variant.h"
#include "StringUtilities.h"
#include "DataView.h"
#include "DataController.h"

namespace Rml {
namespace Core {


class DataMemberGetSet {
public:
	virtual ~DataMemberGetSet() = default;
	virtual bool Get(const void* object, Rml::Core::Variant& out_value) = 0;
	virtual bool Set(void* object, const Rml::Core::Variant& in_value) = 0;
};

template <typename Object, typename MemberType>
class DataMemberGetSetDefault : public DataMemberGetSet {
public:
	DataMemberGetSetDefault(MemberType Object::* member_ptr) : member_ptr(member_ptr) {}

	bool Get(const void* object, Rml::Core::Variant& out_value) override {
		out_value = static_cast<const Object*>(object)->*member_ptr;
		return true;
	}
	bool Set(void* object, const Rml::Core::Variant& in_value) override {
		MemberType& target = static_cast<Object*>(object)->*member_ptr;
		return in_value.GetInto<MemberType>(target);
	}

private:
	MemberType Object::* member_ptr;
};






enum class ValueType { None, String, Int, Bool, Color, Array, Type };


class DataModel {
public:
	struct Binding {
		ValueType type = ValueType::None;
		void* ptr = nullptr;
		bool writable = false;
		String data_type_name;
	};

	bool GetValue(const String& name, Variant& out_value) const;
	bool GetValue(const String& name, String& out_string) const {
		Variant variant;
		return GetValue(name, variant) && variant.GetInto(out_string);
	}
	bool SetValue(const String& name, const Variant& value) const;
	bool IsWritable(const String& name) const;

	using Bindings = UnorderedMap<String, Binding>;
	Bindings bindings;

	DataControllers controllers;
	DataViews views;

	using DataTypeMembers = SmallUnorderedMap<String, UniquePtr<DataMemberGetSet>>;
	using DataTypes = UnorderedMap<String, DataTypeMembers>;

	DataTypes data_types;
};




class DataTypeHandle {
public:
	DataTypeHandle(DataModel::DataTypeMembers* members) : members(members) {}

	template <typename Object, typename MemberType>
	DataTypeHandle& BindMember(String name, MemberType Object::* member_ptr)
	{
		RMLUI_ASSERT(members);
		members->emplace(name, std::make_unique<DataMemberGetSetDefault<Object, MemberType>>(member_ptr));
		return *this;
	}

private:
	DataModel::DataTypeMembers* members;
};


class DataModelHandle {
public:
	DataModelHandle() : model(nullptr) {}
	DataModelHandle(DataModel* model) : model(model) {}

	DataModelHandle& BindValue(String name, ValueType type, void* ptr, bool writable = false)
	{
		RMLUI_ASSERT(model);
		model->bindings.emplace(name, DataModel::Binding{ type, ptr, writable });
		return *this;
	}

	DataModelHandle& BindDataTypeValue(String name, String type_name, void* ptr, bool writable = false)
	{
		// Todo: We can make this type safe, removing the need for type_name.
		//   Make this a templated function, create another templated "family" class which assigns
		//   a unique id for each new type encountered, look up the type name there. Or use the ID as
		//   the look-up key.
		RMLUI_ASSERT(model);
		model->bindings.emplace(name, DataModel::Binding{ ValueType::Type, ptr, writable, type_name });
		return *this;
	}

	DataTypeHandle RegisterType(String name)
	{
		RMLUI_ASSERT(model);
		auto result = model->data_types.emplace(name, DataModel::DataTypeMembers() );
		return DataTypeHandle(&result.first->second);
	}


	void UpdateControllers() {
		RMLUI_ASSERT(model);
		model->controllers.Update(*model);
	}

	void UpdateViews() {
		RMLUI_ASSERT(model);
		model->views.Update(*model);
	}

	operator bool() { return model != nullptr; }

private:
	DataModel* model;
};

}
}

#endif
