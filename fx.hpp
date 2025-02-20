#pragma once

#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <type_traits>
#include <mutex>

namespace fx
{
    template<typename... Args>
    class fwEvent
    {
    public:
        using TFunc = std::function<bool(Args...)>;
    
        struct callback
        {
            TFunc function;
            std::unique_ptr<callback> next = nullptr;
            int order = 0;
            size_t cookie = 0;

            explicit callback(TFunc func)
                : function(std::move(func)) {}
        };
    
        std::unique_ptr<callback> m_callbacks;
        std::atomic<size_t> m_connectCookie = 0;
        mutable std::mutex m_callbacksMutex;
    };

    class Resource
    {
    public:
        char pad_040[0x40];
        fx::fwEvent<std::vector<char>*> OnBeforeLoadScript;
        fx::fwEvent<> OnStart;
        fx::fwEvent<> OnStop;
        fx::fwEvent<> OnEnter;
        fx::fwEvent<> OnLeave;
        fx::fwEvent<> OnCreate;
        fx::fwEvent<> OnActivate;
        fx::fwEvent<> OnDeactivate;
        fx::fwEvent<> OnRemove;
    };

    class ResourceImpl : public Resource
    {
    public:
        std::string m_name;
    };

    class NetLibrary
    {
    public:
        char pad[0x0f8];
        std::string m_currentServerUrl;
    };

    template<typename... Args>
    inline size_t ConnectInternal(fx::fwEvent<Args...>& event, typename fx::fwEvent<Args...>::TFunc func, int order)
    {
        if (!func)
            return static_cast<size_t>(-1);

        size_t cookie = event.m_connectCookie.fetch_add(1, std::memory_order_relaxed);
        auto cb = std::make_unique<typename fx::fwEvent<Args...>::callback>(std::move(func));
        cb->order = order;
        cb->cookie = cookie;

        std::unique_lock<std::mutex> lock(event.m_callbacksMutex);

        if (!event.m_callbacks)
        {
            event.m_callbacks = std::move(cb);
        }
        else
        {
            auto* cur = &event.m_callbacks;
            typename fx::fwEvent<Args...>::callback* last = nullptr;

            while (*cur && order >= (*cur)->order)
            {
                last = cur->get();
                cur = &(*cur)->next;
            }

            cb->next = std::move(*cur);
            (!last ? event.m_callbacks : last->next) = std::move(cb);
        }

        return cookie;
    }

    template<typename... Args, typename T>
    inline auto Connect(fx::fwEvent<Args...>& event, T func)
    {
        return Connect(event, func, 0);
    }

    template<typename... Args, typename T>
    inline auto Connect(fx::fwEvent<Args...>& event, T func, int order)
    {
        if constexpr (std::is_invocable_r_v<bool, T, Args...>)
        {
            return ConnectInternal(event, func, order);
        }
        else if constexpr (std::is_invocable_v<T, Args...>)
        {
            return ConnectInternal(event, [func](Args&&... args)
                {
                    std::invoke(func, std::forward<Args>(args)...);
                    return true;
                },
                order);
        }
        else
        {
            static_assert(sizeof...(Args) == 0, "Invalid function signature.");
            return static_cast<size_t>(-1);
        }
    }
}
