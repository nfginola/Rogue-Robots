#include "LuaW.h"

LuaW::LuaW(lua_State* l)
{
	m_luaState = l;
}

bool LuaW::IsUserData(int index) const
{
	return lua_isuserdata(m_luaState, index);
}

void LuaW::Error(const std::string& errorMessage)
{
	std::cout << "ERROR: " << errorMessage << "\n";
	PrintStack();
	assert(false);
}

void LuaW::RunScript(const std::string& luaFileName)
{
	//Load lua file and run it
	int error = luaL_loadfile(m_luaState, luaFileName.c_str()) || lua_pcall(m_luaState, 0, LUA_MULTRET, 0);
	if (error)
	{
		std::cout << lua_tostring(m_luaState, -1) << std::endl;
		assert(false);
		lua_pop(m_luaState, 1);
	}
}

//Sets an integer to a global variable
void LuaW::PushGlobalNumber(const std::string& luaGlobalName, int number)
{
	PushIntegerToStack(number);
	SetGlobal(luaGlobalName);
}

//Sets an float to a global variable
void LuaW::PushGlobalNumber(const std::string& luaGlobalName, float number)
{
	PushFloatToStack(number);
	SetGlobal(luaGlobalName);
}

//Sets an double to a global variable
void LuaW::PushGlobalNumber(const std::string& luaGlobalName, double number)
{
	PushDoubleToStack(number);
	SetGlobal(luaGlobalName);
}

//Sets an string to a global variable
void LuaW::PushGlobalString(const std::string& luaGlobalName, const std::string& string)
{
	PushStringToStack(string);
	SetGlobal(luaGlobalName);
}

//Sets an string to a global variable
void LuaW::PushGlobalString(const std::string& luaGlobalName, const char* string)
{
	PushStringToStack(string);
	SetGlobal(luaGlobalName);
}

//Sets an bool to a global variable
void LuaW::PushGlobalBool(const std::string& luaGlobalName, bool boolean)
{
	PushBoolToStack(boolean);
	SetGlobal(luaGlobalName);
}

//Create an interface for lua
RegisterClassFunctions LuaW::CreateLuaInterface(const std::string& interfaceName)
{
	RegisterClassFunctions reg;
	reg.className = interfaceName;
	return reg;
}

//Pushes the interface to lua
void LuaW::PushLuaInterface(RegisterClassFunctions& registerInterface)
{
	std::vector<luaL_Reg> lua_regs; 

	for (int i = 0; i < registerInterface.classFunctions.size(); ++i)
	{
		lua_regs.push_back({ registerInterface.classFunctions[i].functionName.c_str(), registerInterface.classFunctions[i].classFunction});
	}
	//Have to have null, null at the end!
	lua_regs.push_back({NULL, NULL});

	//Create a metatable on the stack (an table)
	luaL_newmetatable(m_luaState, registerInterface.className.c_str());
	//Sets the functions to the metatable
	luaL_setfuncs(m_luaState, lua_regs.data(), 0);
	//Copies the metatable on the stack
	lua_pushvalue(m_luaState, -1);
	//Sets the __index to the metatable (to it self)
	lua_setfield(m_luaState, -1, "__index");
	//Create the interface for lua
	SetGlobal(registerInterface.className);
}

void LuaW::PrintStack()
{
	std::cout << "Stack:\n";
	int top = lua_gettop(m_luaState);
	for (int i = 1; i <= top; i++)
	{
		printf("%d\t%s\t", i, luaL_typename(m_luaState, i));
		switch (lua_type(m_luaState, i))
		{
		case LUA_TNUMBER:
			std::cout << lua_tonumber(m_luaState, i) << std::endl; 
			break;
		case LUA_TSTRING:
			std::cout << lua_tostring(m_luaState, i) << std::endl; 
			break;
		case LUA_TBOOLEAN:
			std::cout << (lua_toboolean(m_luaState, i) ? "true" : "false") << std::endl; 
			break;
		case LUA_TNIL:
			std::cout << "nil" << std::endl;
			break;
		default:
			std::cout << lua_topointer(m_luaState, i) << std::endl; 
			break;
		}
	}
}

