#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
// Windows 头文件
#include <windows.h>

#ifndef STUPID_LIBRARY_EXPORTS
#define EXPORT_API __declspec(dllexport)
#else
#define EXPORT_API __declspec(dllimport)
#endif // STUPID_LIBRARY_EXPORTS

#define VECTOR_EXPORT(type)                                                                                                  \
    template class EXPORT_API std::_Vector_val<std::_Simple_types<type>>;                                                    \
    template class EXPORT_API std::_Compressed_pair<std::allocator<type>, std::_Vector_val<std::_Simple_types<type>>, true>; \
    template class EXPORT_API std::vector<type, std::allocator<type>>;
