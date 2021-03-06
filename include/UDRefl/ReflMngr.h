#pragma once

#include "Info.h"

namespace Ubpa::UDRefl {
	constexpr Type GlobalType = TypeIDRegistry::Meta::global;
	constexpr ObjectView Global = { GlobalType, nullptr };

	extern ReflMngr* Mngr;
	extern const ObjectView MngrView;

	class ReflMngr {
	public:
		static ReflMngr& Instance() noexcept {
			static ReflMngr instance;
			return instance;
		}

		//
		// Data
		/////////
		//
		// enum is a special type (all member is static)
		//

		mutable NameIDRegistry nregistry;
		mutable TypeIDRegistry tregistry;

		std::unordered_map<Type, TypeInfo> typeinfos;

		// remove cvref
		TypeInfo* GetTypeInfo(Type type) const;

		std::pmr::synchronized_pool_resource* GetTemporaryResource() const { return &temporary_resource; }
		std::pmr::synchronized_pool_resource* GetObjectResource() const { return &object_resource; }

		// clear order
		// - field attrs
		// - type attrs
		// - type dynamic shared field
		// - typeinfos
		// - temporary_resource
		// - object_resource
		void Clear() noexcept;

		//
		// Traits
		///////////

		bool ContainsVirtualBase(Type type) const;

		//
		// Factory
		////////////
		//
		// - we will register the value type when generating FieldPtr, so those APIs aren't static
		//

		// field_data can be:
		// - static field: pointer to **non-void** type
		// - member object pointer
		// - enumerator
		template<auto field_data>
		FieldPtr GenerateFieldPtr();

		// data can be:
		// 1. member object pointer
		// 2. pointer to **non-void** and **non-function** type
		// 3. functor : Value*(Object*)  / Value&(Object*)
		// 4. enumerator
		template<typename T>
		FieldPtr GenerateFieldPtr(T&& data);

		// if T is bufferable, T will be stored as buffer,
		// else we will use MakeShared to store it
		template<typename T, typename... Args>
		FieldPtr GenerateDynamicFieldPtr(Args&&... args);

		template<typename... Params>
		static ParamList GenerateParamList() noexcept(sizeof...(Params) == 0);

		// funcptr can be
		// 1. member method : member function pointer
		// 2. static method : function pointer
		template<auto funcptr>
		static MethodPtr GenerateMethodPtr();

		// void(T&, Args...)
		template<typename T, typename... Args>
		static MethodPtr GenerateConstructorPtr();

		// void(const T&)
		template<typename T>
		static MethodPtr GenerateDestructorPtr();

		// Func: Ret(const? Object&, Args...)
		template<typename Func>
		static MethodPtr GenerateMemberMethodPtr(Func&& func);

		// Func: Ret(Args...)
		template<typename Func>
		static MethodPtr GenerateStaticMethodPtr(Func&& func);

		template<typename Derived, typename Base>
		static BaseInfo GenerateBaseInfo();

		//
		// Modifier
		/////////////

		Type RegisterType(Type type, size_t size, size_t alignment, bool is_polymorphic = false);
		Name AddField(Type type, Name field_name, FieldInfo fieldinfo);
		Name AddMethod(Type type, Name method_name, MethodInfo methodinfo);
		Type AddBase(Type derived, Type base, BaseInfo baseinfo);
		bool AddTypeAttr(Type type, Attr attr);
		bool AddFieldAttr(Type type, Name field_name, Attr attr);
		bool AddMethodAttr(Type type, Name method_name, Attr attr);
		
		Name AddTrivialDefaultConstructor(Type type);
		Name AddTrivialCopyConstructor   (Type type);
		Name AddZeroDefaultConstructor   (Type type);
		Name AddDefaultConstructor       (Type type);
		Name AddDestructor               (Type type);

		// - data-driven

		// require
		// - all bases aren't polymorphic and don't contain any virtual base
		// - field_types.size() == field_names.size()
		// auto compute
		// - size & alignment of type
		// - baseinfos
		// - fields' forward offset value
		Type RegisterType(
			Type type,
			std::span<const Type> bases,
			std::span<const Type> field_types,
			std::span<const Name> field_names
		);

