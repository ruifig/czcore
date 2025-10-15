using namespace cz;

#if defined(_MSVC_LANG)
		__pragma(warning(push))
		__pragma(warning(disable: 5204))  /* 'type-name': class has virtual functions, but its trivial destructor is not virtual; instances of objects derived from this class may not be destructed correctly */
		__pragma(warning(disable: 4868))  /* compiler may not enforce left-to-right evaluation order in braced initializer list */
		__pragma(warning(disable: 5264)) /* 'variable-name': 'const' variable is not used */
		__pragma(warning(disable: 5267)) /* definition of implicit copy constructor/assignment operator for 'type' is deprecated because it has a user-provided assignment operator/copy constructor  */
		__pragma(warning(disable: 4242)) /*	'identifier': conversion from 'type1' to 'type2', possible loss of data */
		__pragma(warning(disable: 4244)) /* 'operator': conversion from 'type1' to 'type2', possible loss of data */
		__pragma(warning(disable: 4267)) /* 'var' : conversion from 'size_t' to 'type', possible loss of data */

#endif

namespace 
{
	enum class ObjType : uint8_t
	{
		A = 10,
		B,
		C,
		D,
		E
	};

	struct alignas(alignof(void*)) A
	{
		ObjType type;
		int8_t a1;

		A() = default;
		~A() = default;

		virtual ObjType val() const
		{
			return ObjType::A;
		}

		virtual void clear()
		{
		}

		A(int32_t a1)
			: type(ObjType::A)
			, a1(static_cast<int8_t>(a1))
		{
		}

		A(ObjType type, int8_t a1)
			: type(type)
			, a1(a1)
		{
		}

		friend auto operator <=>(const A& lhs, const A& rhs) = default;
	};

	struct B : public A
	{
		B() = default;

		B(int8_t a1, int8_t b1)
			: A(ObjType::B, a1)
			, b1(static_cast<uint8_t>(b1))
		{
		}

		virtual ObjType val() const override
		{
			return ObjType::B;
		}

		uint8_t b1;
	};

	struct C : public A
	{
		C() = default;

		C(int8_t a1, int8_t c1)
			: A(ObjType::C, a1)
			, c1(c1)
		{
		}

		virtual ObjType val() const override
		{
			return ObjType::C;
		}

		int64_t c1;
	};

	struct D : public A
	{
		D() = default;

		D(int8_t a1, int8_t d1)
			: A(ObjType::D, a1)
			, d1(d1)
		{
		}

		char* str()
		{
			return reinterpret_cast<char*>((this+1));
		}

		virtual ObjType val() const override
		{
			return ObjType::D;
		}

		int8_t d1; 
	};

	struct E : public A
	{
		E() = default;

		E(int8_t a1, int* counter)
			: A(ObjType::E, a1)
			, counter(counter)
		{
		}

		virtual ObjType val() const override
		{
			return ObjType::E;
		}

		virtual void clear() override
		{
			(*counter)--;
		}

		int* counter = nullptr;
	};


	constexpr int aa1 = alignof(A);
	constexpr int aa2 = alignof(B);
	constexpr int aa3 = alignof(C);

	constexpr int ss1 = sizeof(A);
	constexpr int ss2 = sizeof(B);
	constexpr int ss3 = sizeof(C);

	using VecType = VSOVector<A>;
}

struct CheckBase
{
	virtual void check(const A& base) = 0;
	virtual ~CheckBase() = default;
};

template<typename T>
struct Check : CheckBase
{
	T val;
	VecType::Ref ref;
	VecType& owner;
	std::string str;

	Check(VecType& owner)
		: owner(owner)
	{
	}

	virtual void check(const A& base) override
	{
		auto inobj = static_cast<const T*>(&base);
		auto obj = static_cast<T*>(&owner.at(ref));
		CHECK( val == *obj);
		CHECK( val == *inobj);
		CHECK(base.val() == base.type);

		if constexpr(std::is_same_v<T, D>)
		{
			CHECK(str == obj->str());
		}
	}
};

struct VSOVTestHarness
{
	VSOVTestHarness(VecType::SizeType capacity)
		: vec(capacity)
	{
	}

	VecType vec;
	std::vector<std::unique_ptr<CheckBase>> all;

