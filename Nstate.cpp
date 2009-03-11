//
//  Nstate.cpp - Template for variable that can only take on values 
//     from [0..radix-1], and an array type which is able to compactly
//     store a series of these variables by taking advantage of 
//     the knowledge of the constraint.
//
//  Copyright (c) 2009 HostileFork.com
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
//  See http://hostilefork.com/nstate for documentation.
//

#include <vector>
#include <map>

#include "Nstate.hpp"

// GCC limitation, Static data member templates are not supported.
// http://www.cs.utah.edu/dept/old/texinfo/gcc/gppFAQ.html#SEC32

namespace nocycle {

// due to complications with static members of templates, using a single external variable
// and a map works on more compilers
std::map<unsigned, std::vector<unsigned> > nstatePowerTables;

// Note: If you are going to try and define template static members on an individual
// basis, you needs "template<>" or you will get "too few template-parameter-lists" 
// in gcc4
// http://c2.com/cgi/wiki?TooFewTemplateParameterLists
//template<> std::vector<unsigned> Nstate<3>::power_table;
//template<> unsigned Nstate<3>::NSTATES_IN_UNSIGNED = 0;

}