		// -- template --

		// call
		// - RegisterType(type_name<T>(), sizeof(T), alignof(T), std::is_polymorphic<T>)
		// - details::TypeAutoRegister<T>::run
		// you can custom type register by specialize details::TypeAutoRegister<T>
		template<typename T>
		void RegisterType();

		// get TypeID from field_data
		// field_data can be
		// 1. member object pointer
		// 2. enumerator
		template<auto field_data>
		bool AddField(Name name, AttrSet attrs = {});

		// data can be:
		// 1. member object pointer
		// 2. enumerator
		// 3. pointer to **non-void** and **non-function** type
		// 4. functor : Value*(Object*) / Value&(Object*)
		template<typename T> requires std::negation_v<std::is_same<std::decay_t<T>, FieldInfo>>
		bool AddField(Type type, Name name, T&& data, AttrSet attrs = {})
		{ return AddField(type, name, { GenerateFieldPtr(std::forward<T>(data)), std::move(attrs) }); }

		// data can be:
		// 1. member object pointer
		// 2. functor : Value*(Object*)
		// > - result must be an pointer of **non-void** type
		// 3. enumerator
		template<typename T> requires std::negation_v<std::is_same<std::decay_t<T>, FieldInfo>>
		bool AddField(Name name, T&& data, AttrSet attrs = {});

		template<typename T, typename... Args>
		bool AddDynamicFieldWithAttr(Type type, Name name, AttrSet attrs, Args&&... args)
		{ return AddField(type, name, { GenerateDynamicFieldPtr<T>(std::forward<Args>(args)...), std::move(attrs) }); }

		template<typename T, typename... Args>
		bool AddDynamicField(Type type, Name name, Args&&... args)
		{ return AddDynamicFieldWithAttr<T>(type, name, {}, std::forward<Args>(args)...); }

		// funcptr is member function pointer
		// get TypeID from funcptr
		template<auto member_func_ptr>
		bool AddMethod(Name name, AttrSet attrs = {});

		// funcptr is function pointer
		template<auto func_ptr>
		bool AddMethod(Type type, Name name, AttrSet attrs = {});

		template<typename T, typename... Args>
		bool AddConstructor(AttrSet attrs = {});
		template<typename T>
		bool AddDestructor(AttrSet attrs = {});

		// Func: Ret(const? Object&, Args...)
		template<typename Func>
		bool AddMemberMethod(Name name, Func&& func, AttrSet attrs = {});

		// Func: Ret(Args...)
		template<typename Func>
		bool AddStaticMethod(Type type, Name name, Func&& func, AttrSet attrs = {})
		{ return AddMethod(type, name, { GenerateStaticMethodPtr(std::forward<Func>(func)), std::move(attrs) }); }

		template<typename Derived, typename... Bases>
		bool AddBases();

		//
		// Cast
		/////////

		ObjectView StaticCast_DerivedToBase (ObjectView obj, Type type) const;
		ObjectView StaticCast_BaseToDerived (ObjectView obj, Type type) const;
		ObjectView DynamicCast_BaseToDerived(ObjectView obj, Type type) const;
		ObjectView StaticCast               (ObjectView obj, Type type) const;
		ObjectView DynamicCast              (ObjectView obj, Type type) const;

		//
		// Var
		////////
		//
		// - result type of Var maintains the CVRefMode of the input
		//

		ObjectView Var(ObjectView obj,            Name field_name, FieldFlag flag = FieldFlag::All) const;
		// for diamond inheritance
		ObjectView Var(ObjectView obj, Type base, Name field_name, FieldFlag flag = FieldFlag::All) const;

		//
		// Invoke
		///////////
		//
		// - 'A' means auto, ObjectView/SharedObject will be transformed as type + ptr
		// - 'B' means basic
		// - 'D' means default
		// - 'M' means memory
		// - auto search methods in bases
		// - support overload
		// - require IsCompatible()
		// - MInvoke will allocate buffer for result, and move to SharedObject
		// - if result is a reference, SharedObject is a ObjectView actually
		// - if result is ObjectView or SharedObject, then MInvoke's result is it.
		// - temp_args_rsrc is used for temporary allocation of arguments (release before return)
		//

