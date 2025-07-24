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
    // Event system for registering and invoking callbacks with specified arguments
    template<typename... Args>
    class fwEvent
    {
    public:
        using TFunc = std::function<bool(Args...)>;

        // Represents a single callback with its function, order, and cookie
        struct callback
        {
            TFunc function;
            std::unique_ptr<callback> next = nullptr;
            int order = 0;
            size_t cookie = 0;

            explicit callback(TFunc func) noexcept
                : function(std::move(func)) {}
        };

        // Invokes all registered callbacks in order, stopping if any returns false
        bool Invoke(Args... args) const noexcept
        {
            std::unique_lock<std::mutex> lock(m_callbacksMutex);
            for (auto* current = m_callbacks.get(); current; current = current->next.get())
            {
                if (!current->function(std::forward<Args>(args)...))
                {
                    return false;
                }
            }
            return true;
        }

        // Removes a callback by its cookie
        bool Disconnect(size_t cookie) noexcept
        {
            std::unique_lock<std::mutex> lock(m_callbacksMutex);
            if (!m_callbacks) return false;

            if (m_callbacks->cookie == cookie)
            {
                m_callbacks = std::move(m_callbacks->next);
                return true;
            }

            auto* current = m_callbacks.get();
            while (current->next && current->next->cookie != cookie)
            {
                current = current->next.get();
            }

            if (current->next)
            {
                current->next = std::move(current->next->next);
                return true;
            }

            return false;
        }

        std::unique_ptr<callback> m_callbacks;
        std::atomic<size_t> m_connectCookie{0};
        mutable std::mutex m_callbacksMutex;
    };

    // Base class for resources with event hooks for lifecycle management
    class Resource
    {
    public:
        // Padding for ABI compatibility or memory alignment (adjust as needed)
        char pad_040[0x40];
        fx::fwEvent<std::vector<char>*> OnBeforeLoadScript; // Called before loading a script
        fx::fwEvent<> OnStart;                              // Called when resource starts
        fx::fwEvent<> OnStop;                               // Called when resource stops
        fx::fwEvent<> OnEnter;                              // Called when resource is entered
        fx::fwEvent<> OnLeave;                              // Called when resource is left
        fx::fwEvent<> OnCreate;                             // Called when resource is created
        fx::fwEvent<> OnActivate;                           // Called when resource is activated
        fx::fwEvent<> OnDeactivate;                         // Called when resource is deactivated
        fx::fwEvent<> OnRemove;                             // Called when resource is removed
    };

    // Implementation of Resource with additional metadata
    class ResourceImpl : public Resource
    {
    public:
        std::string m_name; // Resource name
    };

    // Network library for server communication
    class NetLibrary
    {
    public:
        // Padding for ABI compatibility or memory alignment (adjust as needed)
        char pad[0x0f8];
        std::string m_currentServerUrl; // Current server URL
    };

    // Internal function to connect a callback to an event
    template<typename... Args>
    inline size_t ConnectInternal(fx::fwEvent<Args...>& event, typename fx::fwEvent<Args...>::TFunc func, int order) noexcept
    {
        if (!func)
            return static_cast<size_t>(-1);

        size_t cookie = event.m_connectCookie.fetch_add(1, std::memory_order_acq_rel);
        auto cb = std::make_unique<typename fx::fwEvent<Args...>::callback>(std::move(func));
        cb->order = order;
        cb->cookie = cookie;

        std::unique_lock<std::mutex> lock(event.m_callbacksMutex);

        // Insert callback in ordered position (ascending by order)
        if (!event.m_callbacks || order < event.m_callbacks->order)
        {
            cb->next = std::move(event.m_callbacks);
            event.m_callbacks = std::move(cb);
        }
        else
        {
            auto* current = event.m_callbacks.get();
            while (current->next && current->next->order <= order)
            {
                current = current->next.get();
            }
            cb->next = std::move(current->next);
            current->next = std::move(cb);
        }

        return cookie;
    }

    // Connects a function to an event, supporting both bool and void return types
    template<typename... Args, typename T>
    inline auto Connect(fx::fwEvent<Args...>& event, T&& func, int order = 0) noexcept
    {
        using ResultType = std::invoke_result_t<T, Args...>;
        if constexpr (std::is_same_v<ResultType, bool>)
        {
            return ConnectInternal(event, std::forward<T>(func), order);
        }
        else if constexpr (std::is_void_v<ResultType>)
        {
            return ConnectInternal(event,
                [f = std::forward<T>(func)](Args&&... args) -> bool
                {
                    std::invoke(f, std::forward<Args>(args)...);
                    return true;
                }, order);
        }
        else
        {
            static_assert(sizeof...(Args) == 0, "Invalid function signature: must return bool or void.");
            return static_cast<size_t>(-1);
        }
    }
}