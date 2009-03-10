//
//  Nstate.hpp - Template for variable that can only take on values 
//     from [0..radix-1], and an array type which is able to
//     store a series of these variables and take advantage of 
//     the knowledge of the constraint in order to compact them
//     closer to their fundamentally minimal size.
//
//  	 	Copyright (c) 2009 HostileFork.com
// Distributed under the Boost Software License, Version 1.0. 
//    (See accompanying file LICENSE_1_0.txt or copy at
//           http://www.boost.org/LICENSE_1_0.txt)
//
// See http://hostilefork.com/nstate for documentation.
//

#ifndef __NSTATE_HPP__
#define __NSTATE_HPP__

#include "NocycleSettings.hpp"

#include <vector>
#include <map>
#include <exception>
#include <math.h>

//
// NSTATE
//

namespace nstate {

// see: http://www.cplusplus.com/doc/tutorial/exceptions.html
class bad_nstate : public std::exception {
	virtual const char* what() const throw() {
		return "Invalid Nstate value";
	}
};


template<int radix> 
class Nstate {
private:
	unsigned m_value; // ranges from [0..radix-1]
	
private:
	void ThrowExceptionIfBadValue() {
		if (m_value >= radix) {
			bad_nstate bt;
			throw bt;
		}
	} 
public:
	// constructor is also implicit cast operator from unsigned
	// see: http://www.acm.org/crossroads/xrds3-1/ovp3-1.html
	Nstate(unsigned value) : m_value (value) {
		ThrowExceptionIfBadValue();
	}
	// default constructor setting value to zero... good idea?  Bad idea?
	Nstate() : m_value (0) {
		ThrowExceptionIfBadValue();
	}
	// implicit cast operator to unsigned
	operator unsigned() const {
		return m_value;
	}
// "A base class destructor should be either public and virtual, 
//  or protected and nonvirtual."
// http://www.gotw.ca/publications/mill18.htm
public:
	virtual ~Nstate() { }

public:
	static bool SelfTest(); // Class is self-testing for regression
};

}


//
// NSTATE ARRAY
//

// Due to complications with static members of templates, I am using a single 
// external variable of a map for the power tables that each template
// instantiation needs.  This works on more compilers.  However, because 
// this lookup is slow, I've optimized it for Tristates until a better solution
// can be found.
extern std::map<unsigned, std::vector<unsigned> > nstatePowerTables;

// TODO:
//   * iterators?
//   * 64-bit packing or something more clever?
//   * memory-mapped file implementation, with standardized packing/order
//     (to work across platforms)?
template <int radix>
class NstateArray {
private:
	typedef unsigned PackedType;
	
private:
	// Note: Typical library limits of the STL for vector lengths 
	// are things like 1,073,741,823...
	std::vector<PackedType> m_buffer;
	size_t m_max;
	
private:
	PackedType* m_cachedPowerTable; // array allocated for fast access, first element is length! []
	
private:
	static void EnsurePowerTableBuiltForRadix() {
		if (nstatePowerTables.count(radix) == 0) {
			unsigned nstatesInUnsigned = static_cast<unsigned>(floor(log(2)/log(radix)*CHAR_BIT*sizeof(unsigned)));
			std::vector<unsigned> powerTable;
			PackedType value = 1;
			for (unsigned index = 0; index < nstatesInUnsigned; index++) {
				powerTable.push_back(value);
				value = value * radix;
			}
			nstatePowerTables[radix] = powerTable;
			nocycle_assert(nstatePowerTables.count(radix) != 0);
		}
	}
	inline size_t NstatesInPackedType() const {
		return m_cachedPowerTable[0];
	}
	inline PackedType PowerForDigit(unsigned digit) const {
		return m_cachedPowerTable[digit+1];
	}
	nstate::Nstate<radix> GetDigitInPackedValue(PackedType packed, unsigned digit) const;
	PackedType SetDigitInPackedValue(PackedType packed, unsigned digit, nstate::Nstate<radix> t) const;
	
public:
	// Derived from boost's dynamic_bitset
	// http://www.boost.org/doc/libs/1_36_0/libs/dynamic_bitset/dynamic_bitset.html 
	class reference;
	friend class NstateArray<radix>::reference;
    class reference
    {
		friend class NstateArray<radix>;
		
	private:
		NstateArray<radix>& m_na;
		unsigned m_indexIntoBuffer;
		unsigned m_digit;
		reference(NstateArray<radix> &na, size_t indexIntoBuffer, unsigned digit) :
			m_na (na),
			m_indexIntoBuffer (indexIntoBuffer),
			m_digit (digit)
		{
		}
	
		void operator&(); // not defined
		void do_assign(nstate::Nstate<radix> x) {
			m_na.m_buffer[m_indexIntoBuffer] = 
				m_na.SetDigitInPackedValue(m_na.m_buffer[m_indexIntoBuffer], m_digit, x);
		}
    public:
        // An automatically generated copy constructor.
		reference& operator=(nstate::Nstate<radix> x) { do_assign(x); return *this; } // for b[i] = x
	    reference& operator=(const reference& rhs) { do_assign(rhs); return *this; } // for b[i] = b[j]

