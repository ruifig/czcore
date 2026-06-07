
using namespace cz;

namespace
{
	
struct WrongBase
{
};

#define THREADSAFE 1

bool gCaptureStackTraces = true;

struct Base :
#if THREADSAFE
	EnableSharedFromThis<Base>
#else
	EnableLocalSharedFromThis<Base>
#endif
{

	static bool captureSharedPtrStackTraces()
	{
		return gCaptureStackTraces;
	}

	Base()
	{
		alive++;
	}
	virtual ~Base()
	{
		alive--;
	}

	inline static int alive = 0;
	int a = 100;
};

struct Foo : Base
{
	int b = 200;
};

struct Bar : Foo
{
	int c = 300;
};

// This is used to test using different deleters.
struct MyDeleter
{
	inline static int deletedCount = 0;
	void operator()(Base* obj) const
	{
		obj->a = -1;
		obj->~Base();
		deletedCount++;
	}
};

Foo* nulledFoo = nullptr;

} // anonymous namespace


#if THREADSAFE
template<typename T>
using Ptr = cz::SharedPtr<T>;

template<typename T>
using WPtr = cz::WeakPtr<T>;

template<typename T>
using OPtr = cz::ObserverPtr<T>;

template<typename T>
using Ref = cz::SharedRef<T>;
#else
template<typename T>
using Ptr = cz::LocalSharedPtr<T>;

template<typename T>
using WPtr = cz::LocalWeakPtr<T>;

template<typename T>
using OPtr = cz::LocalObserverPtr<T>;

template<typename T>
using Ref = cz::LocalSharedRef<T>;
#endif

//using namespace unittests_details;

template<template<typename> class SmartPtrType, template<typename> class WeakPtrType>
struct SharedPtrTests
{

	/**
	 * Calls std::make_shared or cz::makeShared depending on the smart pointer type being tested
	 */
	template<typename T, typename... Args>
	static SmartPtrType<T> createSharedPtr(Args&& ... args)
	{
		if constexpr(std::is_same_v<std::shared_ptr<T>, SmartPtrType<T>>)
		{
			return std::make_shared<T>(std::forward<Args>(args)...);
		}
		else
		{
			return details::basicMakeShared<T, THREADSAFE>(std::forward<Args>(args)...);
		}
	}

	template<typename T, typename... Args>
	static T* createRaw(Args&& ... args)
	{
		if constexpr(std::is_same_v<std::shared_ptr<T>, SmartPtrType<T>>)
		{
			return new T(std::forward<Args>(args)...);
		}
		else
		{
			return new(details::allocSharedPtrBlock<T, THREADSAFE>()) T(std::forward<Args>(args)...);
		}
	}

	static void basics()
	{
		auto ptr = createSharedPtr<Foo>();
		CHECK(Base::alive == 1);
		ptr = nullptr;
		CHECK(Base::alive == 0);
	}