		// parameter <- argument
		// - ObjectView
		// - same
		// - reference
		// > - 0 (invalid), 1 (convertible), 2 (constructible)
		// > - table
		//     |    -     | T | T & | const T & | T&& | const T&& |
		//     |      T   | - |  2  |     2     |  1  |     2     |
		//     |      T & | 0 |  -  |     0     |  0  |     0     |
		//     |const T & | 1 |  1  |     -     |  1  |     1     |
		//     |      T&& | 1 |  0  |     0     |  -  |     0     |
		//     |const T&& | 1 |  0  |     0     |  1  |     -     |
		// - pointer and array (non cvref)
		// > - 0 (invalid), 1 (convertible)
		// > - table
		//     |     -     | T * | const T * | T[] | const T[] |
		//     |       T * |  -  |     0     |  1  |     0     |
		//     | const T * |  1  |     -     |  1  |     1     |
		//     |       T[] |  1  |     0     | -/1 |     0     |
		//     | const T[] |  1  |     1     |  1  |    -/1    |
		bool IsCompatible(std::span<const Type> paramTypes, std::span<const Type> argTypes) const;

		Type IsInvocable(Type type, Name method_name, std::span<const Type> argTypes = {}, MethodFlag flag = MethodFlag::All) const;

		Type BInvoke(
			ObjectView obj,
			Name method_name,
			void* result_buffer = nullptr,
			ArgsView args = {},
			MethodFlag flag = MethodFlag::All,
			std::pmr::memory_resource* temp_args_rsrc = Mngr->GetTemporaryResource()) const;

		SharedObject MInvoke(
			ObjectView obj,
			Name method_name,
			std::pmr::memory_resource* rst_rsrc,
			std::pmr::memory_resource* temp_args_rsrc,
			ArgsView args = {},
			MethodFlag flag = MethodFlag::All) const;

		SharedObject Invoke(
			ObjectView obj,
			Name method_name,
			ArgsView args = {},
			MethodFlag flag = MethodFlag::All) const
		{ return MInvoke(obj, method_name, &object_resource, &temporary_resource, args, flag); }

		// -- template --

		template<typename... Args>
		Type IsInvocable(Type type, Name method_name, MethodFlag flag = MethodFlag::All) const;

		template<typename T>
		T BInvokeRet(
			ObjectView obj,
			Name method_name,
			ArgsView args = {},
			MethodFlag flag = MethodFlag::All,
			std::pmr::memory_resource* temp_args_rsrc = Mngr->GetTemporaryResource()) const;

		template<typename T, typename... Args>
		T BInvoke(ObjectView obj, Name method_name, MethodFlag flag, std::pmr::memory_resource* temp_args_rsrc, Args&&... args) const;

		template<typename... Args>
		SharedObject MInvoke(
			ObjectView obj,
			Name method_name,
			std::pmr::memory_resource* rst_rsrc,
			std::pmr::memory_resource* temp_args_rsrc,
			MethodFlag flag,
			Args&&... args) const;

		template<typename... Args>
		SharedObject Invoke(
			ObjectView obj,
			Name method_name,
			Args&&... args) const;

		//
		// Make
		/////////
		//
		// - if the type doesn't contains any ctor, then we use trivial ctor (do nothing)
		// - if the type doesn't contains any dtor, then we use trivial dtor (do nothing)
		//

		bool IsConstructible    (Type type, std::span<const Type> argTypes = {}) const;
		bool IsCopyConstructible(Type type) const;
		bool IsMoveConstructible(Type type) const;
		bool IsDestructible     (Type type) const;

		bool Construct(ObjectView obj, ArgsView args = {}) const;
		void Destruct (ObjectView obj) const;

		ObjectView   MNew       (Type      type, std::pmr::memory_resource* rsrc, ArgsView args = {}) const;
		SharedObject MMakeShared(Type      type, std::pmr::memory_resource* rsrc, ArgsView args = {}) const;
		bool         MDelete    (ObjectView obj, std::pmr::memory_resource* rsrc                    ) const;

