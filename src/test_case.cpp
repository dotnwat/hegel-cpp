#include <hegel/internal.h>
#include <hegel/test_case.h>

#include <engine.h>
#include <test_case.h>

#include <iostream>
#include <string>
#include <string_view>

namespace hegel {

    void TestCase::assume(bool condition) const {
        if (!condition) {
            throw internal::HegelReject();
        }
    }

    void TestCase::reject() const { throw internal::HegelReject(); }

    void TestCase::target(double score, std::string_view label) const {
        impl::target(*this, score, std::string(label).c_str());
    }

    void TestCase::note(std::string_view message) const {
        if (data_->should_log()) {
            std::cerr << message << std::endl;
        }
    }

} // namespace hegel
