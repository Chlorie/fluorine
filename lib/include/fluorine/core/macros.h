#pragma once

#if defined(_MSC_VER)
    #define FLUORINE_API_IMPORT __declspec(dllimport)
    #define FLUORINE_API_EXPORT __declspec(dllexport)
    #define FLUORINE_SUPPRESS_EXPORT_WARNING __pragma(warning(push)) __pragma(warning(disable: 4251 4275))
    #define FLUORINE_RESTORE_EXPORT_WARNING __pragma(warning(pop))
#elif defined(__GNUC__)
    #define FLUORINE_API_IMPORT
    #define FLUORINE_API_EXPORT __attribute__((visibility("default")))
    #define FLUORINE_SUPPRESS_EXPORT_WARNING
    #define FLUORINE_RESTORE_EXPORT_WARNING
#else
    #define FLUORINE_API_IMPORT
    #define FLUORINE_API_EXPORT
    #define FLUORINE_SUPPRESS_EXPORT_WARNING
    #define FLUORINE_RESTORE_EXPORT_WARNING
#endif

#if defined(FLUORINE_BUILD_SHARED)
    #ifdef FLUORINE_EXPORT_SHARED
        #define FLUORINE_API FLUORINE_API_EXPORT
    #else
        #define FLUORINE_API FLUORINE_API_IMPORT
    #endif
#else
    #define FLUORINE_API
#endif

#if __cplusplus >= 202302L && __cpp_if_consteval >= 202106L
    #define FLUORINE_IF_CONSTEVAL if consteval
    #define FLUORINE_IF_NOT_CONSTEVAL if !consteval
#else
    #define FLUORINE_IF_CONSTEVAL if (std::is_constant_evaluated())
    #define FLUORINE_IF_NOT_CONSTEVAL if (!std::is_constant_evaluated())
#endif

#if defined(__BMI2__) || defined(__AVX2__)
    #define FLUORINE_HAS_BMI2 1
#endif