        operator nstate::Nstate<radix>() const {
			return m_na.GetDigitInPackedValue(m_na.m_buffer[m_indexIntoBuffer], m_digit);
		}
		operator unsigned() const {
			return m_na.GetDigitInPackedValue(m_na.m_buffer[m_indexIntoBuffer], m_digit);
		}
    };
    reference operator[](size_t pos) {
		nocycle_assert(pos < m_max); // STL will only check bounds on integer boundaries
		size_t indexIntoBuffer = pos / NstatesInPackedType();
		unsigned digit = pos % NstatesInPackedType();
		return reference (*this, indexIntoBuffer, digit);
	}
    nstate::Nstate<radix> operator[](size_t pos) const {
		nocycle_assert(pos < m_max); // STL will only check bounds on integer boundaries.
		size_t indexIntoBuffer = pos / NstatesInPackedType();
		unsigned digit = pos % NstatesInPackedType();
		return GetDigitInPackedValue(m_buffer[indexIntoBuffer], digit);
	}
	
	// by convention, we resize and fill available space with zeros if expanding
	void ResizeWithZeros(size_t max) {
		size_t oldBufferSize = m_buffer.size();
		size_t newBufferSize = max / NstatesInPackedType() +
			(max % NstatesInPackedType() == 0 ? 0 : 1);

		unsigned oldMaxDigitNeeded = m_max % NstatesInPackedType();
		if ((oldMaxDigitNeeded == 0) && (m_max > 0))
			oldMaxDigitNeeded = NstatesInPackedType();
		unsigned newMaxDigitNeeded = max % NstatesInPackedType();
		if ((newMaxDigitNeeded == 0) && (max > 0))
			newMaxDigitNeeded = NstatesInPackedType();
			
		m_buffer.resize(newBufferSize, 0 /* fill value */);
		m_max = max;
		
		if ((newBufferSize == oldBufferSize) && (newMaxDigitNeeded < oldMaxDigitNeeded)) {
			// If we did not change the size of the buffer but merely the number of
			// nstates we are fitting in the lastmost packed value, we must set
			// the trailing unused nstates to zero if we are using fewer nstates
			// than we were before
			for (int eraseDigit = newMaxDigitNeeded; eraseDigit < oldMaxDigitNeeded; eraseDigit++)
				m_buffer[newBufferSize - 1] =
					SetDigitInPackedValue(m_buffer[newBufferSize - 1], eraseDigit, 0); 
		} else if ((newBufferSize < oldBufferSize) && (newMaxDigitNeeded > 0)) {
			// If the number of tristates we are using isn't an even multiple of the 
			// # of states that fit in a packed type, then shrinking will leave some 
			// residual values we need to reset to zero in the last element of the vector.
			for (int eraseDigit = newMaxDigitNeeded; eraseDigit < NstatesInPackedType(); eraseDigit++)
				m_buffer[newBufferSize - 1] =
					SetDigitInPackedValue(m_buffer[newBufferSize - 1], eraseDigit, 0);
		}
	}
	size_t Length() const {
		return m_max;
	}

// Constructors and destructors
public:
	NstateArray<radix>(const size_t initial_size) : 
		m_max (0) 
	{
		EnsurePowerTableBuiltForRadix();
		m_cachedPowerTable = new PackedType[nstatePowerTables[radix].size() + 1];
		m_cachedPowerTable[0] = nstatePowerTables[radix].size();
		for (unsigned index = 0; index < nstatePowerTables[radix].size(); index++) {
			m_cachedPowerTable[index+1] = nstatePowerTables[radix][index];
		}
		ResizeWithZeros(initial_size);
	}
	virtual ~NstateArray<radix>() {
		delete[] m_cachedPowerTable;
	}
	
public:
	static bool SelfTest(); // Class is self-testing for regression
};


// Declaring static members SHOULD be ok. in header files if they are templated
// http://www.velocityreviews.com/forums/t279739-templates-with-static-member-in-header-file.html
// ...but a GCC bug disallows this:
// http://www.cs.utah.edu/dept/old/texinfo/gcc/gppFAQ.html#SEC24
/* 
template<int radix> std::vector<unsigned> NstateArray<radix>::power_table;
template<int radix> unsigned NstateArray<radix>::NSTATES_IN_UNSIGNED = 0; 
*/



//
// CODE FOR SELF-TESTS
//
// See: http://gcc.gnu.org/ml/gcc-bugs/2000-12/msg00168.html
// "If the definitions of member functions of a template aren't visible
// when the template is instantiated, they won't be instantiated.  You
// must define template member functions in the header file itself, at
// least until the `export' keyword is implemented."
//