	static void constructors()
	{

		// Empty constructor
		{
			SmartPtrType<Foo> ptr;
			CHECK(Base::alive == 0);
			CHECK(ptr.get() == nullptr);
		}

		// SmartPtrType<T>(nullptr)
		{
			SmartPtrType<Foo> ptr(nullptr);
			CHECK(Base::alive == 0);
			CHECK(ptr.get() == nullptr);
		}

		// template<typename U>
		// SmartPtrType(U* ptr)
		{
			{
				SmartPtrType<Foo> ptr(nulledFoo);
				CHECK(ptr.get() == nullptr);
			}

			// Same type
			{
				SmartPtrType<Foo> ptr(createRaw<Foo>());
				CHECK(ptr.get());
				CHECK(ptr.use_count()==1);
				CHECK(Base::alive == 1);
				ptr.reset();
				CHECK(ptr.get() == nullptr);
				CHECK(Base::alive == 0);
			}

			// Derived type
			{
				SmartPtrType<Foo> ptr(createRaw<Bar>());
				CHECK(ptr.get());
				CHECK(ptr.use_count()==1);
				CHECK(Base::alive == 1);

				Bar* bar = static_cast<Bar*>(ptr.get());
				CHECK(bar->a == 100);
				CHECK(bar->b == 200);
				CHECK(bar->c == 300);

				ptr.reset();
				CHECK(ptr.get() == nullptr);
				CHECK(Base::alive == 0);
			}
		}

		// SmartPtrType(const SmartPtrType& other)
		{
			SmartPtrType<Foo> ptr1 = createSharedPtr<Foo>();
			SmartPtrType<Foo> ptr2(ptr1);
			CHECK(Base::alive == 1);
			CHECK(ptr1.use_count()==2);
			CHECK(ptr2.use_count()==2);

			ptr1.reset();
			CHECK(Base::alive == 1);
			CHECK(ptr1.use_count()==0);
			CHECK(ptr2.use_count()==1);

			ptr2.reset();
			CHECK(Base::alive == 0);
			CHECK(ptr1.use_count()==0);
			CHECK(ptr2.use_count()==0);
		}

		// template<typename U>
		// SmartPtrType(const SmartPtrType<U>& other)
		{
			SmartPtrType<Foo> ptr1 = createSharedPtr<Foo>();
			SmartPtrType<Base> ptr2(ptr1);
			CHECK(Base::alive == 1);
			CHECK(ptr1.use_count()==2);
			CHECK(ptr2.use_count()==2);

			ptr1.reset();
			CHECK(Base::alive == 1);
			CHECK(ptr1.use_count()==0);
			CHECK(ptr2.use_count()==1);

			ptr2.reset();
			CHECK(Base::alive == 0);
			CHECK(ptr1.use_count()==0);
			CHECK(ptr2.use_count()==0);
		}

		// SmartPtrType(SmartPtrType&& other)
		{
			SmartPtrType<Foo> ptr1 = createSharedPtr<Foo>();
			SmartPtrType<Foo> ptr2(std::move(ptr1));
			CHECK(Base::alive == 1);
			CHECK(ptr1.get() == nullptr);
			CHECK(ptr2.use_count()==1);
			ptr2.reset();
			CHECK(Base::alive == 0);
			CHECK(ptr2.use_count()==0);
		}

		// SmartPtrType(SmartPtrType&& other)
		{
			SmartPtrType<Foo> ptr1 = createSharedPtr<Foo>();
			SmartPtrType<Base> ptr2(std::move(ptr1));
			CHECK(Base::alive == 1);
			CHECK(ptr1.get() == nullptr);
			CHECK(ptr2.use_count()==1);
			ptr2.reset();
			CHECK(Base::alive == 0);
			CHECK(ptr2.use_count()==0);
		}
	}

	static void operators()
	{

		// SmartPtrType& operator=(const SmartPtrType& other)	
		{
			SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
			CHECK(Base::alive == 1);
			SmartPtrType<Foo> foo2 = createSharedPtr<Foo>();
			CHECK(Base::alive == 2);
			foo1 = foo2;
			CHECK(Base::alive == 1);
			CHECK(foo1.use_count()==2);
			CHECK(foo1.get() == foo2.get());
		}

		// template<typename U>
		// SmartPtrType& operator=(const SmartPtrType<U>& other)	
		{
			SmartPtrType<Base> foo1 = createSharedPtr<Base>();
			CHECK(Base::alive == 1);
			SmartPtrType<Foo> foo2 = createSharedPtr<Foo>();
			CHECK(Base::alive == 2);
			foo1 = foo2;
			CHECK(Base::alive == 1);
			CHECK(foo1.use_count()==2);
			CHECK(foo1.get() == foo2.get());
		}

		// SmartPtrType& operator=(SmartPtrType&& other)	
		{
			SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
			CHECK(Base::alive == 1);
			SmartPtrType<Foo> foo2 = createSharedPtr<Foo>();
			CHECK(Base::alive == 2);
			foo1 = std::move(foo2);
			CHECK(Base::alive == 1);
			CHECK(foo1.use_count()==1);
			CHECK(foo2.get() == nullptr);
		}

		// template<typename U>
		// SmartPtrType& operator=(SmartPtrType<U>&& other)	
		{
			SmartPtrType<Base> foo1 = createSharedPtr<Foo>();
			CHECK(Base::alive == 1);
			SmartPtrType<Foo> foo2 = createSharedPtr<Foo>();
			CHECK(Base::alive == 2);
			foo1 = std::move(foo2);
			CHECK(Base::alive == 1);
			CHECK(foo1.use_count()==1);
			CHECK(foo2.get() == nullptr);
		}

		// T* operator->()
		{
			SmartPtrType<Foo> foo = createSharedPtr<Foo>();
			CHECK(foo->a == 100);
			CHECK(foo.get()->a == 100);
			foo = nullptr;
			CHECK(foo.get() == nullptr);
		}

		// T& operator*()
		{
			SmartPtrType<Foo> foo = createSharedPtr<Foo>();
			CHECK((*foo).a == 100);
			foo = nullptr;
		}

		// operator bool()
		{
			SmartPtrType<Foo> foo = createSharedPtr<Foo>();
			CHECK( (bool)(foo) == true );
			foo.reset();
			CHECK( (bool)(foo) == false );
		}
	}

