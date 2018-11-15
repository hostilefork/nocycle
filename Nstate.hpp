//
//  Nstate.hpp - Template for variable that can only take on values
//     from [0..radix-1], and an array type which is able to
//     store a series of these variables and take advantage of
//     the knowledge of the constraint in order to compact them
//     closer to their fundamentally minimal size.
//
//          Copyright (c) 2009-2012 HostileFork.com
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//           http://www.boost.org/LICENSE_1_0.txt)
//
// See http://hostilefork.com/nstate for documentation.
//

#pragma once

#include "NocycleConfig.hpp"

#include <vector>
#include <map>
#include <exception>
#include <cmath>

// REVIEW: std::numeric_limits?
#include <climits>
#include <cassert>

//
// NSTATE
//

namespace nocycle {

// see: http://www.cplusplus.com/doc/tutorial/exceptions.html
class bad_nstate : public std::exception {
    virtual const char* what() const throw() {
        return "Invalid Nstate value";
    }
};

typedef unsigned PackedTypeForNstate;

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

  #if NSTATE_SELFTEST
  public:
    static bool SelfTest(); // Class is self-testing for regression
  #endif
};


class PowerTable {
  private:
    std::vector<unsigned> m_table;
  public:
    PowerTable (unsigned radix) {
        unsigned nstatesInUnsigned = static_cast<unsigned>(floor(log(2)/log(radix)*CHAR_BIT*sizeof(unsigned)));
        PackedTypeForNstate value = 1;
        for (unsigned index = 0; index < nstatesInUnsigned; index++) {
            m_table.push_back(value);
            value = value * radix;
        }
    }
    inline size_t NstatesInPackedType() const {
        return m_table.size();
    }
    inline PackedTypeForNstate PowerForDigit(unsigned digit) const {
        return m_table[digit];
    }
  public:
    virtual ~PowerTable () { }
};


//
// NSTATE ARRAY
//

// TODO:
//   * iterators?
//   * 64-bit packing or something more clever?
//   * memory-mapped file implementation, with standardized packing/order
//     (to work across platforms)?
template <int radix>
class NstateArray {
  private:
    // Note: Typical library limits of the STL for vector lengths
    // are things like 1,073,741,823...
    std::vector<PackedTypeForNstate> m_buffer;
    size_t m_max;

  private:
    static const PowerTable& GetPowerTableInstance() {
        // We wish to cache the PowerTable so that all NstateArray instances of the same
        // radix share the same one.  This is hard to do correctly and/or elegantly in
        // a way that is befitting a "general" class library.

        // http://stackoverflow.com/questions/9507973/how-to-mitigate-user-facing-api-effect-of-shared-members-in-templated-classes

        // The power tables are not that large, and in C++11 code this is guaranteed to
        static const PowerTable instance (radix);
        return instance;
    }
    static size_t NstatesInPackedType() {
        return GetPowerTableInstance().NstatesInPackedType();
    }
    static PackedTypeForNstate PowerForDigit(unsigned digit) {
        return GetPowerTableInstance().PowerForDigit(digit);
    }

  private:
    Nstate<radix> GetDigitInPackedValue(PackedTypeForNstate packed, unsigned digit) const;
    PackedTypeForNstate SetDigitInPackedValue(PackedTypeForNstate packed, unsigned digit, Nstate<radix> t) const;

  public:
    // Derived from boost's dynamic_bitset
    // http://www.boost.org/doc/libs/1_36_0/libs/dynamic_bitset/dynamic_bitset.html
    class reference;
    friend class NstateArray<radix>::reference;
    class reference {
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
        void do_assign(Nstate<radix> x) {
            m_na.m_buffer[m_indexIntoBuffer] =
                m_na.SetDigitInPackedValue(m_na.m_buffer[m_indexIntoBuffer], m_digit, x);
        }
      public:
        // An automatically generated copy constructor.
        reference& operator=(Nstate<radix> x) { do_assign(x); return *this; } // for b[i] = x
        reference& operator=(const reference& rhs) { do_assign(rhs); return *this; } // for b[i] = b[j]

