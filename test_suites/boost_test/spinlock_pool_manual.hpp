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
#include <mutex>

class spinlock_pool
{
private:
    const static size_t pad_factor = 64 / sizeof(spinlock);
    const static size_t size = 41;
    spinlock *pool_;
    std::mutex *mutex_pool_;

public:
    spinlock_pool() {
        void *mem;
        mem = malloc(sizeof(spinlock) * size * pad_factor);
        pool_ = new (mem) spinlock[size * pad_factor]();
        mem = malloc(sizeof(std::mutex) * size * 3);
        mutex_pool_ = new (mem) std::mutex[size * 3]();
    }

    spinlock & spinlock_for( void const * pv )
    {
        std::size_t i = reinterpret_cast< std::size_t >( pv ) % size;
        return pool_[ i * pad_factor ];
    }

    std::mutex & mutex_for( void const * pv )
    {
        std::size_t i = reinterpret_cast< std::size_t >( pv ) % size;
        return mutex_pool_[ i * 3 ];
    }

    ~spinlock_pool() {
        delete pool_;
        delete mutex_pool_;
    }

    class scoped_lock
    {
    private:

        spinlock & sp_;
        std::mutex & mutex;

        scoped_lock( scoped_lock const & );
        scoped_lock & operator=( scoped_lock const & );

    public:

        explicit scoped_lock( spinlock_pool &pool, void const * pv ): 
            sp_( pool.spinlock_for( pv ) ), mutex( pool.mutex_for( pv ) )
        {
            mutex.lock();
            sp_.lock();
        }

        ~scoped_lock()
        {
            sp_.unlock();
            mutex.unlock();
        }
    };
};

#endif // #ifndef BOOST_DETAIL_SPINLOCK_POOL_HPP_INCLUDED
