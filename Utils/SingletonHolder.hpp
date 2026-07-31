#pragma once
#include <concepts>

namespace utils {
    template<class T>
    class SingletonHolder {
    public:
        SingletonHolder(const SingletonHolder&) = delete;
        SingletonHolder& operator=(const SingletonHolder&) = delete;
        SingletonHolder(SingletonHolder&&) = delete;
        SingletonHolder& operator=(SingletonHolder&&) = delete;

        [[nodiscard]] static T& get_instance()
        {
            static T unique_instance;
            return unique_instance;
        }

    protected:
        SingletonHolder() = default;
        ~SingletonHolder() = default;
    };

    template<class T>
    concept Singleton = std::derived_from<T, SingletonHolder<T>>;
} // namespace utils