        operator Nstate<radix>() const {
            return m_na.GetDigitInPackedValue(m_na.m_buffer[m_indexIntoBuffer], m_digit);
        }
        operator unsigned() const {
            return m_na.GetDigitInPackedValue(m_na.m_buffer[m_indexIntoBuffer], m_digit);
        }
    };

    reference operator[](size_t pos) {
        assert(pos < m_max); // STL will only check bounds on integer boundaries
        size_t indexIntoBuffer = pos / NstatesInPackedType();
        unsigned digit = pos % NstatesInPackedType();
        return reference (*this, indexIntoBuffer, digit);
    }

    Nstate<radix> operator[](size_t pos) const {
        assert(pos < m_max); // STL will only check bounds on integer boundaries.
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
            for (auto eraseDigit = newMaxDigitNeeded; eraseDigit < oldMaxDigitNeeded; eraseDigit++) {
                m_buffer[newBufferSize - 1] =
                    SetDigitInPackedValue(m_buffer[newBufferSize - 1], eraseDigit, 0);
            }

        } else if ((newBufferSize < oldBufferSize) && (newMaxDigitNeeded > 0)) {

            // If the number of tristates we are using isn't an even multiple of the
            // # of states that fit in a packed type, then shrinking will leave some
            // residual values we need to reset to zero in the last element of the vector.
            for (auto eraseDigit = newMaxDigitNeeded; eraseDigit < NstatesInPackedType(); eraseDigit++) {
                m_buffer[newBufferSize - 1] =
                    SetDigitInPackedValue(m_buffer[newBufferSize - 1], eraseDigit, 0);
            }
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
        ResizeWithZeros(initial_size);
    }
    virtual ~NstateArray<radix> ()
    {
    }

  #if NSTATE_SELFTEST
  public:
    static bool SelfTest(); // Class is self-testing for regression
  #endif
};


//
// Static member functions
//

template <int radix>
Nstate<radix> NstateArray<radix>::GetDigitInPackedValue(PackedTypeForNstate packed, unsigned digit) const {

    // Generalized from Mark Bessey's post in the Joel on Software forum
    // http://discuss.joelonsoftware.com/default.asp?joel.3.205331.14

    assert(digit < NstatesInPackedType());

    // lop off unused top digits - you can skip this
    // for the most-significant digit
    PackedTypeForNstate value = packed;
    if (digit < (NstatesInPackedType()-1)) {
        value = value % PowerForDigit(digit+1);
    }
    // truncate lower digit
    value = value / PowerForDigit(digit);
    return value;
}

template <int radix>
PackedTypeForNstate NstateArray<radix>::SetDigitInPackedValue(PackedTypeForNstate packed, unsigned digit, Nstate<radix> t) const {
    assert(digit < NstatesInPackedType());

    PackedTypeForNstate powForDigitPlusOne = PowerForDigit(digit+1);
    PackedTypeForNstate powForDigit = PowerForDigit(digit);

    PackedTypeForNstate upperPart = 0;
    if (digit < (NstatesInPackedType()-1))
        upperPart = (packed / powForDigitPlusOne) * powForDigitPlusOne;

    PackedTypeForNstate setPart = t * powForDigit;

    PackedTypeForNstate lowerPart = 0;
    if (digit > 0)
        lowerPart = packed % powForDigit;

    return upperPart + setPart + lowerPart;
}

} // temporary end of namespace nocycle


#if NSTATE_SELFTEST

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

namespace nocycle {

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
    } catch (bad_nstate& e) {
    }

    try {
        Nstate<radix> tSet (0);
        tSet = radix;
        std::cout << "FAILURE: Did not detect bad Nstate SetValue." << std::endl;
        return false;
    } catch (bad_nstate& e) {
    }

    return true;
}

template <int radix>
bool NstateArray<radix>::SelfTest() {
    // Basic allocation and set test
    for (size_t initialSize = 0; initialSize < 1024; initialSize++) {
        NstateArray<radix> nv (initialSize);
        std::vector<unsigned> v (initialSize);
        for (size_t index = 0; index < initialSize; index++) {
            Nstate<radix> tRand (rand() % radix);
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
            assert(nv.Length() == v.size());

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
        assert(nv.Length() == v.size());

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

} // end namespace nocycle

#endif
