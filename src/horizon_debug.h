#ifndef HORIZON_DEBUG_H
#define HORIZON_DEBUG_H

#include <iostream>


#ifndef NDEBUG 
#define ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#define WEAK_ASSERT(condition, message) \
    if (! (condition)) \
    { \
        std::cerr << "Weak Assertion '" #condition "' failed in " << __FILE__\
                << " line " << __LINE__ << ": " << message << std::endl; \
    }

#define RUNTIME_ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Runtime assertion `" #condition "` failed in " << __FILE__ \
                      << " line " << __LINE__ << ": " << message << std::endl; \
        } \
    } while (false)

#define RUNTIME_WEAK_ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Runtime weak assertion '" #condition "' failed in " << __FILE__\
                << " line " << __LINE__ << ": " << message << std::endl; \
        } \
    } while (false)

#else
#define WEAK_ASSERT(condition, message) condition;
#define ASSERT(condition, message) condition;

#define RUNTIME_ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Runtime assertion `" #condition "` failed: " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#define RUNTIME_WEAK_ASSERT(condition, message) \
    do { \
        if (! (condition)) { \
            std::cerr << "Runtime weak assertion `" #condition "` failed: " << message << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#endif

#endif