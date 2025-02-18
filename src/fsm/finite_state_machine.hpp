#pragma once
#include <iostream>
#include <variant>
#include <mutex>
template<typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template<typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;


template<typename T, typename... Ts>
struct is_one_of;

template<typename T>
struct is_one_of<T> : std::false_type {};

template<typename T, typename First, typename... Rest>
struct is_one_of<T, First, Rest...>
    : std::conditional_t<std::is_same_v<T, First>, std::true_type, is_one_of<T, Rest...>> {};

template<typename T, typename... Ts>
concept OneOf = is_one_of<T, Ts...>::value;

template<typename ... States>
struct is_one_of_predicate
{
    template<typename T>
    using predicate = is_one_of<T, States...>;
};

template <typename Event, typename Child, typename State>
struct has_handle_event_for_state
{
    template<typename E, typename C, typename S>
    static constexpr auto test(E const * e, C * c, S * s) -> decltype(c->handle_event_impl(*e, *s), std::true_type{});

    template<typename E, typename C, typename S>
    static constexpr auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<Event, Child, State>(nullptr, nullptr, nullptr))::value;
};

template <typename Event, typename Child, typename State>
constexpr bool has_handle_event_for_state_v = has_handle_event_for_state<Event, Child, State>::value;

template <typename Event, typename Child, typename State>
concept HasHandleEventForState = has_handle_event_for_state_v<Event, Child, State>;

template <typename Event, typename Child, typename ... States>
concept HasHandleEvent = (HasHandleEventForState<Event, Child, States> && ...);

template<typename Func, typename State>
concept ReadOnlyStateFunctionForState = requires(Func && func, State const& state) {
    { func(state) };
};

template<typename T, typename State>
struct has_on_leaving_state_handler {

    template<typename V>
    static constexpr auto test(State * s) -> std::is_same<decltype(std::declval<V>().on_leaving_state(*s)), void>;

    template<typename V>
    static constexpr auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<T>(nullptr))::value;
};

template<typename T, typename State>
constexpr bool has_on_leaving_state_handler_v = has_on_leaving_state_handler<T, State>::value;

template<typename T, typename State>
concept HasOnLeavingStateHandler = has_on_leaving_state_handler_v<T, State>;

template<typename T, typename State>
struct has_on_new_state_handler {

    template<typename V>
    static constexpr auto test(State * s) -> std::is_same<decltype(std::declval<V>().on_new_state(*s)), void>;

    template<typename V>
    static constexpr auto test(...) -> std::false_type;

    static constexpr bool value = decltype(test<T>(nullptr))::value;
};

template<typename T, typename State>
constexpr bool has_on_new_state_handler_v = has_on_new_state_handler<T, State>::value;

template<typename T, typename State>
concept HasOnNewStateHandler = has_on_new_state_handler_v<T, State>;

template<typename Func, typename ... States>
concept ReadOnlyStateFunction = (ReadOnlyStateFunctionForState<Func, States> && ...);

#define ENABLE_PRIVATE_HANDLE_EVENT_IMPL     template <typename Event, typename Child, typename State> \
friend struct has_handle_event_for_state;


#define ENABLE_PRIVATE_ON_NEW_STATE     template<typename T, typename State> \
friend struct has_on_new_state_handler;

#define ENABLE_PRIVATE_ON_LEAVING_STATE     template<typename T, typename State> \
friend struct has_on_leaving_state_handler;

