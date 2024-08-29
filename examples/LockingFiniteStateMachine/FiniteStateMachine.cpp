#include <chrono>
#include <thread>
#include <condition_variable>
#include <mutex>

#include <fsm/finite_state_machine.hpp>

//States
struct Idle {};
struct Ready {};
struct Processing { std::string task; };
struct Finished {};

class StatePrinter
{
public:
    void operator()(Idle const&) {
        std::cout << "State: Idle" << std::endl;
    }

    void operator()(Ready const&) {
        std::cout << "State: Ready" << std::endl;
    }

    void operator()(Processing const& state) {
        std::cout << "State: Processing (" << state.task << ")" << std::endl;
    }

    void operator()(Finished const&) {
        std::cout << "State: Finished" << std::endl;
    }
};


//Events
class Init {};
class Start {};
class ChangeTask {};
class Complete {};
class Reset {};

class StateMachineImpl : public StateMachine<StateMachineImpl, Idle, Ready, Processing, Finished>
{
public:
    StateMachineImpl() : StateMachine<StateMachineImpl, Idle, Ready, Processing, Finished>(Idle{}) {}

    ~StateMachineImpl() {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]{
            return quit_;
        });
    }

    //Graph
    void handle_event_impl(Init const& event, Idle const&) {
        this->transition_to(Ready{});
    }

    void handle_event_impl(Start const& event, Ready const&) {
        this->transition_to(Processing{ "Task1" });
    }

    void handle_event_impl(Complete const& event, Processing const&) {
        this->transition_to(Finished{});
        std::scoped_lock lock(mutex_);
        quit_ = true;
        cv_.notify_all();
    }

    void handle_event_impl(ChangeTask const& event, Processing& state) {
        state.task = "Task2";
    }

    void handle_event_impl(Reset const& event, Finished const&) {
    }

    //Stopgap
    template<typename Event, typename State>
    void handle_event_impl(Event const& event, State const& state) {
        throw std::runtime_error(std::string("Unhandled event ") + typeid(event).name() + " in State " + typeid(state).name());
    }

    template<typename NewState>
    void on_new_state(NewState const& state) {

        StatePrinter p;
        std::cout << "New ";
        p(state);
        if constexpr (std::is_same_v<NewState, Ready>) {
            std::thread t([&]{
                std::this_thread::sleep_for(std::chrono::seconds(3));
                this->safe_handle_event(Start{});
            });
            t.detach();
        }
        else if constexpr (std::is_same_v<NewState, Processing>) {
            this->safe_handle_event(Complete{});
        }
    }

    template<typename OldState>
    void on_leaving_state(OldState const& state) {
        StatePrinter p;
        std::cout << "Leaving ";
        p(state);

    }
private:
    std::condition_variable cv_;
    std::mutex mutex_;
    bool quit_ = false;
};


int main() {
    StateMachineImpl fsm;

    StatePrinter printer;
    fsm.visit(printer);
    fsm.safe_handle_event(Init{});


    return 0;
}
