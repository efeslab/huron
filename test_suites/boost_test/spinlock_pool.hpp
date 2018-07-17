#ifndef BOOST_DETAIL_SPINLOCK_POOL_HPP_INCLUDED
#define BOOST_DETAIL_SPINLOCK_POOL_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

//
//  boost/detail/spinlock_pool.hpp
//
//  Copyright (c) 2008 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
//  spinlock_pool<0> is reserved for atomic<>, when/if it arrives
//  spinlock_pool<1> is reserved for shared_ptr reference counts
//  spinlock_pool<2> is reserved for shared_ptr atomic access
//

#include "spinlock.hpp"
#include <cstddef>
#include <cstdio>


class spinlock_pool
{
private:
    spinlock pool_[ 41 ];

public:

    spinlock & spinlock_for( void const * pv )
    {
        std::size_t i = reinterpret_cast< std::size_t >( pv ) % 41;
        return pool_[ i ];
    }

    class scoped_lock
    {
    private:

        spinlock & sp_;

        scoped_lock( scoped_lock const & );
        scoped_lock & operator=( scoped_lock const & );

    public:

        explicit scoped_lock( spinlock_pool *pool, void const * pv ): 
            sp_( pool->spinlock_for( pv ) )
        {
            sp_.lock();
        }

        ~scoped_lock()
        {
            sp_.unlock();
        }
    };
};

#endif // #ifndef BOOST_DETAIL_SPINLOCK_POOL_HPP_INCLUDED