#define ENABLE_PRIVATE_IMPL friend class StateMachine; \
ENABLE_PRIVATE_HANDLE_EVENT_IMPL \
    ENABLE_PRIVATE_ON_NEW_STATE \
    ENABLE_PRIVATE_ON_LEAVING_STATE

    template<typename Child, typename ... States>
    class StateMachine {
protected:
    using State = std::variant<States...>;
public:
    template<OneOf<States...> State>
    StateMachine(State&& initState) : state_(std::forward<State>(initState)) {}

    StateMachine(StateMachine const& other) {
        std::scoped_lock lock(_mutex);
        state_ = other.state_;
    }

    StateMachine(StateMachine&& other) {
        swap(other);
    }

    StateMachine& operator=(StateMachine other) {
        swap(other);
        return *this;
    }

    void swap(StateMachine& other) {
        std::scoped_lock lock(_mutex);
        state_.swap(other.state_);
    }

    template<HasHandleEvent<Child, States...> Event>
    auto handle_event(Event const& event) -> decltype(auto) {
        std::scoped_lock lock(_mutex);
        return std::visit([this, &event](auto& state) { return static_cast<Child*>(this)->handle_event_impl(event, state); }, state_);
    }

    /**
     * @warning using handle_event in visit will deadlock
     * @return @func return value
     */
    template<ReadOnlyStateFunction<States...> Func>
    auto visit(Func&& func) const -> decltype(auto) {
        std::scoped_lock lock(_mutex);
        return std::visit(func, state_);
    }

    template<template<typename> typename Pred, typename Func>
    auto exec_if(Func&& func) const -> decltype(auto) {
        return visit([&func](auto&& state) {
            using Stt = std::remove_cvref_t<decltype(state)>;
            if constexpr(Pred<Stt>::value) {
                return func();
            }
            else {
                return std::invoke_result_t<Func>();
            }
        });
    }

    template<typename ... _States, typename Func>
    auto exec_if_one_of(Func&& func) const -> decltype(auto) {
        return exec_if<is_one_of_predicate<_States...>::predicate, Func>(std::move(func));
    }

    template<HasHandleEvent<Child, States...> Event>
    auto safe_handle_event(Event const& event) -> decltype(auto) {
        try {
            this->handle_event(event);
            return true;
        } catch (std::exception const& e) {
            std::cout << e.what() << std::endl;
            return false;
        }
    }
protected:
    template <OneOf<States...> NewState>
    void transition_to(NewState&& new_state) {
        std::visit([this](auto&& state) { this->call_leaving_state_handler_if_exists<Child>(state); }, state_);
        state_ = std::forward<NewState>(new_state);
        std::visit([this](auto&& state) { this->call_new_state_handler_if_exists<Child>(state); }, state_);
    }
private:
    template<typename C, typename _State>
    void call_leaving_state_handler_if_exists(_State&& state) {
        if constexpr(HasOnLeavingStateHandler<C, std::remove_cvref_t<_State>>) {
            static_cast<Child*>(this)->on_leaving_state(state);
        }
    }
    template<typename C, typename _State>
    void call_new_state_handler_if_exists(_State&& state) {
        if constexpr (HasOnNewStateHandler<C, std::remove_cvref_t<_State>>) {
            static_cast<Child*>(this)->on_new_state(state);
        }
    }

private:
    State state_;
    mutable std::recursive_mutex _mutex;
};

#include <vector>
#include <functional>

template <typename T>
struct FunctionTraits;

// Cas d'une fonction classique ou d'un lambda
template <typename Ret, typename Arg>
struct FunctionTraits<Ret(Arg)> {
    using ArgumentType = Arg;
};

// Cas d'un pointeur de fonction
template <typename Ret, typename Arg>
struct FunctionTraits<Ret(*)(Arg)> {
    using ArgumentType = Arg;
};

// Cas d'un std::function
template <typename Ret, typename Arg>
struct FunctionTraits<std::function<Ret(Arg)>> {
    using ArgumentType = Arg;
};

// Cas général pour les lambdas et foncteurs
template <typename T>
struct FunctionTraits : FunctionTraits<decltype(&T::operator())> {};

// Cas d'un opérateur de fonction
template <typename C, typename Ret, typename Arg>
struct FunctionTraits<Ret(C::*)(Arg) const> {
    using ArgumentType = Arg;
};

template<typename T>
struct TypedId {
    using type = T;

    size_t id;
};

template<typename Child, typename ... States>
class NotifyingStateMachine : public StateMachine<Child, States...> {
public:
    using StateMachine<Child, States...>::StateMachine;

    template<typename Handler>
    auto on_state(Handler&& handler) -> decltype(auto) {
        using State = std::remove_cvref_t<typename FunctionTraits<Handler>::ArgumentType>;
        auto& [prevId, vec] = std::get<std::pair<size_t, std::vector<std::pair<size_t, std::function<void(State const&)>>>>>(_stateHandlers);
        vec.push_back(std::make_pair(++prevId, std::forward<Handler>(handler)));
        return TypedId<State>(prevId);
    }

    template<typename _TypeId>
    auto unregister(_TypeId id) -> bool {
        auto& [prevId, vec] = std::get<std::pair<size_t, std::vector<std::pair<size_t, std::function<void(typename _TypeId::type const&)>>>>>(_stateHandlers);
        auto toErase = std::find_if(vec.begin(), vec.end(), [&](auto& pair) {return pair.first == id.id;});
        if(toErase != vec.cend()) {
            vec.erase(toErase);
            return true;
        }
        return false;
    }

    template<OneOf<States...> State>
    void on_new_state(State const& state) {
        auto& [prevId, vec] = std::get<std::pair<size_t, std::vector<std::pair<size_t, std::function<void(State const&)>>>>>(_stateHandlers);

        for(auto& [id, handler] : vec) {
            handler(state);
        }
    }
private:
    std::tuple<std::pair<size_t, std::vector<std::pair<size_t, std::function<void(States const&)>>>>...> _stateHandlers;
};
