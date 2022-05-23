#ifndef HORIZON_DEBUG_H
#define HORIZON_DEBUG_H

#include <iostream>

#define runtime_assert(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Runtime assertion `" #condition "` failed: " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#define runtime_weak_assert(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Runtime weak assertion `" #condition "` failed: " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)


#ifndef NDEBUG 
#define assert(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#define weak_assert(condition, message) \
    if (! (condition)) \
    { \
        std::cerr << "Weak Assertion '" #condition "' failed in " << __FILE__\
                << " line " << __LINE__ << ": " << message << std::endl; \
    }

#else
#define weak_assert(condition, message) condition;
#define assert(condition, message) condition;

#endif

#endif