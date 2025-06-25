#pragma once
#include <type_traits>

#define DELETE_COPY_AND_MOVE(class_name)          \
    class_name(class_name&&) = delete;            \
    class_name(const class_name&) = delete;       \
    class_name& operator=(class_name&&) = delete; \
    class_name& operator=(const class_name&) = delete

namespace utils {
    template<typename T>
    class SingletonHolder {
        DELETE_COPY_AND_MOVE(SingletonHolder);

    public:
        static T& get_instance()
        {
            static T unique_instance;
            return unique_instance;
        }

        virtual ~SingletonHolder() = default;

    protected:
        SingletonHolder() = default;
    };

    template<typename T>
    concept Singleton = std::is_base_of_v<SingletonHolder<T>, T>;
}