//Pushes the global variable to the stack and then return it to the user
int LuaW::GetGlobalInteger(const std::string& luaGlobalName)
{
	GetGlobal(luaGlobalName);
	return GetIntegerFromStack();
}

//Pushes the global variable to the stack and then return it to the user
float LuaW::GetGlobalFloat(const std::string& luaGlobalName)
{
	GetGlobal(luaGlobalName);
	return GetFloatFromStack();
}

//Pushes the global variable to the stack and then return it to the user
double LuaW::GetGlobalDouble(const std::string& luaGlobalName)
{
	GetGlobal(luaGlobalName);
	return GetDoubleFromStack();
}

//Pushes the global variable to the stack and then return it to the user
std::string LuaW::GetGlobalString(const std::string& luaGlobalName)
{
	GetGlobal(luaGlobalName);
	return std::move(GetStringFromStack());
}

//Pushes the global variable to the stack and then return it to the user
bool LuaW::GetGlobalBool(const std::string& luaGlobalName)
{
	GetGlobal(luaGlobalName);
	return GetBoolFromStack();
}

//Pushes the global variable to the stack and then return it to the user
Function LuaW::GetGlobalFunction(const std::string& luaGlobalName)
{
	GetGlobal(luaGlobalName);
	return GetFunctionFromStack();
}

//Pushes the global variable to the stack and then return it to the user
Table LuaW::GetGlobalTable(const std::string& luaGlobalName)
{
	GetGlobal(luaGlobalName);
	return GetTableFromStack();
}

//Pushes the global variable to the stack and then return it to the user
UserData LuaW::GetGlobalUserData(const std::string& luaGlobalName)
{
	GetGlobal(luaGlobalName);
	return GetUserDataFromStack();
}

//Checks if the current index on the stack is an integer and return it
int LuaW::GetIntegerFromStack(int index)
{
	if (lua_isnumber(m_luaState, index))
	{
		int number = (int)lua_tonumber(m_luaState, index);
		lua_pop(m_luaState, index);
		return number;
	}
	else
	{
		Error("The value on the stack is not an number");
	}
	return 0;
}

//Checks if the current index on the stack is an float and return it
float LuaW::GetFloatFromStack(int index)
{
	if (lua_isnumber(m_luaState, index))
	{
		float number = (float)lua_tonumber(m_luaState, index);
		lua_pop(m_luaState, index);
		return number;
	}
	else
	{
		Error("The value on the stack is not an number");
	}
	return 0.0f;
}

//Checks if the current index on the stack is an double and return it
double LuaW::GetDoubleFromStack(int index)
{
	if (lua_isnumber(m_luaState, index))
	{
		double number = lua_tonumber(m_luaState, index);
		lua_pop(m_luaState, index);
		return number;
	}
	else
	{
		Error("The value on the stack is not an number");
	}
	return 0.0;
}

//Checks if the current index on the stack is an string and return it
std::string LuaW::GetStringFromStack(int index)
{
	if (lua_isstring(m_luaState, index))
	{
		std::string string = std::string(lua_tostring(m_luaState, index));
		lua_pop(m_luaState, index);
		return std::move(string);
	}
	else
	{
		Error("The value on the stack is not an number");
	}
	return std::string();
}

//Checks if the current index on the stack is an bool and return it
bool LuaW::GetBoolFromStack(int index)
{
	if (lua_isboolean(m_luaState, index))
	{
		bool boolean = lua_toboolean(m_luaState, index);
		lua_pop(m_luaState, index);
		return boolean;
	}
	else
	{
		Error("The value on the stack is not an boolean");
	}
	return false;
}

//Checks if the current index on the stack is an function and return it
Function LuaW::GetFunctionFromStack(int index)
{
	if (lua_isfunction(m_luaState, index))
	{
		Function function = {luaL_ref(m_luaState, LUA_REGISTRYINDEX)};
		return function;
	}
	else
	{
		Error("The value on the stack is not an function");
	}
	return Function(-1);
}

