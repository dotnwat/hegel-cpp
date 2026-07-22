#include <hegel/internal.h>
#include <hegel/test_case.h>

#include <engine.h>
#include <test_case.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace hegel::impl::test_case {

    TestCaseData::~TestCaseData() {
        if (tc != nullptr) {
            hegel_test_case_free(thread_context(), tc);
        }
    }

} // namespace hegel::impl::test_case

namespace hegel {

    TestCase::TestCase(std::unique_ptr<impl::test_case::TestCaseData> data)
        : data_(std::move(data)) {}

    TestCase::TestCase(TestCase&&) noexcept = default;
    TestCase& TestCase::operator=(TestCase&&) noexcept = default;
    TestCase::~TestCase() = default;

    TestCase TestCase::clone() const {
        hegel_test_case_t* handle =
            impl::test_case_clone(impl::thread_context(), data_->tc);
        auto cloned = std::unique_ptr<impl::test_case::TestCaseData>(
            new impl::test_case::TestCaseData{
                handle, data_->is_final, data_->verbosity,
                data_->stateful_step_count, data_->mode, data_->note_indent});
        return TestCase(std::move(cloned));
    }

    void TestCase::assume(bool condition) const {
        if (!condition) {
            throw internal::HegelReject();
        }
    }

    void TestCase::reject() const { throw internal::HegelReject(); }

    void TestCase::target(double score, std::string_view label) const {
        impl::target(*this, score, std::string(label).c_str());
    }

    void TestCase::repeat(const std::function<void()>& body) const {
        // Seed the loop's minimum length, then let the engine decide each
        // iteration: the count is drawn like any collection, so it shrinks.
        int64_t min_size = internal::draw_integer(*this, 0, int64_t{1} << 20);
        int64_t collection = internal::new_collection(
            *this, static_cast<uint64_t>(min_size), internal::no_max_size);
        uint64_t iteration = 0;
        while (internal::collection_more(*this, collection)) {
            note("// Repetition #" + std::to_string(++iteration));
            try {
                body();
            } catch (const internal::HegelReject&) {
                // Iteration rejected; discard it and keep looping.
                continue;
            }
        }
    }

    void TestCase::note(std::string_view message) const {
        if (data_->should_log()) {
            std::cerr << data_->indent_prefix() << message << std::endl;
        }
    }

    namespace internal {

        NoteIndentScope::NoteIndentScope(const TestCase& tc) : tc_(tc) {
            tc_.data()->note_indent++;
        }

        NoteIndentScope::~NoteIndentScope() { tc_.data()->note_indent--; }

    } // namespace internal

} // namespace hegel