	static void methods()
	{
		// use_count
		{
			SmartPtrType<Foo> foo = createSharedPtr<Foo>();
			constexpr int count = 10;

			std::vector<SmartPtrType<Foo>> v;
			for(int i=0; i<count; i++)
			{
				v.emplace_back(foo);
			}

			CHECK(v.size() == count);
			CHECK(foo.use_count() == (count + 1));

			v.clear();
			CHECK(v.size() == 0);
			CHECK(foo.use_count() == 1);

			foo.reset();
			CHECK(foo.use_count() == 0);
			CHECK(Base::alive == 0);
		}

		// unique() . As of C++20 std::shared doesn't have this any longer, so only test for SharedPtr
		if constexpr(std::is_same_v<Ptr<Foo>, SmartPtrType<Foo>>)
		{
			SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
			CHECK(foo1.unique() == true);

			SmartPtrType<Foo> foo2 = foo1;
			CHECK(foo1.unique() == false);

			foo2.reset();
			CHECK(foo1.unique() == true);

			foo1.reset();
			CHECK(foo1.unique() == false);
		}

		// swap
		{
			SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
			SmartPtrType<Foo> foo2;
			CHECK(foo1.get());
			CHECK(foo2.get() == nullptr);

			foo2.swap(foo1);
			CHECK(foo1.get() == nullptr);
			CHECK(foo2.get());
		}
	}

	static void weak_constructors()
	{
		// Empty 
		{
			WeakPtrType<Foo> w;
			CHECK(w.use_count() == 0);
		}

		// WeakPtrType(const WeakPtr& other)
		{
			WeakPtrType<Foo> w1;
			CHECK(w1.use_count() == 0);
			WeakPtrType<Foo> w2 = w1;
			CHECK(w2.use_count() == 0);

			auto foo1 = createSharedPtr<Foo>();
			w2 = WeakPtrType<Foo>(foo1);
			CHECK(w2.use_count() == 1);

			WeakPtrType<Foo> w3 = w2;
			CHECK(w3.use_count() == 1);
		}

		// template<typename U>
		// WeakPtrType(const WeakPtr<U>& other)
		{
			WeakPtrType<Base> w1;
			CHECK(w1.use_count() == 0);

			auto foo1 = createSharedPtr<Foo>();
			WeakPtrType<Foo> w2 = WeakPtrType<Foo>(foo1);
			CHECK(w2.use_count() == 1);

			WeakPtrType<Base> w3 = w2;
			CHECK(w3.use_count() == 1);

			w2.reset();
			w3.reset();
			// Make sure the weakpointers didn't delete the object
			CHECK(foo1->a == 100);
			CHECK(w2.use_count() == 0);
			CHECK(w3.use_count() == 0);
		}

		// template<typename U>
		// WeakPtrType(const SharedPtrType<U>& other)
		{
			// try with null first
			{
				SmartPtrType<Foo> foo1;
				CHECK(foo1.get() == nullptr);
				WeakPtrType<Foo> wfoo = foo1;
				CHECK(wfoo.use_count() == 0);
			}

			// try with a derived null
			{
				SmartPtrType<Foo> foo1;
				CHECK(foo1.get() == nullptr);
				WeakPtrType<Base> wfoo = foo1;
				CHECK(wfoo.use_count() == 0);
			}

			// try with a non-null
			{
				SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
				CHECK(foo1->a == 100);
				WeakPtrType<Foo> wfoo = foo1;
				CHECK(wfoo.use_count() == 1);
			}

			// try with a non-null derived
			{
				SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
				CHECK(foo1->a == 100);
				WeakPtrType<Base> wfoo = foo1;
				CHECK(wfoo.use_count() == 1);
			}
		}


		// WeakPtrType(WeakPtrType&& other)
		{
			// null
			{
				WeakPtrType<Foo> w1;
				CHECK(w1.use_count() == 0);
				WeakPtrType<Foo> w2 = std::move(w1);
				CHECK(w2.use_count() == 0);
			}

			// null derived
			{
				WeakPtrType<Foo> w1;
				CHECK(w1.use_count() == 0);
				WeakPtrType<Base> w2 = std::move(w1);
				CHECK(w2.use_count() == 0);
			}

			// non-null
			{
				SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
				CHECK(foo1->a == 100);
				WeakPtrType<Foo> w1 = foo1;
				CHECK(w1.use_count() == 1);

				WeakPtrType<Foo> w2 = std::move(w1);
				CHECK(w1.use_count() == 0);
				CHECK(w2.use_count() == 1);
			}

			// non-null derived
			{
				SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
				CHECK(foo1->a == 100);
				WeakPtrType<Foo> w1 = foo1;
				CHECK(w1.use_count() == 1);

				WeakPtrType<Base> w2 = std::move(w1);
				CHECK(w1.use_count() == 0);
				CHECK(w2.use_count() == 1);
			}
		}
	}