		ObjectView   New       (Type      type, ArgsView args = {}) const;
		SharedObject MakeShared(Type      type, ArgsView args = {}) const;
		bool         Delete    (ObjectView obj                    ) const;

		// -- template --

		template<typename... Args> bool IsConstructible(Type type) const;

		template<typename... Args> bool Construct(ObjectView obj, Args&&... args) const;

		template<typename... Args> ObjectView   MNew       (Type type, std::pmr::memory_resource* rsrc, Args&&... args) const;
		template<typename... Args> SharedObject MMakeShared(Type type, std::pmr::memory_resource* rsrc, Args&&... args) const;

		template<typename... Args> ObjectView   New       (Type type, Args&&... args) const;
		template<typename... Args> SharedObject MakeShared(Type type, Args&&... args) const;

		// - if T is not register, call RegisterType<T>()
		// - call AddConstructor<T, Args...>()
		template<typename T, typename... Args>
		ObjectView NewAuto(Args... args);

		// - if T is not register, call RegisterType<T>()
		// - call AddConstructor<T, Args...>()
		template<typename T, typename... Args>
		SharedObject MakeSharedAuto(Args... args);

		//
		// Algorithm
		//////////////

		// ForEach (DFS)

		void ForEachTypeInfo(Type type,
			const std::function<bool(InfoTypePair)>& func) const;
		void ForEachField(Type type,
			const std::function<bool(InfoTypePair, InfoFieldPair)>& func, FieldFlag flag = FieldFlag::All) const;
		void ForEachMethod(Type type,
			const std::function<bool(InfoTypePair, InfoMethodPair)>& func, MethodFlag flag = MethodFlag::All) const;
		void ForEachVar(ObjectView obj,
			const std::function<bool(InfoTypePair, InfoFieldPair, ObjectView)>& func, FieldFlag flag = FieldFlag::All) const;

		// Gather (DFS)

		std::vector<InfoTypeFieldPair>                                   GetTypeFields   (Type      type, FieldFlag  flag = FieldFlag ::All);
		std::vector<InfoTypeMethodPair>                                  GetTypeMethods  (Type      type, MethodFlag flag = MethodFlag::All);
		std::vector<std::tuple<InfoTypePair, InfoFieldPair, ObjectView>> GetTypeFieldVars(ObjectView obj, FieldFlag  flag = FieldFlag ::All);

		std::vector<InfoTypePair>   GetTypes  (Type      type);
		std::vector<InfoFieldPair>  GetFields (Type      type, FieldFlag  flag = FieldFlag ::All);
		std::vector<InfoMethodPair> GetMethods(Type      type, MethodFlag flag = MethodFlag::All);
		std::vector<ObjectView>     GetVars   (ObjectView obj, FieldFlag  flag = FieldFlag ::All);

		// Find (DFS)

		InfoTypePair   FindType  (Type      type, const std::function<bool(InfoTypePair  )>& func) const;
		InfoFieldPair  FindField (Type      type, const std::function<bool(InfoFieldPair )>& func, FieldFlag  flag = FieldFlag ::All) const;
		InfoMethodPair FindMethod(Type      type, const std::function<bool(InfoMethodPair)>& func, MethodFlag flag = MethodFlag::All) const;
		ObjectView     FindVar   (ObjectView obj, const std::function<bool(ObjectView    )>& func, FieldFlag  flag = FieldFlag ::All) const;

		// Contains (DFS)

		bool ContainsBase  (Type type, Type base       ) const;
		bool ContainsField (Type type, Name field_name , FieldFlag  flag = FieldFlag ::All) const;
		bool ContainsMethod(Type type, Name method_name, MethodFlag flag = MethodFlag::All) const;

	private:
		ReflMngr();
		~ReflMngr();

		// for
		// - argument copy
		// - user argument buffer
		mutable std::pmr::synchronized_pool_resource temporary_resource;

		// for
		// - New
		mutable std::pmr::synchronized_pool_resource object_resource;
	};
}

#include "details/ReflMngr.inl"
