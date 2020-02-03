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

#ifndef RMLUICOREDATAVIEW_H
#define RMLUICOREDATAVIEW_H

#include "Header.h"
#include "Types.h"
#include "Variant.h"
#include "StringUtilities.h"
#include "Traits.h"
#include "DataVariable.h"
#include <unordered_map>

namespace Rml {
namespace Core {

class Element;
class ElementText;
class DataModel;
class DataExpression;
using DataExpressionPtr = UniquePtr<DataExpression>;

class RMLUICORE_API DataViewInstancer : public NonCopyMoveable {
public:
	DataViewInstancer() {}
	virtual ~DataViewInstancer() {}
	virtual DataViewPtr InstanceView(Element* element) = 0;
};

template<typename T>
class DataViewInstancerDefault final : public DataViewInstancer {
public:
	DataViewPtr InstanceView(Element* element) override {
		return DataViewPtr(new T(element));
	}
};


class RMLUICORE_API DataView : public Releasable {
public:
	virtual ~DataView();

	// Initialize the data view.
	// @param model The data model the view will be attached to.
	// @param data_expression The value of the attribute associated with the The attribute value associated
	virtual bool Initialize(DataModel& model, Element* element, const String& data_expression, const String& view_label) = 0;

	// Update the data view.
	// Returns true if the update resulted in a change.
	virtual bool Update(DataModel& model) = 0;

	// Returns the list of data variable name(s) which can modify this view.
	virtual StringList GetVariableNameList() const = 0;


	// Returns the attached element if it still exists.
	Element* GetElement() const;

	// Returns the depth of the attached element in the document tree.
	int GetElementDepth() const;
	
	// Returns true if the element still exists
	bool IsValid() const;
	
protected:
	DataView(Element* element);
	void Release() override;

private:
	ObserverPtr<Element> attached_element;
	int element_depth;
};



class DataViewNamedExpression : public DataView {
public:
	DataViewNamedExpression(Element* element);
	~DataViewNamedExpression();

	bool Initialize(DataModel& model, Element* element, const String& data_expression_str, const String& view_label) override;

	StringList GetVariableNameList() const override;

protected:
	const String& GetViewLabel() const;
	DataExpression& GetDataExpression();

private:
	String view_label;
	DataExpressionPtr data_expression;
};


class DataViewAttribute final : public DataViewNamedExpression {
public:
	DataViewAttribute(Element* element);
	~DataViewAttribute();

	bool Update(DataModel& model) override;
};


class DataViewStyle final : public DataViewNamedExpression {
public:
	DataViewStyle(Element* element);
	~DataViewStyle();

	bool Update(DataModel& model) override;
};

class DataViewClass final : public DataViewNamedExpression {
public:
	DataViewClass(Element* element);
	~DataViewClass();

	bool Update(DataModel& model) override;
};

class DataViewRml final : public DataViewNamedExpression {
public:
	DataViewRml(Element* element);
	~DataViewRml();

	bool Update(DataModel& model) override;

private:
	String previous_rml;
};


class DataViewIf final : public DataViewNamedExpression {
public:
	DataViewIf(Element* element);
	~DataViewIf();

	bool Update(DataModel& model) override;
};




class DataViewText final : public DataView {
public:
	DataViewText(Element* in_element);
	~DataViewText();

	bool Initialize(DataModel& model, Element* element, const String& data_expression, const String& view_label) override;

	bool Update(DataModel& model) override;
	StringList GetVariableNameList() const override;

private:
	String BuildText() const;

	struct DataEntry {
		size_t index = 0; // Index into 'text'
		DataExpressionPtr data_expression;
		String value;
	};

	String text;
	std::vector<DataEntry> data_entries;
};



class DataViewFor final : public DataView {
public:
	DataViewFor(Element* element);
	~DataViewFor();

	bool Initialize(DataModel& model, Element* element, const String& binding_str, const String& rml_contents) override;

	bool Update(DataModel& model) override;

	StringList GetVariableNameList() const override;

private:
	DataAddress variable_address;
	String alias_name;
	String rml_contents;
	ElementAttributes attributes;

	ElementList elements;
};


class RMLUICORE_API DataViews : NonCopyMoveable {
public:
	DataViews();
	~DataViews();

	void Add(DataViewPtr view);

	void OnElementRemove(Element* element);

	bool Update(DataModel& model, const SmallUnorderedSet< String >& dirty_variables);

private:
	using DataViewList = std::vector<DataViewPtr>;

	DataViewList views;
	
	DataViewList views_to_add;
	DataViewList views_to_remove;

	using NameViewMap = std::unordered_multimap<String, DataView*>;
	NameViewMap name_view_map;
};

}
}

#endif
