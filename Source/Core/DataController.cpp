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
#include "../../Include/RmlUi/Core/DataController.h"
#include "../../Include/RmlUi/Core/DataModel.h"

namespace Rml {
namespace Core {

DataController::DataController(Element* element) : attached_element(element->GetObserverPtr())
{}

Variable DataController::GetVariable() const {
	if (Element* element = attached_element.get())
	{
		if (DataModel* model = element->GetDataModel())
			return model->GetVariable(address);
	}
	return Variable();
}

void DataController::SetValue(const Variant& new_value)
{
	RMLUI_ASSERT(!address.empty());
	if (value == new_value)
		return;

	Element* element = attached_element.get();
	RMLUI_ASSERT(element);
	if (!element)
		return;

	DataModel* model = element->GetDataModel();
	RMLUI_ASSERT(model);
	if (!model)
		return;

	if (Variable variable = model->GetVariable(address))
	{
		value = new_value;
		variable.Set(value);
		model->DirtyVariable(address.front().name);
	}
}

DataController::~DataController()
{}

DataControllerValue::DataControllerValue(DataModel& model, Element* element, const String& in_variable_name) : DataController(element)
{
	DataAddress variable_address = model.ResolveAddress(in_variable_name, element);

	if (model.GetVariable(variable_address) && !variable_address.empty())
	{
		SetAddress(std::move(variable_address));
	}
	element->AddEventListener(EventId::Change, this);
}

DataControllerValue::~DataControllerValue()
{
	if (Element* element = GetElement())
	{
		element->RemoveEventListener(EventId::Change, this);
	}
}

void DataControllerValue::ProcessEvent(Event& event)
{
	if (Element* element = GetElement())
	{
		if (Variant* new_value = element->GetAttribute("value"))
		{
			SetValue(*new_value);
		}
	}
}



DataControllerEvent::DataControllerEvent(DataModel& model, Element* element, const String& in_variable_name) : DataController(element)
{
	RMLUI_ASSERT(element);

	DataAddress variable_address = model.ResolveAddress(in_variable_name, element);

	// TODO add data assignment expression parser


	if (model.GetVariable(variable_address) && !variable_address.empty())
	{
		SetAddress(std::move(variable_address));
	}

	element->AddEventListener(EventId::Click, this);
}

DataControllerEvent::~DataControllerEvent()
{
	if (Element* element = GetElement())
	{
		element->RemoveEventListener(EventId::Click, this);
	}
}

void DataControllerEvent::ProcessEvent(Event& event)
{
	if (Element* element = GetElement())
	{
		static unsigned int counter = 0;
		counter += 1;
		element->SetInnerRML(CreateString(64, "We got a click! Number %d.", counter));


		// TODO Run data assignment expression
		//Element* element = GetElement();
		//DataExpressionInterface interface(&model, element);

		//if (element && GetExpression().Run(interface, variant))
	}
}





void DataControllers::Add(UniquePtr<DataController> controller) {
	RMLUI_ASSERT(controller);

	Element* element = controller->GetElement();
	RMLUI_ASSERTMSG(element, "Invalid controller, make sure it is valid before adding");
	if (!element)
		return;

	controllers.emplace(element, std::move(controller));
}

void DataControllers::OnElementRemove(Element* element)
{
	controllers.erase(element);
}


}
}