	template<typename T, typename... Args>
	VecType::Ref push_back(Args&& ... args)
	{
		T obj(std::forward<Args>(args)...);
		auto check = std::make_unique<Check<T>>(vec);
		check->val = obj;
		VecType::Ref ref = vec.push_back(std::move(obj));
		check->ref = ref;
		all.push_back(std::move(check));

		checkAll();
		return ref;
	}

	template<typename T, typename... Args>
	VecType::Ref push_back_extra(const char* str, Args&& ... args)
	{
		T obj(std::forward<Args>(args)...);
		auto check = std::make_unique<Check<T>>(vec);
		check->val = obj;
		check->str = str;
		VecType::Ref ref = vec.push_back(std::move(obj), strlen(str)+1);

		// Copy the string over
		memcpy(vec.atAs<T>(ref).str(), str, strlen(str)+1);

		check->ref = ref;
		all.push_back(std::move(check));
		checkAll();
		return ref;
	}

	void checkAll()
	{

		{
			size_t index = 0;
			// Try with iterator
			for(auto it = vec.begin(); it != vec.end(); ++it)
			{
				all[index]->check(*it);

				// Test iteratorToRef
				all[index]->check(vec.at(vec.iteratorToRef(it)));
				index++;
			}
			// Make sure we iterated to the end
			CHECK(index == all.size());
		}

		{
			size_t index = 0;
			// Try with range loop
			for(A& o : vec)
			{
				all[index]->check(o);
				index++;
			}
			// Make sure we iterated to the end
			CHECK(index == all.size());
		}

		{
			size_t index = 0;
			VecType::Ref ref = vec.beginRef();
			// Try with incrementing ref
			while(ref != vec.endRef())
			{
				all[index]->check(vec.at(ref));

				// Test refToIterator
				all[index]->check(*vec.refToIterator(ref));

				ref = vec.next(ref);
				index++;
			}
			// Make sure we iterated to the end
			CHECK(index == all.size());
		}
	}
};

TEST_CASE("InvalidReference","[VSOVector]")
{
	VSOVTestHarness harness(2);
	VecType::Ref ref;
	CHECK(ref.isSet() == false);
	ref = harness.vec.beginRef();
	CHECK(ref.isSet() == true);
}

TEST_CASE("Adding objects of different sizes","[VSOVector]")
{
	VSOVTestHarness harness(2);

	CHECK(harness.vec.getFreeCapacity() == harness.vec.getCapacity());
	CHECK(harness.vec.getUsedCapacity() == 0);

	//auto previousUsedCapacity = harness.vec.getUsedCapacity();
	VecType::Ref ref = harness.push_back<A>(1);
	CHECK(harness.vec.getUsedCapacity() == (harness.vec.getHeaderSize() + sizeof(A)));

	ref = harness.push_back<A>(2);

	ref = harness.push_back<B>(3,4);
	ref = harness.push_back<B>(4,5);
	ref = harness.push_back<C>(5,7);
	ref = harness.push_back<C>(6,8);
}

TEST_CASE("Iterators","[VSOVector]")
{
	VSOVTestHarness harness(2);
	VecType::Ref ref;

	ref = harness.push_back<A>(1);
	ref = harness.push_back<A>(2);
	ref = harness.push_back<B>(3,4);
	ref = harness.push_back<B>(4,5);
	ref = harness.push_back<C>(5,7);
	ref = harness.push_back<C>(6,8);
}

