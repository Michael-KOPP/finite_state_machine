#include "FiniteStateMachine.h"


//States
struct Idle {};
struct Processing { std::string task; };
struct Finished {};

//Events
class Start {};
class ChangeTask {};
class Complete {};
class Reset {};

class StateMachineImpl : public StateMachine<StateMachineImpl, Idle, Processing, Finished>
{
public:
    StateMachineImpl() : StateMachine<StateMachineImpl, Idle, Processing, Finished>(Idle{}) {}

    //Graph
    void handle_event(Start const& event, Idle const&) {
        this->transition_to(Processing{ "Task1" });
    }

    void handle_event(Complete const& event, Processing const&) {
        this->transition_to(Finished{});
    }

    void handle_event(ChangeTask const& event, Processing& state) {
        state.task = "Task2";
    }

    void handle_event(Reset const& event, Finished const&) {
        this->transition_to(Idle{});
    }

    //Stopgap
    template<typename Event, typename State>
    void handle_event(Event const& event, State const& state) {
        throw std::runtime_error(std::string("Unhandled event ") + typeid(event).name() + " in State " + typeid(state).name());
    }
};

class StatePrinter
{
public:
    void operator()(Idle const&) {
        std::cout << "State: Idle" << std::endl;
    }

    void operator()(Processing const& state) {
        std::cout << "State: Processing (" << state.task << ")" << std::endl;
    }

    void operator()(Finished const&) {
        std::cout << "State: Finished" << std::endl;
    }
};

int main() {
    StateMachineImpl fsm;

    StatePrinter printer;
    fsm.visit(printer);
    fsm.safe_handle_event(Start{});
    fsm.visit(printer);
    fsm.safe_handle_event(ChangeTask{});
    fsm.visit(printer);
    fsm.safe_handle_event(Complete{});
    fsm.visit(printer);
    fsm.safe_handle_event(Start{});
    fsm.visit(printer);
    fsm.safe_handle_event(Reset{});
    fsm.visit(printer);

    return 0;
}
