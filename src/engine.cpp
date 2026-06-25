#include <engine.h>

#include <hegel/internal.h>
#include <hegel/json.h>
#include <hegel/test_case.h>

#include "json_impl.h"

#include <hegel.h>
#include <protocol.h>
#include <test_case.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace hegel::impl {

    std::string last_error(hegel_context_t* ctx) {
        const char* msg = hegel_context_last_error(ctx);
        return msg ? std::string(msg) : std::string();
    }

} // namespace hegel::impl

namespace hegel::internal {

    // Draw a single value: hand the CBOR schema to libhegel's in-process
    // engine via `hegel_generate` and decode the CBOR value it returns.
    // Returns `{"result": <value>}` so callers (BasicGenerator::do_draw,
    // HegelRandom) can keep reading `response["result"]`.
    hegel::internal::json::json
    generate_from_schema(const hegel::internal::json::json& schema,
                         const hegel::TestCase& tc) {
        auto* data = tc.data();
        hegel_context_t* ctx = data->ctx;
        hegel_test_case_t* htc = data->tc;

        const nlohmann::json& schema_raw = json::ImplUtil::raw(schema);
        std::vector<uint8_t> schema_cbor =
            impl::protocol::cbor_encode(schema_raw);

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "REQUEST: " << schema_raw.dump() << "\n";
        }

        const uint8_t* out_value = nullptr;
        size_t out_len = 0;
        hegel_result_t rc =
            hegel_generate(ctx, htc, schema_cbor.data(), schema_cbor.size(),
                           &out_value, &out_len);

        // Engine ran out of choice budget for this case: abandon the body.
        // The runner marks the case OVERRUN.
        if (rc == HEGEL_E_STOP_TEST) {
            throw HegelStopTest();
        }
        // A precondition (engine-side filter / assume) rejected this draw.
        if (rc == HEGEL_E_ASSUME) {
            throw HegelReject();
        }
        if (rc != HEGEL_OK) {
            throw std::runtime_error("hegel_generate failed: " +
                                     impl::last_error(ctx));
        }

        nlohmann::json value = impl::protocol::cbor_decode(out_value, out_len);

        if (impl::protocol::protocol_debug_enabled()) {
            std::cerr << "RESPONSE: " << value.dump() << "\n";
        }
        // Auto-log generated values during the final replay (counterexample).
        if (data->is_last_run) {
            std::cerr << "Generated: " << value.dump() << "\n";
        }

        nlohmann::json response;
        response["result"] = std::move(value);
        return json::ImplUtil::create(response);
    }

} // namespace hegel::internal
