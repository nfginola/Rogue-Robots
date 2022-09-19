#pragma once
#include "LuaW.h"

class LuaContext;

class LuaTable
{
	friend LuaContext;

private:
	LuaW* m_luaW;
	Table m_table;

private:
	Table& GetTable();

public:
	LuaTable(LuaW* luaW);
	LuaTable(LuaW* luaW, Table& table, bool addReference = false);
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
	void AddFunctionToTable(const std::string& name);

	LuaTable CreateTableInTable(const std::string& name);

	int GetIntFromTable(const std::string& name);
	float GetFloatFromTable(const std::string& name);
	double GetDoubleFromTable(const std::string& name);
	std::string GetStringFromTable(const std::string& name);
	bool GetBoolFromTable(const std::string& name);
	LuaTable GetTableFromTable(const std::string& name);

	Function GetFunctionFromTable(const std::string& name);
	Function TryGetFunctionFromTable(const std::string& name);
	void CallFunctionOnTable(Function function);
	void CallFunctionOnTable(const std::string& name);

	void RemoveReferenceToTable();
};

template<void(*func)(LuaContext*)>
inline void LuaTable::AddFunctionToTable(const std::string& name)
{
	m_luaW->AddFunctionToTable<func>(m_table, name); 
}
