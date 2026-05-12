#include "test_helpers.hpp"

#include <cassert>
#include <memory>
#include <string>
#include <typeinfo>

using namespace event_hub_test;

namespace {

class PlainValueNode final : public event_hub::EventNode {
public:
    explicit PlainValueNode(event_hub::EventBus& bus)
        : EventNode(bus) {}

    void start() {
        m_subscription = subscribe<Ping>([this](const Ping& ping) {
            total += ping.value;
        });
    }

    void stop_one() {
        unsubscribe(m_subscription);
    }

    void publish(int value) {
        post<Ping>(value);
    }

    void on_event(const event_hub::Event&) override {}

    int total = 0;

private:
    SubscriptionId m_subscription = 0;
};

} // namespace

int main() {
    {
        DerivedEvent event(5);

        assert(event.is<DerivedEvent>());
        assert(!event.is<OtherEvent>());
        assert(std::string(event.name()) == "DerivedEvent");
        assert(event.as<DerivedEvent>() == &event);
        assert(event.as<OtherEvent>() == nullptr);

        auto clone = event.clone();
        assert(clone->is<DerivedEvent>());
        assert(clone->as_ref<DerivedEvent>().value == 5);

        bool bad_cast = false;
        try {
            (void)clone->as_ref<OtherEvent>();
        } catch (const std::bad_cast&) {
            bad_cast = true;
        }
        assert(bad_cast);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        CountingListener listener;

        endpoint.subscribe<DerivedEvent>(listener);
        endpoint.emit<DerivedEvent>(5);

        assert(listener.total == 5);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        CountingListener listener;
        auto listener_guard = std::make_shared<int>(0);

        endpoint.subscribe<DerivedEvent>(
            std::weak_ptr<int>(listener_guard),
            listener);

        listener_guard.reset();
        endpoint.emit<DerivedEvent>(7);

        assert(listener.total == 0);
    }

    {
        event_hub::EventBus bus;
        TestNode node(bus);

        assert(&node.bus() == &bus);
        assert(!node.is_closed());

        node.start();

        bus.post<NodeTestEvent>(3);
        assert(bus.process() == 1);
        assert(node.received == 3);

        node.close();
        assert(node.is_closed());

        bus.post<NodeTestEvent>(5);
        assert(bus.process() == 1);
        assert(node.received == 3);
    }

    {
        event_hub::EventBus bus;
        PlainValueNode node(bus);

        node.start();
        bus.emit<Ping>(2);
        assert(node.total == 2);

        node.publish(3);
        assert(bus.process() == 1);
        assert(node.total == 5);

        node.stop_one();
        bus.emit<Ping>(4);
        assert(node.total == 5);
    }

    return 0;
}
