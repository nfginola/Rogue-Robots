#pragma once
#include "LuaW.h"

namespace DOG
{
	class LuaContext;

	class LuaTable
	{
		friend LuaContext;
		friend LuaW;

	private:
		LuaW* m_luaW;
		Table m_table;

	private:
		Table& GetTable();
		void CallFunctionOnTableNoReturn(Function function);
		void CallFunctionOnTableNoReturn(const std::string& name);

	public:
		LuaTable();
		LuaTable(Table& table, bool addReference = false);
		~LuaTable();
		LuaTable(const LuaTable& table);
		LuaTable& operator=(const LuaTable& table);

		void CreateEnvironment(const std::string& luaFileName);
		bool TryCreateEnvironment(const std::string& luaFileName);

		void AddIntToTable(const std::string& name, int value);
		void AddFloatToTable(const std::string& name, float value);
		void AddDoubleToTable(const std::string& name, double value);
		void AddStringToTable(const std::string& name, const std::string& string);
		void AddBoolToTable(const std::string& name, bool boolean);
		void AddTableToTable(const std::string& name, LuaTable table);
		void AddFunctionToTable(const std::string& name, Function& function);
		template <void (*func)(LuaContext*)>
		Function AddFunctionToTable(const std::string& name);
		template <typename T>
		UserData AddUserDataToTable(const std::string& name, T* object, const std::string& interfaceName);
		void AddUserDataToTable(const std::string& name, UserData& userData);

		void AddIntToTable(int index, int value);
		void AddFloatToTable(int index, float value);
		void AddDoubleToTable(int index, double value);
		void AddStringToTable(int index, const std::string& string);
		void AddBoolToTable(int index, bool boolean);
		void AddTableToTable(int index, LuaTable table);
		void AddFunctionToTable(int index, Function& function);
		template <void (*func)(LuaContext*)>
		Function AddFunctionToTable(int index);
		template <typename T>
		UserData AddUserDataToTable(int index, T* object, const std::string& interfaceName);
		void AddUserDataToTable(int index, UserData& userData);

		LuaTable CreateTableInTable(const std::string& name);

		int GetIntFromTable(const std::string& name);
		float GetFloatFromTable(const std::string& name);
		double GetDoubleFromTable(const std::string& name);
		std::string GetStringFromTable(const std::string& name);
		bool GetBoolFromTable(const std::string& name);
		LuaTable GetTableFromTable(const std::string& name);
		//Get Pointer from the UserData from Lua table
		template <typename T>
		T* GetUserDataPointerFromTable(const std::string& name);
		//Get UserData from Lua table
		UserData GetUserDataFromTable(const std::string& name);

		int GetIntFromTable(int index);
		float GetFloatFromTable(int index);
		double GetDoubleFromTable(int index);
		std::string GetStringFromTable(int index);
		bool GetBoolFromTable(int index);
		LuaTable GetTableFromTable(int index);
		//Get Pointer from the UserData from Lua table
		template <typename T>
		T* GetUserDataPointerFromTable(int index);
		//Get UserData from Lua table
		UserData GetUserDataFromTable(int index);

		Function GetFunctionFromTable(const std::string& name);
		Function TryGetFunctionFromTable(const std::string& name);
		template <class... Args>
		LuaFunctionReturn CallFunctionOnTable(Function function, Args&&... args);
		template <class... Args>
		LuaFunctionReturn CallFunctionOnTable(const std::string& name, Args&&... args);
		LuaFunctionReturn CallFunctionOnTable(Function function);
		LuaFunctionReturn CallFunctionOnTable(const std::string& name);

		void RemoveReferenceToTable();
	};

	template<void(*func)(LuaContext*)>
	inline Function LuaTable::AddFunctionToTable(const std::string& name)
	{
		m_luaW->AddFunctionToTable<func>(m_table, name);
		return GetFunctionFromTable(name);
	}

	template<typename T>
	inline UserData LuaTable::AddUserDataToTable(const std::string& name, T* object, const std::string& interfaceName)
	{
		m_luaW->AddUserDataToTable(m_table, object, interfaceName, name);
		return GetUserDataFromTable(name);
	}

	template<void(*func)(LuaContext*)>
	inline Function LuaTable::AddFunctionToTable(int index)
	{
		m_luaW->AddFunctionToTable<func>(m_table, ++index);
		//The get function handles the index inside so we do not need to increase it here
		return GetFunctionFromTable(index);
	}

	template<typename T>
	inline UserData LuaTable::AddUserDataToTable(int index, T* object, const std::string& interfaceName)
	{
		m_luaW->AddUserDataToTable(m_table, object, interfaceName, ++index);
		//The get function handles the index inside so we do not need to increase it here
		return GetUserDataFromTable(index);
	}

	template<typename T>
	inline T* LuaTable::GetUserDataPointerFromTable(const std::string& name)
	{
		return m_luaW->GetUserDataPointer<T>(m_luaW->GetUserDataFromTable(m_table, name));
	}

	template<typename T>
	inline T* LuaTable::GetUserDataPointerFromTable(int index)
	{
		return m_luaW->GetUserDataPointer<T>(m_luaW->GetUserDataFromTable(m_table, ++index));
	}

	template<class ...Args>
	inline LuaFunctionReturn LuaTable::CallFunctionOnTable(Function function, Args&& ...args)
	{
		//Get the amount of arguments
		int argumentSize = sizeof...(Args);
		//Push the arguments to the stack
		m_luaW->PushStack(std::forward<Args>(args)...);

		return m_luaW->CallTableLuaFunctionReturn(m_table, function, argumentSize);
	}

	template<class ...Args>
	inline LuaFunctionReturn LuaTable::CallFunctionOnTable(const std::string& name, Args&& ...args)
	{
		//Get function
		Function function = m_luaW->GetFunctionFromTable(m_table, name);
		//Get the amount of arguments
		int argumentSize = sizeof...(Args);
		//Push the arguments to the stack
		m_luaW->PushStack(std::forward<Args>(args)...);

		auto luaReturns = m_luaW->CallTableLuaFunctionReturn(m_table, function, argumentSize);
		m_luaW->RemoveReferenceToFunction(function);
		return luaReturns;
	}
}