#pragma once
#include <iostream>
#include <variant>
#include <string>

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
    { child.handle_event(event, state) } -> std::same_as<void>;
};

template <typename Event, typename Child, typename ... States>
concept HasHandleEvent = (HasHandleEventForState<Event, Child, States> && ...);

template<typename Func, typename State>
concept ReadOnlyStateFunctionForState = requires(Func && func, State const& state) {
    { func(state) };
};

template<typename Func, typename ... States>
concept ReadOnlyStateFunction = (ReadOnlyStateFunctionForState<Func, States> && ...);

template<typename Child, typename ... States>
class StateMachine {
    using State = std::variant<States...>;
public:
    template<OneOf<States...> State>
    StateMachine(State&& initState) : state_(std::forward<State>(initState)) {}

    template <OneOf<States...> NewState>
    void transition_to(NewState&& new_state) {
        state_ = std::forward<NewState>(new_state);
    }

    template<HasHandleEvent<Child, States...> Event>
    void handle_event(Event const& event) {
        std::visit([this, &event](auto& state) { static_cast<Child*>(this)->handle_event(event, state); }, state_);
    }

    template<ReadOnlyStateFunction<States...> Func>
    void visit(Func&& func) {
        std::visit(func, state_);
    }

    template<HasHandleEvent<Child, States...> Event>
    bool safe_handle_event(Event const& event) {
        try
        {
            this->handle_event(event);
            return true;
        }
        catch (std::exception const& e)
        {
            std::cout << e.what() << std::endl;
            return false;
        }
    }

private:
    State state_;
};

// TODO: Référencez ici les en-têtes supplémentaires nécessaires à votre programme.