//Checks if the current index on the stack is an table and return it
Table LuaW::GetTableFromStack(int index)
{
	if (lua_istable(m_luaState, index))
	{
		Table table = { luaL_ref(m_luaState, LUA_REGISTRYINDEX) };
		return table;
	}
	else
	{
		Error("The value on the stack is not an table");
	}
	return Table(-1);
}

//Checks if the current index on the stack is an UserData and return it
UserData LuaW::GetUserDataFromStack(int index)
{
	if (lua_isuserdata(m_luaState, index))
	{
		UserData userData = { luaL_ref(m_luaState, LUA_REGISTRYINDEX) };
		return userData;
	}
	else
	{
		Error("The value on the stack is not an userdata");
	}
	return UserData(-1);
}

//Push an integer to the stack
void LuaW::PushIntegerToStack(int integer)
{
	lua_pushinteger(m_luaState, integer);
}

//Push an float to the stack
void LuaW::PushFloatToStack(float number)
{
	lua_pushnumber(m_luaState, number);
}

//Push an double to the stack
void LuaW::PushDoubleToStack(double number)
{
	lua_pushnumber(m_luaState, number);
}

//Push an string to the stack
void LuaW::PushStringToStack(const std::string& string)
{
	lua_pushstring(m_luaState, string.c_str());
}

//Push an string to the stack
void LuaW::PushStringToStack(const char* string)
{
	lua_pushstring(m_luaState, string);
}

//Push an bool to the stack
void LuaW::PushBoolToStack(bool boolean)
{
	lua_pushboolean(m_luaState, boolean);
}

//Push the function reference to the stack
void LuaW::PushFunctionToStack(Function& function)
{
	lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, function.ref);
}

//Push the table reference to the stack
void LuaW::PushTableToStack(Table& table)
{
	lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, table.ref);
}

//Push the UserData reference to the stack
void LuaW::PushUserDataToStack(UserData& userData)
{
	lua_rawgeti(m_luaState, LUA_REGISTRYINDEX, userData.ref);
}

//Takes the top of the stack and pushes it to lua with the name
void LuaW::SetGlobal(const std::string& luaGlobalName)
{
	lua_setglobal(m_luaState, luaGlobalName.c_str());
}

//Takes the global from lua and pushes it to the stack
void LuaW::GetGlobal(const std::string& luaGlobalName)
{
	lua_getglobal(m_luaState, luaGlobalName.c_str());
}

//Pushes two items on the stack into the table and then we pop the table from the stack
void LuaW::SetTableAndPop()
{
	lua_settable(m_luaState, -3);

	lua_pop(m_luaState, 1);
}

//Gets item from the table and then we remove it from the stack
void LuaW::GetTableAndRemove()
{
	lua_gettable(m_luaState, -2);

	lua_remove(m_luaState, 1);
}

void LuaW::CallLuaFunction(Function& function, int arguments)
{
	if (function.ref == -1)
	{
		Error("Function references nothing");
		return;
	}
	//Pushes the function to the stack
	PushFunctionToStack(function);
	//Insert the function to the bottom of the stack
	//The arguments need to be at the top and the function call needs to be last
	lua_insert(m_luaState, 1);
	//Call the function with the amount of arguments
	int error = lua_pcall(m_luaState, arguments, LUA_MULTRET, 0);
	if (error)
	{
		Error(lua_tostring(m_luaState, -1));
		assert(false);
		lua_pop(m_luaState, 1);
	}
}

void LuaW::CallTableLuaFunction(Table& table, Function& function, int arguments)
{
	if (function.ref == -1 || table.ref == -1)
	{
		Error("Table or Function references nothing");
		return;
	}

	//Pushes the function to the stack
	PushFunctionToStack(function);
	//Insert the function to the bottom of the stack
	//The arguments need to be at the top and the function call needs to be last
	lua_insert(m_luaState, 1);
	//Pushes the table to the stack
	PushTableToStack(table);
	//Insert the table above the function on the stack
	//If you want to be able to access private table variables in the function then this is needed
	//This is equivalent to writing table.function(table) or table:function()
	lua_insert(m_luaState, 2);

	int error = lua_pcall(m_luaState, 1 + arguments, LUA_MULTRET, 0);
	if (error)
	{
		Error(lua_tostring(m_luaState, -1));
		assert(false);
		lua_pop(m_luaState, 1);
	}
}