	static void weak_operators()
	{
		// WeakPtrType& operator=(const WeakPtrType& other)
		{
			// with null
			{
				WeakPtrType<Foo> w1;
				CHECK(w1.use_count()==0);
				WeakPtrType<Foo> w2;
				CHECK(w2.use_count()==0);
				w2 = w1;
				CHECK(w1.use_count()==0);
				CHECK(w2.use_count()==0);
			}

			// with null derived
			{
				WeakPtrType<Foo> w1;
				CHECK(w1.use_count()==0);
				WeakPtrType<Base> w2;
				CHECK(w2.use_count()==0);
				w2 = w1;
				CHECK(w1.use_count()==0);
				CHECK(w2.use_count()==0);
			}

			// non-null
			{
				SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
				WeakPtrType<Foo> w1 = foo1;
				CHECK(w1.use_count()==1);

				WeakPtrType<Foo> w2;
				CHECK(w2.use_count()==0);

				w2 = w1;
				CHECK(w1.use_count()==1);
				CHECK(w2.use_count()==1);
			}

			// non-null derived
			{
				SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
				WeakPtrType<Foo> w1 = foo1;
				CHECK(w1.use_count()==1);

				WeakPtrType<Base> w2;
				CHECK(w2.use_count()==0);

				w2 = w1;
				CHECK(w1.use_count()==1);
				CHECK(w2.use_count()==1);

			}
		}

		// template<typename U>
		// WeakPtrType& operator=(const SharedPtr<U>& other)
		{
			// null
			{
				SmartPtrType<Foo> foo;
				CHECK(foo.get() == nullptr);
				WeakPtrType<Foo> w;
				CHECK(w.use_count() == 0);

				w = foo;
				CHECK(w.use_count() == 0);
			}

			// null derived
			{
				SmartPtrType<Foo> foo;
				CHECK(foo.get() == nullptr);
				WeakPtrType<Base> w;
				CHECK(w.use_count() == 0);

				w = foo;
				CHECK(w.use_count() == 0);
			}

			// non-null
			{
				SmartPtrType<Foo> foo = createSharedPtr<Foo>();
				CHECK(foo.get());
				WeakPtrType<Foo> w;
				CHECK(w.use_count() == 0);

				w = foo;
				CHECK(w.use_count() == 1);
			}

			// non-null derived
			{
				SmartPtrType<Foo> foo = createSharedPtr<Foo>();
				CHECK(foo.get());
				WeakPtrType<Base> w;
				CHECK(w.use_count() == 0);

				w = foo;
				CHECK(w.use_count() == 1);
			}
		}

		// template<typename U>
		// WeakPtr& operator=(WeakPtr<U>&& other)
		{
			// null
			{
				WeakPtrType<Foo> w1;
				CHECK(w1.use_count()==0);
				WeakPtrType<Foo> w2;
				CHECK(w2.use_count()==0);

				w2 = std::move(w1);
				CHECK(w1.use_count()==0);
				CHECK(w2.use_count()==0);
			}

			// null derived
			{
				WeakPtrType<Foo> w1;
				CHECK(w1.use_count()==0);
				WeakPtrType<Base> w2;
				CHECK(w2.use_count()==0);

				w2 = std::move(w1);
				CHECK(w1.use_count()==0);
				CHECK(w2.use_count()==0);
			}

			// non-null
			{
				SmartPtrType<Foo> foo = createSharedPtr<Foo>();
				CHECK(foo->a == 100);
				WeakPtrType<Foo> w1 = foo;
				CHECK(w1.use_count()==1);
				WeakPtrType<Foo> w2;
				CHECK(w2.use_count()==0);

				w2 = std::move(w1);
				CHECK(w1.use_count()==0);
				CHECK(w2.use_count()==1);
			}

			// non-null derived
			{
				SmartPtrType<Foo> foo = createSharedPtr<Foo>();
				CHECK(foo->a == 100);
				WeakPtrType<Foo> w1 = foo;
				CHECK(w1.use_count()==1);
				WeakPtrType<Base> w2;
				CHECK(w2.use_count()==0);

				w2 = std::move(w1);
				CHECK(w1.use_count()==0);
				CHECK(w2.use_count()==1);
			}

		}

	}

