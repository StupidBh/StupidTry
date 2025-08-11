#pragma once
#include <cstddef>

enum class CoreType
{
    Physical, // 物理核心数
    Logical,  // 逻辑核心数
    Total     // 总核心数（通常与逻辑核心数一致）
};

/// @brief 获取 CPU 核心数
/// @param type 核心类型（物理/逻辑/总核心数）
/// @return 核心数
/// @note 在 Windows 使用 WMIC 命令获取物理核心数，在非 Windows 使用 sysconf。
[[nodiscard]] size_t get_core_count(CoreType type);
