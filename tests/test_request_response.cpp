#include <event_hub.hpp>

#include <cassert>
#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

struct EchoRequest {
    event_hub::RequestId request_id = event_hub::invalid_request_id;
    int value = 0;
};

struct EchoResult {
    event_hub::RequestId request_id = event_hub::invalid_request_id;
    int value = 0;
};

struct CustomRequest {
    event_hub::RequestId correlation_id = event_hub::invalid_request_id;
    int value = 0;
};

struct CustomResult {
    event_hub::RequestId correlation_id = event_hub::invalid_request_id;
    int value = 0;
};

struct ReplyRequest {
    int value = 0;
    event_hub::Reply<EchoResult> reply;
};

} // namespace

namespace event_hub {

template <>
struct RequestTraits<CustomRequest> {
    static RequestId get_id(const CustomRequest& event) noexcept {
        return event.correlation_id;
    }

    static void set_id(CustomRequest& event, RequestId id) noexcept {
        event.correlation_id = id;
    }
};

template <>
struct RequestTraits<CustomResult> {
    static RequestId get_id(const CustomResult& event) noexcept {
        return event.correlation_id;
    }

    static void set_id(CustomResult& event, RequestId id) noexcept {
        event.correlation_id = id;
    }
};

} // namespace event_hub

namespace {

class RequestNode final : public event_hub::EventNode {
public:
    explicit RequestNode(event_hub::EventBus& bus)
        : EventNode(bus) {}

    event_hub::RequestId start(int value, int& output) {
        EchoRequest event;
        event.value = value;

        return request<EchoRequest, EchoResult>(
            event,
            [&output](const EchoResult& result) {
                output = result.value;
            });
    }

    std::future<EchoResult> start_future(int value) {
        EchoRequest event;
        event.value = value;
        return request_future<EchoRequest, EchoResult>(event);
    }

    event_hub::RequestId allocate_request_id() {
        return next_request_id();
    }
};

class RequestModule final : public event_hub::Module {
public:
    explicit RequestModule(event_hub::EventBus& bus)
        : Module(bus) {}

    event_hub::RequestId start(int value, int& output) {
        EchoRequest event;
        event.value = value;

        return request<EchoRequest, EchoResult>(
            event,
            [&output](const EchoResult& result) {
                output = result.value;
            });
    }
};

void add_echo_service(event_hub::EventEndpoint& service) {
    service.subscribe<EchoRequest>([&service](const EchoRequest& request) {
        EchoResult result;
        result.request_id = request.request_id;
        result.value = request.value * 2;
        service.post<EchoResult>(result);
    });
}

void test_request_callback() {
    event_hub::EventBus bus;
    event_hub::EventEndpoint client(bus);
    event_hub::EventEndpoint service(bus);
    add_echo_service(service);

    int output = 0;
    EchoRequest event;
    event.value = 21;

    const auto id = client.request<EchoRequest, EchoResult>(
        event,
        [&output](const EchoResult& result) {
            output = result.value;
        });

    assert(id != event_hub::invalid_request_id);
    assert(bus.process() == 1U);
    assert(output == 0);
    assert(bus.process() == 1U);
    assert(output == 42);
}

void test_request_future() {
    event_hub::EventBus bus;
    event_hub::EventEndpoint client(bus);
    event_hub::EventEndpoint service(bus);
    add_echo_service(service);

    EchoRequest event;
    event.value = 7;

    auto future = client.request_future<EchoRequest, EchoResult>(event);

    assert(bus.process() == 1U);
    assert(future.wait_for(std::chrono::milliseconds(0)) ==
           std::future_status::timeout);

    assert(bus.process() == 1U);
    const auto result = future.get();
    assert(result.value == 14);
}

void test_request_future_timeout() {
    event_hub::EventBus bus;
    event_hub::EventEndpoint client(bus);

    EchoRequest event;
    event.value = 1;

    bool timeout_called = false;
    auto options = event_hub::AwaitOptions::timeout_ms(1);
    options.on_timeout = [&timeout_called] {
        timeout_called = true;
    };

    auto future = client.request_future<EchoRequest, EchoResult>(event, options);

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(250);
    while (future.wait_for(std::chrono::milliseconds(0)) !=
           std::future_status::ready) {
        (void)bus.process();
        if (std::chrono::steady_clock::now() >= deadline) {
            throw std::runtime_error("request_future timeout test did not complete");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    assert(timeout_called);

    try {
        (void)future.get();
        assert(false && "request_future must throw after timeout");
    } catch (const event_hub::RequestTimeoutError& error) {
        assert(error.request_id() != event_hub::invalid_request_id);
    }
}

void test_request_traits_custom_field() {
    event_hub::EventBus bus;
    event_hub::EventEndpoint client(bus);
    event_hub::EventEndpoint service(bus);

    service.subscribe<CustomRequest>([&service](const CustomRequest& request) {
        CustomResult result;
        result.correlation_id = request.correlation_id;
        result.value = request.value + 3;
        service.post<CustomResult>(result);
    });

    int output = 0;
    CustomRequest event;
    event.value = 4;

    const auto id = client.request<CustomRequest, CustomResult>(
        event,
        [&output](const CustomResult& result) {
            output = result.value;
        });

    assert(id != event_hub::invalid_request_id);
    assert(bus.process() == 1U);
    assert(bus.process() == 1U);
    assert(output == 7);
}

void test_reply_callback_in_event() {
    event_hub::EventBus bus;
    event_hub::EventEndpoint client(bus);
    event_hub::EventEndpoint service(bus);

    service.subscribe<ReplyRequest>([](const ReplyRequest& request) {
        EchoResult result;
        result.value = request.value * 3;
        request.reply(result);
    });

    int output = 0;
    ReplyRequest event;
    event.value = 5;
    event.reply = event_hub::Reply<EchoResult>(
        [&output](const EchoResult& result) {
            output = result.value;
        });

    client.post<ReplyRequest>(event);
    assert(bus.process() == 1U);
    assert(output == 15);

    auto copy = event.reply;
    EchoResult result;
    result.value = 2;
    copy(result);
    assert(output == 2);

    copy.reset();
    assert(!copy);
}

void test_event_node_request_helpers() {
    event_hub::EventBus bus;
    event_hub::EventEndpoint service(bus);
    add_echo_service(service);
    RequestNode node(bus);

    int output = 0;
    const auto id = node.start(6, output);

    assert(id != event_hub::invalid_request_id);
    assert(node.allocate_request_id() != id);
    assert(bus.process() == 1U);
    assert(bus.process() == 1U);
    assert(output == 12);

    auto future = node.start_future(8);
    assert(bus.process() == 1U);
    assert(bus.process() == 1U);
    assert(future.get().value == 16);
}

void test_module_inherits_request_helpers() {
    event_hub::EventBus bus;
    event_hub::EventEndpoint service(bus);
    add_echo_service(service);
    RequestModule module(bus);

    int output = 0;
    const auto id = module.start(9, output);

    assert(id != event_hub::invalid_request_id);
    assert(bus.process() == 1U);
    assert(bus.process() == 1U);
    assert(output == 18);
}

} // namespace

int main() {
    test_request_callback();
    test_request_future();
    test_request_future_timeout();
    test_request_traits_custom_field();
    test_reply_callback_in_event();
    test_event_node_request_helpers();
    test_module_inherits_request_helpers();
    return 0;
}
