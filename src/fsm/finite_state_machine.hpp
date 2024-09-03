#pragma once
#include <iostream>
#include <variant>
#include <string>
#include <optional>

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
concept HasHandleEventForState = requires(Event const& event, Child & child, State & state) {
    { child.handle_event_impl(event, state) };
};

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

template<typename Child, typename ... States>
class StateMachine {
protected:
    using State = std::variant<States...>;
public:
    template<OneOf<States...> State>
    StateMachine(State&& initState) : state_(std::forward<State>(initState)) {}

    template <OneOf<States...> NewState>
    void transition_to(NewState&& new_state) {
        std::visit([this](auto&& state) { this->call_leaving_state_handler_if_exists<Child>(state); }, state_);
        state_ = std::forward<NewState>(new_state);
        std::visit([this](auto&& state) { this->call_new_state_handler_if_exists<Child>(state); }, state_);
    }

    template<HasHandleEvent<Child, States...> Event>
    auto handle_event(Event const& event) -> decltype(auto) {
        return std::visit([this, &event](auto& state) { return static_cast<Child*>(this)->handle_event_impl(event, state); }, state_);
    }

    template<ReadOnlyStateFunction<States...> Func>
    auto visit(Func&& func) const -> decltype(auto) {
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
};