	static void weak_methods()
	{
		
		// reset
		{
			SmartPtrType<Foo> foo = createSharedPtr<Foo>();
			WeakPtrType<Foo> w1 = foo;
			CHECK(w1.use_count() == 1);
			w1.reset();
			CHECK(w1.use_count() == 0);
			CHECK(foo->a==100);
		}

		// swap
		{
			SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
			SmartPtrType<Foo> foo2 = createSharedPtr<Foo>();
			foo1->a = 10;
			foo2->a = 20;
			WeakPtrType<Foo> w1 = foo1;
			WeakPtrType<Foo> w2 = foo2;

			CHECK(w1.lock()->a == 10);
			CHECK(w2.lock()->a == 20);
			w1.swap(w2);
			CHECK(w1.lock()->a == 20);
			CHECK(w2.lock()->a == 10);
		}

		// use_count
		{
			WeakPtrType<Foo> w1;
			CHECK(w1.use_count() == 0);
			SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
			w1 = foo1;
			CHECK(w1.use_count() == 1);
			SmartPtrType<Foo> foo2 = foo1;
			CHECK(w1.use_count() == 2);
			SmartPtrType<Foo> foo3 = foo1;
			CHECK(w1.use_count() == 3);
		}

		// expired & lock
		{
			WeakPtrType<Foo> w1;
			CHECK(w1.expired() == true);
			CHECK(w1.lock() == nullptr);

			SmartPtrType<Foo> foo1 = createSharedPtr<Foo>();
			w1 = foo1;
			CHECK(w1.expired() == false);
			CHECK(w1.lock() == nullptr);
			SmartPtrType<Foo> foo2 = foo1;
			SmartPtrType<Foo> foo3 = foo1;
			CHECK(w1.expired() == false);
			CHECK(w1.lock()->a == 100);

			foo2.reset();
			foo3.reset();
			foo1.reset();
			CHECK(w1.expired() == true);
			CHECK(w1.lock() == nullptr);
		}

	}

	static void all()
	{
		basics();
		constructors();
		operators();
		methods();

		weak_constructors();
		weak_operators();
	}
};

namespace
{
	
	struct Foo2 : public Base
	{
		using SharedPtrDeleter = MyDeleter;
	};
}

TEST_CASE("Custom Deleter", "[SmartPointers]")
{
	auto ff = std::make_shared<Foo>();
	std::weak_ptr<Foo> wff = ff;
	ff = nullptr;


	CHECK(MyDeleter::deletedCount == 0);

	{
		Ptr<Foo2> p1 = cz::details::basicMakeShared<Foo2, THREADSAFE>();
	}

	CHECK(MyDeleter::deletedCount == 1);
}

TEST_CASE("SharedPtr", "[SmartPointers]")
{
	SharedPtrTests<std::shared_ptr, std::weak_ptr>::all();
	SharedPtrTests<Ptr, WPtr>::all();
}

// Test if the memory is being cleared when the object is destroyed but the memory not yet deallocated
#if CZ_SHAREDPTR_CLEAR_MEM
TEST_CASE("Memory clear", "[SmartPointers]")
{
	auto bar = cz::details::basicMakeShared<Bar, THREADSAFE>();
	WPtr<Bar> wbar = bar;
	Bar* ptr = bar.get();

	CHECK(ptr->a == 100);
	CHECK(ptr->b == 200);
	CHECK(ptr->c == 300);

	// Destroy the object, but don't deallocated the memory yet (Because the WeakPtr is still alive)
	bar.reset();

	// The object has been destroyed, but we still have a raw pointer and we test if the memory has been cleared.
	CHECK(ptr->a == 0xDDDDDDDD);
	CHECK(ptr->b == 0xDDDDDDDD);
	CHECK(ptr->c == 0xDDDDDDDD);
}

