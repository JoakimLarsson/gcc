// { dg-do compile }

// Copyright (C) 2007 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 2, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License along
// with this library; see the file COPYING.  If not, write to the Free
// Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
// USA.

#include <csignal>

namespace gnu
{
#ifndef SIGABRT
    #error "SIGABRT_must_be_a_macro"
#endif

#ifndef SIGILL
    #error "SIGILL_must_be_a_macro"
#endif

#ifndef SIGSEGV
    #error "SIGSEGV_must_be_a_macro"
#endif

#ifndef SIG_DFL
    #error "SIG_DFL_must_be_a_macro"
#endif

#ifndef SIGFPE
    #error "SIGFPE_must_be_a_macro"
#endif

#ifndef SIGINT
    #error "SIGINT_must_be_a_macro"
#endif

#ifndef SIGTERM
    #error "SIGTERM_must_be_a_macro"
#endif

#ifndef SIG_ERR
    #error "SIG_ERR_must_be_a_macro"
#endif
}
