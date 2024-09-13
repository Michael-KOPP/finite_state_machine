#pragma once
#include <iostream>
#include <variant>
#include <mutex>

template<typename T, typename... Ts>
struct is_one_of;

template<typename T>
struct is_one_of<T> : std::false_type {};

template<typename T, typename First, typename... Rest>
struct is_one_of<T, First, Rest...>
    : std::conditional_t<std::is_same_v<T, First>, std::true_type, is_one_of<T, Rest...>> {};

template<typename T, typename... Ts>
concept OneOf = is_one_of<T, Ts...>::value;

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

    template <OneOf<States...> NewState>
    void transition_to(NewState&& new_state) {
        std::visit([this](auto&& state) { this->call_leaving_state_handler_if_exists<Child>(state); }, state_);
        state_ = std::forward<NewState>(new_state);
        std::visit([this](auto&& state) { this->call_new_state_handler_if_exists<Child>(state); }, state_);
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
    mutable std::mutex _mutex;
};