TEST_CASE("ExtraBytes","[VSOVector]")
{
	VSOVTestHarness harness(2);
	VecType::Ref ref;

	ref = harness.push_back<A>(1);
	ref = harness.push_back<A>(2);
	ref = harness.push_back<B>(3,4);
	ref = harness.push_back<B>(4,5);
	ref = harness.push_back<C>(6,7);
	ref = harness.push_back<C>(7,8);

	// 6 character + null = 7, to test if it adds extra bytes on top of the string length, because of the objects alignment
	auto previousUsedCapacity = harness.vec.getUsedCapacity();
	ref = harness.push_back_extra<D>("Hello!", 8, 9);
	// +8 because the string only needs 7, but it will align according to the objects
	CHECK((harness.vec.getUsedCapacity() - previousUsedCapacity) == harness.vec.getHeaderSize() + sizeof(D) + 8);

	// 7 character + null = 8, to test if if correctly used that size (it's aligned already)
	previousUsedCapacity = harness.vec.getUsedCapacity();
	ref = harness.push_back_extra<D>("HellOO!", 9, 10);
	CHECK((harness.vec.getUsedCapacity() - previousUsedCapacity) == harness.vec.getHeaderSize() + sizeof(D) + 8);

	// 8 character + null = 9, to test if it aligned extra bytes to the object alignment
	previousUsedCapacity = harness.vec.getUsedCapacity();
	ref = harness.push_back_extra<D>("HellOOO!", 10, 11);
	CHECK((harness.vec.getUsedCapacity() - previousUsedCapacity) == harness.vec.getHeaderSize() + sizeof(D) + 16);
}


TEST_CASE("Find", "[VSOVector]")
{
	VSOVTestHarness harness(2);
	VecType::Ref ref;

	auto r0 = harness.push_back<A>(0);
	auto r1 = harness.push_back<B>(1,22);
	auto r2 = harness.push_back<C>(2,33);
	auto r3 = harness.push_back<C>(3,33);
	auto r4 = harness.push_back_extra<D>("Hello", 4,44);

	auto advance = [](VecType::Iterator it, int n) -> VecType::Iterator
	{
		while(n--)
		{
			++it;
		}
		return it;
	};

	for(int i=0; i<= static_cast<int>(harness.vec.getNumElements()); i++)
	{
		VecType::Iterator it = std::find_if(harness.vec.begin(), harness.vec.end(), [i](const A& v)
		{
			return v.a1 == i;
		});

		// Check it returned the expected iterator
		CHECK(it == advance(harness.vec.begin(), i));
		if (i == static_cast<int>(harness.vec.getNumElements()))
		{
			CHECK(it == harness.vec.end());
		}
	}

	auto last = advance(harness.vec.begin(), 2);
	VecType::Iterator it = std::find_if(harness.vec.begin(), last, [](const A& v)
	{
		return v.a1 == 4;
	});
	// If not found, it should return last
	CHECK(it == last);

}

TEST_CASE("Clear", "[VSOVector]")
{
	VSOVTestHarness harness(2);
	VecType::Ref ref;

	//auto r0 = harness.push_back<A>(0);
	//auto r1 = harness.push_back<B>(1,22);
	//auto r2 = harness.push_back<C>(2,33);
	//auto r3 = harness.push_back<C>(3,33);
	//auto r4 = harness.push_back_extra<D>("Hello", 4,44);

	int counter = 100;
	auto r0 = harness.push_back<A>(0);
	auto r1 = harness.push_back<E>(5, &counter);

	harness.vec.clear([](A& obj)
	{
		obj.clear();
	});

	CHECK(counter == 99);

	CHECK(harness.vec.getNumElements() == 0);
	CHECK(harness.vec.getUsedCapacity() == 0);
}

TEST_CASE("OOB data", "[VSOVector]")
{
	struct Vec3
	{
		float a;
		float b;
		float c;
	};

	struct Foo
	{
		int a;
		uint32_t ref;
	};

	VSOVector<Foo> vsov;
	Vec3 v[4] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {9, 10, 11}};

	// Write oob data at the beginning and in between objects
	vsov.emplace_back<Foo>(100, vsov.oob_push_back(&v[0], 2).pos);
	vsov.emplace_back<Foo>(101, vsov.oob_push_back(&v[2], 2).pos);

	// Read back the objects and oob data
	int i=0;
	for(Foo& f : vsov)
	{
		Vec3* data = &vsov.oobAtAs<Vec3>(VSOVector<Foo>::Ref(f.ref));
		CHECK(f.a == (100 + i));
		Vec3* base = &v[i * 2];

		CHECK(data[0].a == base[0].a);
		CHECK(data[0].b == base[0].b);
		CHECK(data[0].c == base[0].c);
		CHECK(data[1].a == base[1].a);
		CHECK(data[1].b == base[1].b);
		CHECK(data[1].c == base[1].c);
		i++;
	}

}


#if defined(_MSVC_LANG)
		__pragma(warning(pop))
#endif