Table LuaW::CreateGlobalTable(const std::string& globalTableName)
{
	//Creates a table on the stack
	lua_newtable(m_luaState);

	//Copies the table on the stack
	lua_pushvalue(m_luaState, -1);

	//Take the copied table and pushes it as a global
	SetGlobal(globalTableName);

	//Then take the first table and create a refernce to the global table
	int index = luaL_ref(m_luaState, LUA_REGISTRYINDEX);

	Table table = {index};

	return table;
}

Table LuaW::CreateTable()
{
	//Creates a table on the stack
	lua_newtable(m_luaState);

	//Creates a reference to the table on the stack
	int index = luaL_ref(m_luaState, LUA_REGISTRYINDEX);

	Table table = { index };

	return table;
}

void LuaW::AddNumberToTable(Table& table, const std::string& numberName, int number)
{
	//Pushes the table on the stack
	PushTableToStack(table);
	//Pushes the name of the table on the stack
	PushStringToStack(numberName);
	//Pushes the number to the stack
	PushIntegerToStack(number);

	//Sets the number to the numberName in the table
	SetTableAndPop();
}

void LuaW::AddNumberToTable(Table& table, const std::string& numberName, float number)
{
	PushTableToStack(table);
	PushStringToStack(numberName);
	PushFloatToStack(number);

	//Sets the number to the numberName in the table
	SetTableAndPop();
}

void LuaW::AddNumberToTable(Table& table, const std::string& numberName, double number)
{
	PushTableToStack(table);
	PushStringToStack(numberName);
	PushDoubleToStack(number);

	//Sets the number to the numberName in the table
	SetTableAndPop();
}

void LuaW::AddStringToTable(Table& table, const std::string& stringName, const std::string& string)
{
	PushTableToStack(table);
	PushStringToStack(stringName);
	PushStringToStack(string);

	//Sets the string to the stringName in the table
	SetTableAndPop();
}

void LuaW::AddStringToTable(Table& table, const std::string& stringName, const char* string)
{
	PushTableToStack(table);
	PushStringToStack(stringName);
	PushStringToStack(string);

	//Sets the string to the stringName in the table
	SetTableAndPop();
}

void LuaW::AddBoolToTable(Table& table, const std::string& boolName, bool boolean)
{
	PushTableToStack(table);
	PushStringToStack(boolName);
	PushBoolToStack(boolean);

	//Sets the bool to the boolName in the table
	SetTableAndPop();
}

void LuaW::AddTableToTable(Table& table, const std::string& tableName, Table& addTable)
{
	PushTableToStack(table);
	PushStringToStack(tableName);
	PushTableToStack(addTable);

	//Sets the table to the tableName in the table
	SetTableAndPop();
}

void LuaW::AddFunctionToTable(Table& table, const std::string& functionName, Function& function)
{
	PushTableToStack(table);
	PushStringToStack(functionName);
	PushTableToStack(function);

	//Sets the function to the functionName in the table
	SetTableAndPop();
}

LuaW::LuaW()
{
	//Create the luaState and add the libs
	m_luaState = luaL_newstate();
	luaL_openlibs(m_luaState);
}

void LuaW::AddUserDataToTable(Table& table, const std::string& userDataName, UserData& userData)
{
	PushTableToStack(table);
	PushStringToStack(userDataName);
	PushTableToStack(userData);

	//Sets the userData to the userDataName in the table
	SetTableAndPop();
}

int LuaW::GetIntegerFromTable(Table& table, const std::string& tableIntegerName)
{
	//Pushes the table to the stack
	PushTableToStack(table);
	//Pushes the integerName to the stack
	PushStringToStack(tableIntegerName);

	//Get the integer from the table and remove the table from the stack
	GetTableAndRemove();

	return GetIntegerFromStack();
}

