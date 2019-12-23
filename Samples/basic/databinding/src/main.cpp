/*
 * This source file is part of RmlUi, the HTML/CSS Interface Middleware
 *
 * For the latest information, see http://github.com/mikke89/RmlUi
 *
 * Copyright (c) 2018 Michael R. P. Ragazzon
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

#include <RmlUi/Core.h>
#include <RmlUi/Controls.h>
#include <RmlUi/Debugger.h>
#include <Input.h>
#include <Shell.h>
#include <ShellRenderInterfaceOpenGL.h>


class DemoWindow : public Rml::Core::EventListener
{
public:
	DemoWindow(const Rml::Core::String &title, const Rml::Core::Vector2f &position, Rml::Core::Context *context)
	{
		using namespace Rml::Core;
		document = context->LoadDocument("basic/databinding/data/databinding.rml");
		if (document)
		{
			document->GetElementById("title")->SetInnerRML(title);
			document->SetProperty(PropertyId::Left, Property(position.x, Property::PX));
			document->SetProperty(PropertyId::Top, Property(position.y, Property::PX));

			document->Show();
		}
	}

	void Update() 
	{

	}

	void Shutdown() 
	{
		if (document)
		{
			document->Close();
			document = nullptr;
		}
	}

	void ProcessEvent(Rml::Core::Event& event) override
	{
		using namespace Rml::Core;

		switch (event.GetId())
		{
		case EventId::Keydown:
		{
			Rml::Core::Input::KeyIdentifier key_identifier = (Rml::Core::Input::KeyIdentifier) event.GetParameter< int >("key_identifier", 0);
			bool ctrl_key = event.GetParameter< bool >("ctrl_key", false);

			if (key_identifier == Rml::Core::Input::KI_ESCAPE)
			{
				Shell::RequestExit();
			}
			else if (key_identifier == Rml::Core::Input::KI_F8)
			{
				Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
			}
		}
		break;

		default:
			break;
		}
	}

	Rml::Core::ElementDocument * GetDocument() {
		return document;
	}


private:
	Rml::Core::ElementDocument *document = nullptr;
};



struct Invader {
	Rml::Core::String name;
	Rml::Core::String sprite;
	Rml::Core::String color;
};


struct MyData {
	Rml::Core::String hello_world = "Hello World!";
	int rating = 99;
	bool good_rating = true;

	Invader invader{ "Delightful invader", "icon-invader", "red" };

	std::vector<Invader> invaders;

	std::vector<int> indices = { 1, 2, 3, 4, 5 };
} my_data;

Rml::Core::DataModelHandle my_model;












namespace Data {
	using namespace Rml::Core;


	template<typename T>
	struct is_valid_scalar {
		static constexpr bool value = std::is_convertible<T, byte>::value
			|| std::is_convertible<T, char>::value
			|| std::is_convertible<T, float>::value
			|| std::is_convertible<T, int>::value
			|| std::is_convertible<T, String>::value
			|| std::is_convertible<T, Vector2f>::value
			|| std::is_convertible<T, Vector3f>::value
			|| std::is_convertible<T, Vector4f>::value
			|| std::is_convertible<T, Colourb>::value
			|| std::is_convertible<T, Colourf>::value
			|| std::is_convertible<T, char*>::value
			|| std::is_convertible<T, void*>::value;
	};



	struct AddressEntry {
		AddressEntry(String name) : name(name), index(-1) { }
		AddressEntry(int index) : index(index) { }
		String name;
		int index;
	};
	using Address = std::vector<AddressEntry>;

	class Variable;
	enum class VariableType { Scalar, Array, Struct };


	class VariableDefinition {
	public:
		virtual ~VariableDefinition() = default;
		VariableType Type() const { return type; }

		virtual bool Get(void* ptr, Variant& variant) {
			Log::Message(Log::LT_WARNING, "Values can only be retrieved from scalar data types.");
			return false;
		}
		virtual bool Set(void* ptr, const Variant& variant) {
			Log::Message(Log::LT_WARNING, "Values can only be assigned to scalar data types.");
			return false;
		}
		virtual int Size(void* ptr) {
			Log::Message(Log::LT_WARNING, "Tried to get the size from a non-array data type.");
			return 0;
		}
		virtual Variable GetChild(void* ptr, const AddressEntry& address);

	protected:
		VariableDefinition(VariableType type) : type(type) {}

	private:
		VariableType type;
	};


	class Variable {
	public:
		Variable() {}
		Variable(VariableDefinition* definition, void* ptr) : definition(definition), ptr(ptr) {}

		operator bool() const { return definition && ptr; }

		bool Get(Variant& variant) {
			return definition->Get(ptr, variant);
		}
		bool Set(const Variant& variant) {
			return definition->Set(ptr, variant);
		}
		int Size() {
			return definition->Size(ptr);
		}
		Variable GetChild(const AddressEntry& address) {
			return definition->GetChild(ptr, address);
		}
		VariableType Type() const {
			return definition->Type();
		}

	private:
		VariableDefinition* definition = nullptr;
		void* ptr = nullptr;
	};



	inline Variable VariableDefinition::GetChild(void* ptr, const AddressEntry& address) {
		Log::Message(Log::LT_WARNING, "Tried to get the child of a scalar type.");
		return Variable();
	}


	
	template<typename T>
	class ScalarDefinition final : public VariableDefinition {
	public:
		ScalarDefinition() : VariableDefinition(VariableType::Scalar) {}

		bool Get(void* ptr, Variant& variant) override 
		{
			variant = *static_cast<T*>(ptr);
			return true;
		}
		bool Set(void* ptr, const Variant& variant) override
		{
			return variant.GetInto<T>(*static_cast<T*>(ptr));
		}
	};


	template<typename Container>
	class ArrayDefinition final : public VariableDefinition {
	public:
		ArrayDefinition(VariableDefinition* underlying_variable) : VariableDefinition(VariableType::Array), underlying_variable(underlying_variable) {}

		int Size(void* ptr) override {
			return int(static_cast<Container*>(ptr)->size());
		}

	protected:
		Variable GetChild(void* void_ptr, const AddressEntry& address) override
		{
			Container* ptr = static_cast<Container*>(void_ptr);
			const int index = address.index;

			if (index < 0 && index >= (int)ptr->size())
			{
				Log::Message(Log::LT_WARNING, "Data array index out of bounds.");
				return Variable();
			}

			void* next_ptr = &((*ptr)[index]);
			return Variable(underlying_variable, next_ptr);
		}

	private:
		VariableDefinition* underlying_variable;
	};


	class StructMember {
	public:
		StructMember(VariableDefinition* variable) : variable(variable) {}
		virtual ~StructMember() = default;

		VariableDefinition* GetVariable() const { return variable; }

		virtual void* GetPointer(void* base_ptr) = 0;

	private:
		VariableDefinition* variable;
	};

	template <typename Object, typename MemberType>
	class StructMemberDefault final : public StructMember {
	public:
		StructMemberDefault(VariableDefinition* variable, MemberType Object::* member_ptr) : StructMember(variable), member_ptr(member_ptr) {}

		void* GetPointer(void* base_ptr) override {
			return &(static_cast<Object*>(base_ptr)->*member_ptr);
		}

	private:
		MemberType Object::* member_ptr;
	};


	class StructDefinition final : public VariableDefinition {
	public:
		StructDefinition() : VariableDefinition(VariableType::Struct)
		{}

		Variable GetChild(void* ptr, const AddressEntry& address) override
		{
			const String& name = address.name;
			if (name.empty())
			{
				Log::Message(Log::LT_WARNING, "Expected a struct member name but none given.");
				return Variable();
			}

			auto it = members.find(name);
			if (it == members.end())
			{
				Log::Message(Log::LT_WARNING, "Member %s not found in data struct.", name.c_str());
				return Variable();
			}

			void* next_ptr = it->second->GetPointer(ptr);
			VariableDefinition* next_variable = it->second->GetVariable();

			return Variable(next_variable, next_ptr);
		}

		void AddMember(const String& name, UniquePtr<StructMember> member)
		{
			RMLUI_ASSERT(member);
			bool inserted = members.emplace(name, std::move(member)).second;
			RMLUI_ASSERTMSG(inserted, "Member name already exists.");
		}

	private:
		SmallUnorderedMap<String, UniquePtr<StructMember>> members;
	};



	class TypeRegister;

	class TypeHandle {
	public:
		operator bool() const { return type_register && GetDefinition(); }
		virtual VariableDefinition* GetDefinition() const = 0;
	protected:
		TypeHandle(TypeRegister* type_register) : type_register(type_register) {}
		TypeRegister* type_register;
	};

	template <typename T>
	class ScalarHandle : public TypeHandle {
	public:
		ScalarHandle(TypeRegister* type_register) : TypeHandle(type_register) {}
		VariableDefinition* GetDefinition() const override;
	};

	template<typename Object>
	class StructHandle final : public TypeHandle {
	public:
		StructHandle(TypeRegister* type_register, StructDefinition* struct_definition) : TypeHandle(type_register), struct_definition(struct_definition) {}

		// Register scalar member type 
		template <typename MemberType>
		StructHandle<Object>& RegisterMember(const String& name, MemberType Object::* member_ptr) {
			static_assert(is_valid_scalar<MemberType>::value, "Not a valid scalar member type. Did you mean to add a struct or array member? If so, provide its handle.");
			return RegisterMember(name, member_ptr, ScalarHandle<MemberType>(type_register));
		}

		// Register struct or array member type
		template <typename MemberType>
		StructHandle<Object>& RegisterMember(const String& name, MemberType Object::* member_ptr, const TypeHandle& member_handle);

		VariableDefinition* GetDefinition() const override {
			return struct_definition;
		}

	private:
		StructDefinition* struct_definition;
	};

	template<typename Container>
	class ArrayHandle final : public TypeHandle {
	public:
		ArrayHandle(TypeRegister* type_register, ArrayDefinition<Container>* array_definition) : TypeHandle(type_register), array_definition(array_definition) {}

		VariableDefinition* GetDefinition() const override {
			return array_definition;
		}

	private:
		ArrayDefinition<Container>* array_definition;
	};




	class TypeRegister {
	public:
		template<typename T>
		StructHandle<T> RegisterStruct()
		{
			static_assert(std::is_class<T>::value, "Type must be a struct or class type.");
			FamilyId id = Family<T>::Id();

			auto struct_variable = std::make_unique<StructDefinition>();
			StructDefinition* struct_variable_raw = struct_variable.get();

			bool inserted = type_register.emplace(id, std::move(struct_variable)).second;
			if (!inserted)
			{
				RMLUI_ERRORMSG("Type already declared");
				return StructHandle<T>(nullptr, nullptr);
			}
			
			return StructHandle<T>(this, struct_variable_raw);
		}

		// Register array of scalars
		template<typename Container>
		ArrayHandle<Container> RegisterArray()
		{
			using value_type = typename Container::value_type;
			static_assert(is_valid_scalar<value_type>::value, "Underlying value type of array is not a valid scalar type. Provide the type handle if adding an array of structs or arrays.");
			VariableDefinition* value_variable = GetOrAddScalar<value_type>();
			return RegisterArray<Container>(value_variable);
		}

		// Register array of structs or arrays
		template<typename Container>
		ArrayHandle<Container> RegisterArray(const TypeHandle& type_handle)
		{
			using value_type = typename Container::value_type;
			VariableDefinition* value_variable = Get<value_type>();
			bool correct_handle = (type_handle.GetDefinition() == value_variable);

			RMLUI_ASSERTMSG(value_variable, "Underlying value type of array has not been registered.");
			RMLUI_ASSERTMSG(correct_handle, "Improper type handle provided.");
			if (!value_variable || !correct_handle)
				return ArrayHandle<Container>(nullptr, nullptr);

			return RegisterArray<Container>(value_variable);
		}

		template<typename T>
		VariableDefinition* GetOrAddScalar()
		{
			FamilyId id = Family<T>::Id();

			auto result = type_register.emplace(id, nullptr);
			auto& it = result.first;
			bool inserted = result.second;

			if (inserted)
			{
				it->second = std::make_unique<ScalarDefinition<T>>();
			}

			return it->second.get();
		}

		template<typename T>
		VariableDefinition* Get() const
		{
			FamilyId id = Family<T>::Id();
			auto it = type_register.find(id);
			if (it == type_register.end())
				return nullptr;

			return it->second.get();
		}

	private:
		template<typename Container>
		ArrayHandle<Container> RegisterArray(VariableDefinition* value_variable)
		{
			FamilyId container_id = Family<Container>::Id();

			auto array_variable = std::make_unique<ArrayDefinition<Container>>(value_variable);
			ArrayDefinition<Container>* array_variable_raw = array_variable.get();

			bool inserted = type_register.emplace(container_id, std::move(array_variable)).second;
			if (!inserted)
			{
				RMLUI_ERRORMSG("Array type already declared.");
				return ArrayHandle<Container>(nullptr, nullptr);
			}

			return ArrayHandle<Container>(this, array_variable_raw);
		}

		UnorderedMap<FamilyId, UniquePtr<VariableDefinition>> type_register;
	};


	template<typename T>
	inline VariableDefinition* ScalarHandle<T>::GetDefinition() const {
		return type_register->GetOrAddScalar<T>();
	}

	template<typename Object>
	template<typename MemberType>
	inline StructHandle<Object>& StructHandle<Object>::RegisterMember(const String& name, MemberType Object::* member_ptr, const TypeHandle& member_handle) {
		RMLUI_ASSERTMSG(member_handle.GetDefinition() == type_register->Get<MemberType>(), "Mismatch between member type and provided type handle.");
		struct_definition->AddMember(name, std::make_unique<StructMemberDefault<Object, MemberType>>(member_handle.GetDefinition(), member_ptr));
		return *this;
	}



	Address ParseAddress(const String& address_str);

	class Model {
	public:
		Model(TypeRegister* type_register) : type_register(type_register) {}

		template<typename T> bool BindScalar(String name, T* ptr) {
			return Bind(name, ptr, type_register->GetOrAddScalar<T>(), VariableType::Scalar);
		}
		template<typename T> bool BindStruct(String name, T* ptr) {
			return Bind(name, ptr, type_register->Get<T>(), VariableType::Struct);
		}
		template<typename T> bool BindArray(String name, T* ptr) {
			return Bind(name, ptr, type_register->Get<T>(), VariableType::Array);
		}

		Variant GetValue(const String& address_str) const;
		bool SetValue(const String& address_str, const Variant& variant) const;

		Variable GetVariable(const String& address_str) const;
		Variable GetVariable(const Address& address) const;

	private:
		bool Bind(String name, void* ptr, VariableDefinition* variable, VariableType type);



		TypeRegister* type_register;

		UnorderedMap<String, Variable> variables;
	};


	Address ParseAddress(const String& address_str)
	{
		StringList list;
		StringUtilities::ExpandString(list, address_str, '.');

		Address address;
		address.reserve(list.size() * 2);

		for (const auto& item : list)
		{
			if (item.empty())
				return Address();

			size_t i_open = item.find('[', 0);
			if (i_open == 0)
				return Address();

			address.emplace_back(item.substr(0, i_open));

			while (i_open != String::npos)
			{
				size_t i_close = item.find(']', i_open + 1);
				if (i_close == String::npos)
					return Address();

				int index = FromString<int>(item.substr(i_open + 1, i_close - i_open), -1);
				if (index < 0)
					return Address();

				address.emplace_back(index);

				i_open = item.find('[', i_close + 1);
			}
			// TODO: Abort on invalid characters among [ ] and after the last found bracket?
		}

		return address;
	};

	Variant Model::GetValue(const Rml::Core::String& address_str) const
	{
		Variable variable = GetVariable(address_str);

		Variant result;
		if (!variable)
			return result;

		if (variable.Type() != VariableType::Scalar)
		{
			Log::Message(Log::LT_WARNING, "Error retrieving data variable '%s': Only the values of scalar variables can be parsed.", address_str.c_str());
			return result;
		}
		if(!variable.Get(result))
			Log::Message(Log::LT_WARNING, "Could not parse data value '%s'", address_str.c_str());

		return result;
	}


	bool Model::SetValue(const String& address_str, const Variant& variant) const
	{
		Variable variable = GetVariable(address_str);

		if (!variable)
			return false;

		if (variable.Type() != VariableType::Scalar)
		{
			Log::Message(Log::LT_WARNING, "Could not assign data value '%s', variable is not a scalar type.", address_str.c_str());
			return false;
		}

		if(!variable.Set(variant))
		{
			Log::Message(Log::LT_WARNING, "Could not assign data value '%s'", address_str.c_str());
			return false;
		}

		return true;
	}

	bool Model::Bind(String name, void* ptr, VariableDefinition* variable, VariableType type)
	{
		RMLUI_ASSERT(ptr);
		if (!variable)
		{
			Log::Message(Log::LT_WARNING, "No registered type could be found for the data variable '%s'.", name.c_str());
			return false;
		}

		if (variable->Type() != type)
		{
			Log::Message(Log::LT_WARNING, "The registered type does not match the given type for the data variable '%s'.", name.c_str());
			return false;
		}

		bool inserted = variables.emplace(name, Variable(variable, ptr)).second;
		if (!inserted)
		{
			Log::Message(Log::LT_WARNING, "Data model variable with name '%s' already exists.", name.c_str());
			return false;
		}

		return true;
	}

	Variable Model::GetVariable(const String& address_str) const
	{
		Address address = ParseAddress(address_str);

		if (address.empty() || address.front().name.empty())
		{
			Log::Message(Log::LT_WARNING, "Invalid data address '%s'.", address_str.c_str());
			return Variable();
		}

		Variable instance = GetVariable(address);
		if (!instance)
		{
			Log::Message(Log::LT_WARNING, "Could not find the data variable '%s'.", address_str.c_str());
			return Variable();
		}

		return instance;
	}

	Variable Model::GetVariable(const Address& address) const
	{
		if (address.empty() || address.front().name.empty())
			return Variable();

		auto it = variables.find(address.front().name);
		if (it == variables.end())
			return Variable();

		Variable variable = it->second;

		for (int i = 1; i < (int)address.size() && variable; i++)
		{
			variable = variable.GetChild(address[i]);
			if (!variable)
				return Variable();
		}

		return variable;
	}

}



void TestDataVariable()
{
	using namespace Rml::Core;
	using namespace Data;

	using IntVector = std::vector<int>;

	struct FunData {
		int i = 99;
		String x = "hello";
		IntVector magic = { 3, 5, 7, 11, 13 };
	};

	using FunArray = std::array<FunData, 3>;

	struct SmartData {
		bool valid = true;
		FunData fun;
		FunArray more_fun;
	};

	TypeRegister types;

	{
		auto int_vector_handle = types.RegisterArray<IntVector>();

		auto fun_handle = types.RegisterStruct<FunData>();
		if (fun_handle)
		{
			fun_handle.RegisterMember("i", &FunData::i);
			fun_handle.RegisterMember("x", &FunData::x);
			fun_handle.RegisterMember("magic", &FunData::magic, int_vector_handle);
		}

		auto fun_array_handle = types.RegisterArray<FunArray>(fun_handle);

		auto smart_handle = types.RegisterStruct<SmartData>();
		if (smart_handle)
		{
			smart_handle.RegisterMember("valid", &SmartData::valid);
			smart_handle.RegisterMember("fun", &SmartData::fun, fun_handle);
			smart_handle.RegisterMember("more_fun", &SmartData::more_fun, fun_array_handle);
		}
	}

	Model model(&types);

	SmartData data;
	data.fun.x = "Hello, we're in SmartData!";

	model.BindStruct("data", &data);

	{
		std::vector<String> test_addresses = { "data.more_fun[1].magic[3]", "data.fun.x", "data.valid" };
		std::vector<String> expected_results = { ToString(data.more_fun[1].magic[3]), ToString(data.fun.x), ToString(data.valid) };

		std::vector<String> results;

		for(auto& address : test_addresses)
		{
			auto the_address = ParseAddress(address);

			Variant variant = model.GetValue(address);
			results.push_back(variant.Get<String>());
		}

		RMLUI_ASSERT(results == expected_results);

		bool success = model.SetValue("data.more_fun[1].magic[1]", Variant(String("199")));
		RMLUI_ASSERT(success && data.more_fun[1].magic[1] == 199);

		data.fun.magic = { 99, 190, 55, 2000, 50, 60, 70, 80, 90 };

		String result = model.GetValue("data.fun.magic[8]").Get<String>();
		RMLUI_ASSERT(result == "90");
	}

}





bool SetupDataBinding(Rml::Core::Context* context)
{
	my_model = context->CreateDataModel("my_model");
	if (!my_model)
		return false;

	my_model.BindValue("hello_world", &my_data.hello_world);
	my_model.BindValue("rating", &my_data.rating);
	my_model.BindValue("good_rating", &my_data.good_rating);

	auto invader_type = my_model.RegisterType<Invader>();
	invader_type.RegisterMember("name", &Invader::name);
	invader_type.RegisterMember("sprite", &Invader::sprite);
	invader_type.RegisterMember("color", &Invader::color);

	my_model.BindTypeValue("invader", &my_data.invader);

	my_model.BindContainer("indices", &my_data.indices);

	TestDataVariable();

	return true;
}



Rml::Core::Context* context = nullptr;
ShellRenderInterfaceExtensions *shell_renderer;
std::unique_ptr<DemoWindow> demo_window;

void GameLoop()
{
	my_model.UpdateControllers();
	my_data.good_rating = (my_data.rating > 50);
	my_model.UpdateViews();

	demo_window->Update();
	context->Update();

	shell_renderer->PrepareRenderBuffer();
	context->Render();
	shell_renderer->PresentRenderBuffer();
}




class DemoEventListener : public Rml::Core::EventListener
{
public:
	DemoEventListener(const Rml::Core::String& value, Rml::Core::Element* element) : value(value), element(element) {}

	void ProcessEvent(Rml::Core::Event& event) override
	{
		using namespace Rml::Core;

		if (value == "exit")
		{
			Shell::RequestExit();
		}
	}

	void OnDetach(Rml::Core::Element* element) override { delete this; }

private:
	Rml::Core::String value;
	Rml::Core::Element* element;
};



class DemoEventListenerInstancer : public Rml::Core::EventListenerInstancer
{
public:
	Rml::Core::EventListener* InstanceEventListener(const Rml::Core::String& value, Rml::Core::Element* element) override
	{
		return new DemoEventListener(value, element);
	}
};


#if defined RMLUI_PLATFORM_WIN32
#include <windows.h>
int APIENTRY WinMain(HINSTANCE RMLUI_UNUSED_PARAMETER(instance_handle), HINSTANCE RMLUI_UNUSED_PARAMETER(previous_instance_handle), char* RMLUI_UNUSED_PARAMETER(command_line), int RMLUI_UNUSED_PARAMETER(command_show))
#else
int main(int RMLUI_UNUSED_PARAMETER(argc), char** RMLUI_UNUSED_PARAMETER(argv))
#endif
{
#ifdef RMLUI_PLATFORM_WIN32
	RMLUI_UNUSED(instance_handle);
	RMLUI_UNUSED(previous_instance_handle);
	RMLUI_UNUSED(command_line);
	RMLUI_UNUSED(command_show);
#else
	RMLUI_UNUSED(argc);
	RMLUI_UNUSED(argv);
#endif

	const int width = 1600;
	const int height = 900;

	ShellRenderInterfaceOpenGL opengl_renderer;
	shell_renderer = &opengl_renderer;

	// Generic OS initialisation, creates a window and attaches OpenGL.
	if (!Shell::Initialise() ||
		!Shell::OpenWindow("Data Binding Sample", shell_renderer, width, height, true))
	{
		Shell::Shutdown();
		return -1;
	}

	// RmlUi initialisation.
	Rml::Core::SetRenderInterface(&opengl_renderer);
	opengl_renderer.SetViewport(width, height);

	ShellSystemInterface system_interface;
	Rml::Core::SetSystemInterface(&system_interface);

	Rml::Core::Initialise();

	// Create the main RmlUi context and set it on the shell's input layer.
	context = Rml::Core::CreateContext("main", Rml::Core::Vector2i(width, height));

	if (!context || !SetupDataBinding(context))
	{
		Rml::Core::Shutdown();
		Shell::Shutdown();
		return -1;
	}

	Rml::Controls::Initialise();
	Rml::Debugger::Initialise(context);
	Input::SetContext(context);
	shell_renderer->SetContext(context);
	
	DemoEventListenerInstancer event_listener_instancer;
	Rml::Core::Factory::RegisterEventListenerInstancer(&event_listener_instancer);

	Shell::LoadFonts("assets/");

	demo_window = std::make_unique<DemoWindow>("Data binding", Rml::Core::Vector2f(150, 50), context);
	demo_window->GetDocument()->AddEventListener(Rml::Core::EventId::Keydown, demo_window.get());
	demo_window->GetDocument()->AddEventListener(Rml::Core::EventId::Keyup, demo_window.get());

	Shell::EventLoop(GameLoop);

	demo_window->Shutdown();

	// Shutdown RmlUi.
	Rml::Core::Shutdown();

	Shell::CloseWindow();
	Shell::Shutdown();

	demo_window.reset();

	return 0;
}
