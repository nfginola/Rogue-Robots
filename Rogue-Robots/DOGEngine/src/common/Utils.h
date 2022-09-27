#pragma once

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

class HR
{
	HRESULT hr;
public:
	/* implicit constructor */
	constexpr HR(HRESULT hr = 0) noexcept : hr(hr) {}

	void try_fail(const std::string_view message)
	{
		if (this->failed())
		{
			std::cerr << message << "\n" << "HRESULT " << std::hex << hr << ": ";
			this->print();
			assert(false);
		}
	};

	constexpr bool succeeded()
	{
		return SUCCEEDED(hr);
	}

	constexpr bool failed()
	{
		return FAILED(hr);
	}

	void print()
	{
		char out[64];
		FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
			out, 64, nullptr);

		std::cout << out;
	}
};

#ifndef DELETE_COPY_CONSTRUCTOR
#define DELETE_COPY_CONSTRUCTOR(T)	\
			T(const T&) = delete;	\
			T& operator=(const T&) = delete;
#endif
#ifndef DELETE_MOVE_CONSTRUCTOR
#define DELETE_MOVE_CONSTRUCTOR(T)	\
			T(T&&) = delete;	\
			T& operator=(const T&&) = delete;
#endif
#ifndef DELETE_COPY_MOVE_CONSTRUCTOR
	#define DELETE_COPY_MOVE_CONSTRUCTOR(T)	\
			DELETE_COPY_CONSTRUCTOR(T);	\
			DELETE_MOVE_CONSTRUCTOR(T);
#endif
#ifndef DELETE_DEFAULT_CONSTRUCTOR
#define DELETE_DEFAULT_CONSTRUCTOR(T)	\
			T() = delete;	\
			~T() = delete;
#endif
#ifndef STATIC_CLASS
#define STATIC_CLASS(T)	\
			DELETE_DEFAULT_CONSTRUCTOR(T)	\
			DELETE_COPY_CONSTRUCTOR(T)	\
			DELETE_MOVE_CONSTRUCTOR(T)
#endif

#if defined NDEBUG
	#ifndef ASSERT_FUNC
		#define ASSERT_FUNC(expression, errorString) (expression)
	#endif
#else
	#ifndef ASSERT_FUNC
		#define ASSERT_FUNC(expression, errorString) assert(expression && errorString)
	#endif
#endif

#if defined NDEBUG
	#ifndef ASSERT
		#define ASSERT(expression, errorString) 
	#endif
#else
	#ifndef ASSERT
		#define ASSERT(expression, errorString) assert(expression && errorString)
	#endif
#endif

struct Vector2u
{
	u32 x;
	u32 y;
};

struct Vector2i
{
	i32 x;
	i32 y;
};

struct Vector3f
{
	f32 x;
	f32 y;
	f32 z;
};

inline uint64_t GenerateRandomID()
{
	static std::random_device rdev;
	static std::mt19937 gen(rdev());
	static std::uniform_int_distribution<uint64_t> udis(1, std::numeric_limits<uint64_t>::max());
	return udis(gen);
}

// Is this a memory leak???
template <typename T, typename ...Args>
void CallAsync(T&& function, Args&& ...args)
{
	std::shared_ptr<std::future<void>> sharedFuture = std::make_shared<std::future<void>>();
	*sharedFuture = std::async(
		std::launch::async,
		[sharedFuture, function]<typename ...Args>(Args&& ...args) mutable { function(std::forward<Args>(args)...); sharedFuture.reset(); },
		std::forward<Args>(args)...);
}