#endif

TEST_CASE("SharedRef", "[SmartPointers]")
{
	SECTION("makeSharedRef")
	{
		Ref<Foo> ref = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
		CHECK(ref.use_count() == 1);
	}

	SECTION("Constructing from SharedPtr")
	{
		{
			Ptr<Foo> ptr = cz::details::basicMakeShared<Foo, THREADSAFE>();
			Ref<Foo> ref(ptr);
			CHECK(ptr.use_count() == 2);
		}

		{
			Ptr<Foo> ptr = cz::details::basicMakeShared<Foo, THREADSAFE>();
			Ref<Foo> ref(std::move(ptr));
			CHECK(ptr.get() == nullptr);
			CHECK(ref.use_count() == 1);
		}

		// Try converting between base and derived
		{
			Ptr<Foo> ptr = cz::details::basicMakeShared<Foo, THREADSAFE>();
			Ref<Base> ref(ptr);
			CHECK(ptr.use_count() == 2);
		}

		{
			Ptr<Foo> ptr = cz::details::basicMakeShared<Foo, THREADSAFE>();
			Ref<Base> ref(std::move(ptr));
			CHECK(ptr.get() == nullptr);
			CHECK(ref.use_count() == 1);
		}
	}

	SECTION("Constructing from SharedRef")
	{
		{
			Ref<Foo> ref1 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			Ref<Foo> ref2(ref1);
			CHECK(ref2.use_count() == 2);
		}

		{
			Ref<Foo> ref1 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			Ref<Foo> ref2(std::move(ref1));
			// Moving a SharedRef doesn't null the source
			CHECK(ref1.toSharedPtr().get() != nullptr);
			CHECK(ref2.toSharedPtr().get() != nullptr);
		}

		// Try converting between base and derived
		{
			Ref<Foo> ref1 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			Ref<Base> ref2(ref1);
			CHECK(ref2.use_count() == 2);
		}

		{
			Ref<Foo> ref1 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			Ref<Base> ref2(std::move(ref1));
			// Moving a SharedRef doesn't null the source
			CHECK(ref1.toSharedPtr().get() != nullptr);
			CHECK(ref2.toSharedPtr().get() != nullptr);
		}
	}


	// The API doesn't allow automatic conversion from SharedPtr to Ref. This is intentional
	#if 0
	SECTION("Assignment operator from SharedPtr")
	{

		//
		// const 
		//
		{
			Ptr<Foo> ptr1 = makeShared<Foo>();
			Ptr<Foo> ptr2 = makeShared<Foo>();

			Ref<Foo> ref(ptr2);
			CHECK(ptr1.use_count() == 1);
			CHECK(ptr2.use_count() == 2);
			ref = ptr1;
			CHECK(ptr1.use_count() == 2);
			CHECK(ptr2.use_count() == 1);
		}
		// Try converting between base and derived
		{
			Ptr<Foo> ptr1 = makeShared<Foo>();
			Ptr<Base> ptr2 = makeShared<Foo>();

			Ref<Base> ref(ptr2);
			CHECK(ptr1.use_count() == 1);
			CHECK(ptr2.use_count() == 2);
			ref = ptr1;
			CHECK(ptr1.use_count() == 2);
			CHECK(ptr2.use_count() == 1);
		}

		//
		// rvalue
		//
		{
			Ptr<Foo> ptr1 = makeShared<Foo>();

			Ref<Foo> ref(makeShared<Foo>());
			WPtr<Foo> weak1 = ref.toSharedPtr();

			CHECK(ref.use_count() == 1);
			ref = std::move(ptr1);
			CHECK(weak1.expired() == true);
			CHECK(ptr1 == nullptr);
			CHECK(ref.use_count() == 1);
		}
		// Try converting between base and derived
		{
			Ptr<Foo> ptr1 = makeShared<Foo>();

			Ref<Base> ref(makeShared<Foo>());
			WPtr<Base> weak1 = ref.toSharedPtr();

			CHECK(ref.use_count() == 1);
			ref = std::move(ptr1);
			CHECK(weak1.expired() == true);
			CHECK(ptr1 == nullptr);
			CHECK(ref.use_count() == 1);
		}
	}
	#endif

	SECTION("Assignment operator from SharedRef")
	{
		{
			Ref<Foo> ref1 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			Ref<Foo> ref2 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();

			WPtr<Foo> weak1 = ref1.toSharedPtr();
			ref1 = ref2;
			CHECK(weak1.expired() == true);
			CHECK(ref1.use_count() == 2);
			CHECK(ref1.toSharedPtr().get() == ref2.toSharedPtr().get());
		}
		// Try converting between base and derived
		{
			Ref<Base> ref1 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			Ref<Foo> ref2  = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			WPtr<Base> weak1 = ref1.toSharedPtr();
			ref1 = ref2;
			CHECK(weak1.expired() == true);
			CHECK(ref1.use_count() == 2);
			CHECK(ref1.toSharedPtr().get() == ref2.toSharedPtr().get());
		}

		//
		// rvalue
		//
		{
			Ref<Foo> ref1 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			Ref<Foo> ref2 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			WPtr<Foo> weak1 = ref1.toSharedPtr();
			ref1 = std::move(ref2);
			CHECK(weak1.expired() == true);
			CHECK(ref1.use_count() == 2);
			CHECK(ref1.toSharedPtr().get() == ref2.toSharedPtr().get());
		}
		// Try converting between base and derived
		{
			Ref<Base> ref1 = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			Ref<Foo> ref2  = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			WPtr<Base> weak1 = ref1.toSharedPtr();
			ref1 = std::move(ref2);
			CHECK(weak1.expired() == true);
			CHECK(ref1.use_count() == 2);
			CHECK(ref1.toSharedPtr().get() == ref2.toSharedPtr().get());
		}

	}

	SECTION("Conversions")
	{
		// Implicit conversion to SharedPtr
		{
			Ref<Foo> ref = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			CHECK(ref.use_count() == 1);
			Ptr<Foo> ptr = ref;
			CHECK(ref.use_count() == 2);
			CHECK(ptr.use_count() == 2);
		}
		{
			Ref<Foo> ref = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			CHECK(ref.use_count() == 1);
			Ptr<Foo> ptr = ref.toSharedPtr();
			CHECK(ref.use_count() == 2);
			CHECK(ptr.use_count() == 2);
		}

		// Ptr to SharedRef conversion needs to be explicit.
		{
			Ptr<Foo> ptr = cz::details::basicMakeShared<Foo, THREADSAFE>();
			CHECK(ptr.use_count() == 1);
			Ref<Foo> ref = cz::details::basicMakeSharedRef<Foo, THREADSAFE>();
			ref = ptr.toSharedRef();
			CHECK(ptr.use_count() == 2);
			CHECK(ref.use_count() == 2);
		}
	}

}