template <int radix>
nstate::Nstate<radix> NstateArray<radix>::GetDigitInPackedValue(PackedType packed, unsigned digit) const {

	// Generalized from Mark Bessey's post in the Joel on Software forum
	// http://discuss.joelonsoftware.com/default.asp?joel.3.205331.14
	
	nocycle_assert(digit < NstatesInPackedType());
	// lop off unused top digits - you can skip this
	// for the most-significant digit
	PackedType value = packed;
	if (digit < (NstatesInPackedType()-1)) {
		value = value % PowerForDigit(digit+1);
	}
	// truncate lower digit
	value = value / PowerForDigit(digit);
	return value;
}

// For why "typename" is needed, see:
// http://bytes.com/groups/c/538264-expected-constructor-destructor-type-conversion-before
template <int radix>
typename NstateArray<radix>::PackedType NstateArray<radix>::SetDigitInPackedValue(PackedType packed, unsigned digit, nstate::Nstate<radix> t) const {
	nocycle_assert(digit < NstatesInPackedType());
	
	PackedType powForDigitPlusOne = PowerForDigit(digit+1);
	PackedType powForDigit = PowerForDigit(digit);
	
	PackedType upperPart = 0;
	if (digit < (NstatesInPackedType()-1))
		upperPart = (packed / powForDigitPlusOne) * powForDigitPlusOne;

	PackedType setPart = t * powForDigit;
		
	PackedType lowerPart = 0;
	if (digit > 0)
		lowerPart = packed % powForDigit;
		
	return upperPart + setPart + lowerPart;
}



//
// CODE FOR SELF-TESTS
//
// See: http://gcc.gnu.org/ml/gcc-bugs/2000-12/msg00168.html
// "If the definitions of member functions of a template aren't visible
// when the template is instantiated, they won't be instantiated.  You
// must define template member functions in the header file itself, at
// least until the `export' keyword is implemented."
//

#include <cstdlib>
#include <iostream>

namespace nstate {

template <int radix>
bool Nstate<radix>::SelfTest() {
	for (unsigned test = 0; test < radix; test++) {
		Nstate<radix> t (test);
		if (t != test) {
			std::cout << "FAILURE: Nstates did not preserve values properly." << std::endl;
			return false;
		}
	}
	
	try {
		Nstate<radix> t3 (radix);
		std::cout << "FAILURE: Did not detect bad Nstate construction." << std::endl;
		return false;
	} catch (nstate::bad_nstate& e) {
	}
	
	try {
		Nstate<radix> tSet (0);
		tSet = radix;
		std::cout << "FAILURE: Did not detect bad Nstate SetValue." << std::endl;
		return false;
	} catch (nstate::bad_nstate& e) {
	}

    return true;
}

} // end namespace nstate

template <int radix>
bool NstateArray<radix>::SelfTest() {
	// Basic allocation and set test
	for (size_t initialSize = 0; initialSize < 1024; initialSize++) {
		NstateArray<radix> nv (initialSize);
		std::vector<unsigned> v (initialSize);
		for (size_t index = 0; index < initialSize; index++) {
			nstate::Nstate<radix> tRand (rand() % radix);
			nv[index] = tRand;
			v[index] = tRand;
		}
		for (size_t index = 0; index < initialSize; index++) {
			if (nv[index] != v[index]) {
				std::cout << "FAILURE: On NstateArray[" << initialSize << "] for size " << initialSize 
					<< ", a value was recorded as " << static_cast<int>(nv[index]) 
					<< " when it should have been " << static_cast<int>(v[index]) << std::endl;
				return false;
			}
		}
			
		// Try resizing it to some smaller size
		size_t newSmallerSize;
		if (initialSize == 0)
			newSmallerSize = 0;
		else {
			newSmallerSize = (rand() % initialSize);
			nv.ResizeWithZeros(newSmallerSize);
			v.resize(newSmallerSize, 0);
			nocycle_assert(nv.Length() == v.size());
		
			for (size_t index = 0; index < newSmallerSize; index++) {
				if (nv[index] != v[index]) {
					std::cout << "FAILURE: On NstateArray[" << initialSize << "] after resize from " << initialSize
						<< " to " << newSmallerSize
						<< ", a value was recorded as " << static_cast<int>(nv[index]) 
						<< " when it should have been " << static_cast<int>(v[index]) << std::endl;
					return false;
				}
			}
		}
		
		// Try resizing it to some larger size
		size_t newLargerSize = newSmallerSize + (rand() % 128);
		nv.ResizeWithZeros(newLargerSize);
		v.resize(newLargerSize, 0);
		nocycle_assert(nv.Length() == v.size());

		for (size_t index = 0; index < newLargerSize; index++) {
			if (nv[index] != v[index]) {
				std::cout << "FAILURE: On NstateArray[" << initialSize << "] after resize from " << newSmallerSize
					<< " to " << newLargerSize
					<< ", a value was recorded as " << static_cast<int>(nv[index]) 
					<< " when it should have been " << static_cast<int>(v[index]) << std::endl;
				return false;
			}
		}		
	}

	return true;
}

#endif