float LuaW::GetFloatFromTable(Table& table, const std::string& tableFloatName)
{
	PushTableToStack(table);
	PushStringToStack(tableFloatName);

	//Get the float from the table and remove the table from the stack
	GetTableAndRemove();

	return GetFloatFromStack();
}

double LuaW::GetDoubleFromTable(Table& table, const std::string& tableDoubleName)
{
	PushTableToStack(table);
	PushStringToStack(tableDoubleName);

	//Get the double from the table and remove the table from the stack
	GetTableAndRemove();

	return GetDoubleFromStack();
}

std::string LuaW::GetStringFromTable(Table& table, const std::string& tableStringName)
{
	PushTableToStack(table);
	PushStringToStack(tableStringName);

	//Get the string from the table and remove the table from the stack
	GetTableAndRemove();

	return std::move(GetStringFromStack());
}

bool LuaW::GetBoolFromTable(Table& table, const std::string& tableBoolName)
{
	PushTableToStack(table);
	PushStringToStack(tableBoolName);

	//Get the bool from the table and remove the table from the stack
	GetTableAndRemove();

	return GetBoolFromStack();
}

Function LuaW::GetFunctionFromTable(Table& table, const std::string& tableFunctionName)
{
	PushTableToStack(table);
	PushStringToStack(tableFunctionName);

	//Get the function from the table and remove the table from the stack
	GetTableAndRemove();

	return GetFunctionFromStack();
}

Table LuaW::GetTableFromTable(Table& table, const std::string& tableTableName)
{
	PushTableToStack(table);
	PushStringToStack(tableTableName);

	//Get the table from the table and remove the table from the stack
	GetTableAndRemove();

	return GetTableFromStack();
}

UserData LuaW::GetUserDataFromTable(Table& table, const std::string& tableUserDataName)
{
	PushTableToStack(table);
	PushStringToStack(tableUserDataName);

	//Get the UserData from the table and remove the table from the stack
	GetTableAndRemove();

	return GetUserDataFromStack();
}

//Load the lua file and pushes it to the stack
void LuaW::LoadChunk(const std::string& luaScriptName)
{
	int error = luaL_loadfile(m_luaState, luaScriptName.c_str());
	if (error)
		Error("Couldn't find file " + luaScriptName + "!");
}

void LuaW::CreateEnvironment(Table& table, const std::string& luaFileName)
{
	//Pushes the table to the stack
	PushTableToStack(table);

	//Pushes the lua file to the stack
	LoadChunk(luaFileName);

	//Creates a new table on the stack which is used as an metatable
	lua_newtable(m_luaState);

	//Get the global table and pusehs to the stack (lua defined table)
	GetGlobal("_G");

	//Sets the global table to the __index of the metatable
	lua_setfield(m_luaState, 3, "__index");

	//Sets the metatable to the table
	lua_setmetatable(m_luaState, 1);

	//Copies the table to the stack on the top
	lua_pushvalue(m_luaState, 1);

	//Pushes the table to the chunk
	lua_setupvalue(m_luaState, -2, 1);

	//Run the chunk (run the lua file)
	if (lua_pcall(m_luaState, 0, LUA_MULTRET, 0) != 0)
	{
		auto error = lua_tostring(m_luaState, -1);
		Error(error);
	}

	//Pop the table
	lua_pop(m_luaState, 1);
}

//Removes the reference to the table
void LuaW::RemoveReferenceToTable(Table& table)
{
	luaL_unref(m_luaState, LUA_REGISTRYINDEX, table.ref);
	table.ref = -1;
}

//Removes the reference to the function
void LuaW::RemoveReferenceToFunction(Function& function)
{
	luaL_unref(m_luaState, LUA_REGISTRYINDEX, function.ref);
	function.ref = -1;
}

//Removes the reference to the UserData
void LuaW::RemoveReferenceToUserData(UserData& userData)
{
	luaL_unref(m_luaState, LUA_REGISTRYINDEX, userData.ref);
	userData.ref = -1;
}