TEST_CASE("ObserverPtr", "[SmartPointers]")
{
	Ptr<Foo> foo = cz::details::basicMakeShared<Foo, THREADSAFE>();
	WPtr<Foo> w1 = foo;
	OPtr<Foo> o1(w1);

	CHECK(w1.use_count() == 1);
	CHECK(w1.lock().get() != nullptr);

	#if THREADSAFE == 0
		CHECK(o1.tryGet() != nullptr);
	#endif
}

TEST_CASE("EnableSharedFromThis", "[SmartPointers]")
{
	CHECK(Base::alive == 0);
	Ptr<Foo> foo = cz::details::basicMakeShared<Foo, THREADSAFE>();
	WPtr<Foo> wfoo = foo;
	WPtr<Foo> wfoo2;
	CHECK(Base::alive == 1);
	CHECK(foo.use_count() == 1);
	CHECK(foo.weak_use_count() == 2);

	Base* raw = static_cast<Base*>(foo.get());

	Ptr<Foo> foo2 = sharedRefFrom(static_cast<Foo*>(raw));
	CHECK(wfoo.use_count() == 2);
	CHECK(wfoo.weak_use_count() == 2);

	foo = nullptr;
	CHECK(wfoo.use_count() == 1);
	CHECK(wfoo.weak_use_count() == 2);
	CHECK(Base::alive == 1);

	foo2 = nullptr;
	CHECK(wfoo.weak_use_count() == 1);
	CHECK(Base::alive == 0);
	wfoo.reset();

}

TEST_CASE("releaseIfOne", "[SmartPointers]")
{
	Ptr<Foo> foo = cz::details::basicMakeShared<Foo, THREADSAFE>();

	foo.resetIfLast();



}
