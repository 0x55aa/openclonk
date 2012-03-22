/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 1998-2000  Matthes Bender
 * Copyright (c) 2002, 2005  Sven Eberhardt
 * Copyright (c) 2002, 2004-2005, 2007, 2009  Peter Wortmann
 * Copyright (c) 2005, 2007  Günther Brammer
 * Copyright (c) 2010  Nicolas Hake
 * Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de
 *
 * Portions might be copyrighted by other authors who have contributed
 * to OpenClonk.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * See isc_license.txt for full license and disclaimer.
 *
 * "Clonk" is a registered trademark of Matthes Bender.
 * See clonk_trademark_license.txt for full license.
 */

/* Fixed point math extracted from ALLEGRO by Shawn Hargreaves */

/* The Clonk engine uses fixed point math for exact object positions.
   This is rather silly. Nowadays we should simply use floats. However,
   I never dared changing the whole thing. */
/* 01-17-2002: I think the whole, ugly fixed-thing has to be revived,
   because floating point calculations are not guaranteed to be network
   safe...however, it can be solved as a data type with operator
   overloading, automatic type conversions, etc now   - Sven2 */
/* After some time with synchronous float use, C4Real is used again to
   work around the problem that different compilers produce different
   floating point code, leading to desyncs between linux and windows
   engines. */

#ifndef INC_C4Real
#define INC_C4Real

#include <xmmintrin.h>

class C4Real
{
	__m128 value;

	friend C4Real Sin(const C4Real &);
	friend C4Real Cos(const C4Real &);
	friend C4Real Pow(const C4Real &, const C4Real &);

public:
	inline C4Real(float val = 0.0f) : value(_mm_set_ss(val)) { }
	inline C4Real(int32_t val, int32_t prec)
	{
		value = _mm_div_ss(_mm_cvtsi32_ss(value, val), _mm_cvtsi32_ss(value, prec));
	}

	// Copy ctor and assignment
	inline C4Real(const C4Real &rhs) : value(rhs.value) {}
	inline C4Real &operator = (const C4Real &rhs) { value = rhs.value; return *this; }

	// Arithmetics
	// We're using _ps intrinsics for everything except division because they
	// have the same latency as their _ss counterparts, but their representa-
	// tion is one byte shorter (0F xx instead of F3 0F xx).
	// DIVPS is about half as fast as DIVSS, so we use the scalar instruction
	// here.
	inline C4Real &operator += (const C4Real &rhs) { value = _mm_add_ps(value, rhs.value); return *this; }
	inline C4Real &operator -= (const C4Real &rhs) { value = _mm_sub_ps(value, rhs.value); return *this; }
	inline C4Real &operator *= (const C4Real &rhs) { value = _mm_mul_ps(value, rhs.value); return *this; }
	inline C4Real &operator /= (const C4Real &rhs) { value = _mm_div_ss(value, rhs.value); return *this; }

#define C4REAL_ARITHMETIC_OPERATOR(op) \
	/* arithmetic operations on copies */ \
	inline C4Real operator op (const C4Real &rhs) const { C4Real nrv(*this); nrv op##= rhs; return nrv; } \
	inline C4Real operator op (float rhs) const { C4Real nrv(*this); nrv op##= rhs; return nrv; } \
	inline C4Real operator op (int rhs) const { C4Real nrv(*this); nrv op##= rhs; return nrv; } \
	/* arithmetic operations on copies, right-hand C4Real */ \
	/* friends defined in the class are implicitly inline */ \
	friend C4Real operator op (float lhs, const C4Real &rhs) { C4Real nrv(lhs); nrv op##= rhs; return nrv; } \
	friend C4Real operator op (int lhs, const C4Real &rhs) { C4Real nrv(lhs); nrv op##= rhs; return nrv; }

	C4REAL_ARITHMETIC_OPERATOR(+)
	C4REAL_ARITHMETIC_OPERATOR(-)
	C4REAL_ARITHMETIC_OPERATOR(*)
	C4REAL_ARITHMETIC_OPERATOR(/)
#undef C4REAL_ARITHMETIC_OPERATOR
	
	inline C4Real operator + () const { return *this; }
	inline C4Real operator - () const
{
		C4Real nrv;
		nrv -= *this;
		return nrv;
	}

#define C4REAL_COMPARISON_OPERATOR(op, intrinsic) \
	friend bool operator op (const C4Real &lhs, const C4Real &rhs) { return _mm_##intrinsic##_ss(lhs.value, rhs.value) != 0; } \
	friend bool operator op (const C4Real &lhs, float rhs) { return lhs op C4Real(rhs); } \
	friend bool operator op (const C4Real &lhs, int rhs) { return lhs op C4Real(rhs); } \
	friend bool operator op (float lhs, const C4Real &rhs) { return C4Real(lhs) op rhs; } \
	friend bool operator op (int lhs, const C4Real &rhs) { return C4Real(lhs) op rhs; }
	C4REAL_COMPARISON_OPERATOR(<, comilt)
	C4REAL_COMPARISON_OPERATOR(<=, comile)
	C4REAL_COMPARISON_OPERATOR(==, comieq)
	C4REAL_COMPARISON_OPERATOR(>=, comige)
	C4REAL_COMPARISON_OPERATOR(>, comigt)
	C4REAL_COMPARISON_OPERATOR(!=, comineq)
#undef C4REAL_COMPARISON_OPERATOR

	// Conversion
	inline operator int() const { return _mm_cvtss_si32(value); }
	inline operator float() const { float nrv; _mm_store_ss(&nrv, value); return nrv; }

	// Boolean operators
	// Not using safe-bool-idiom here, because we already define conversions
	// to integer, which is why we don't need to worry about unwanted con-
	// versions via operator bool
	inline operator bool() const { return _mm_comineq_ss(value, _mm_setzero_ps()) != 0; }
	inline bool operator !() const { return !operator bool(); }

	// C++03 doesn't support explicitly defaulted ctors, so C4Real can't
	// be a POD, so we can't directly store it into unions.
	struct StorageType
	{
		int32_t v;
	};
#ifdef HAVE_WORKING_IS_POD
	static_assert(boost::is_pod<StorageType>::value, "C4Real: StorageType is not a POD type");
#endif

	friend bool operator==(StorageType lhs, StorageType rhs) { return lhs.v == rhs.v; }

	C4Real(StorageType rhs) : value(_mm_load_ss(reinterpret_cast<float*>(&rhs.v))) {}
	operator StorageType() const { StorageType nrv; _mm_store_ss(reinterpret_cast<float*>(&nrv.v), value); return nrv; }
};

// conversion
inline float fixtof(const C4Real &x) { return static_cast<float>(x); }
inline C4Real ftofix(float x) { return C4Real(x); }
inline int fixtoi(const C4Real &x) { return static_cast<int>(x); }
inline int fixtoi(const C4Real &x, int32_t prec) { return static_cast<int>(x * prec); }
inline C4Real itofix(int32_t x) { return C4Real(x); }
inline C4Real itofix(int32_t x, int32_t prec) { return C4Real(x, prec); }

// additional functions
inline C4Real C4REAL100(int x) { C4Real nrv(x); nrv /= 100; return nrv; }
inline C4Real C4REAL256(int x) { C4Real nrv(x); nrv /= 256; return nrv; }
inline C4Real C4REAL10(int x) { C4Real nrv(x); nrv /= 10; return nrv; }

// define 0
const C4Real Fix0 = C4Real(0);

// CompileFunc for C4Real
void CompileFunc(C4Real &rValue, StdCompiler *pComp);

#endif //C4REAL_H_INC
