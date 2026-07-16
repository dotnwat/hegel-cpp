#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <utility>

#include "hegel/core.h"

using namespace hegel::generators;

/**
 * @brief Scaffolding for stateful testing for a later release.
 */
namespace hegel::stateful {

    template <typename T> class VariablesGenerator;

    /**
     * @brief A pool of previously generated values. Values added to the pool
     * can be drawn later via @ref values_consumed() / @ref values_reusable().
     *
     * @code{.cpp}
        gs::Pool<int> pool = gs::Pool<int>(tc);
        uint8_t sz = tc.draw(gs::integers<uint8_t>());
        std::set<int> original_set =
            tc.draw(gs::sets(gs::integers<int>(), {.max_size = sz}));

        for (int num : original_set) {
            pool.add(num);
        }
     * @endcode
     *
     * @tparam T The type of variables in the pool.
     */
    template <typename T> class Pool {
      public:
        /**
         * @brief Creates an empty pool. Pools are tied to a test case. Do not
         * reuse one across test cases.
         *
         * @param tc The test case tied to the pool
         */
        explicit Pool(const TestCase& tc) : tc_(tc) {
            pool_id_ = hegel::internal::new_pool(tc);
        }

        /**
         * @brief Adds @p element to the pool. Overload for copying lvalues.
         *
         * @param element The element to add to the pool
         */
        void add(const T& element) {
            int64_t var_id = hegel::internal::pool_add(tc_, pool_id_);
            pool_.emplace(var_id, element);
        }

        /**
         * @brief Adds @p element to the pool. Overload for moving rvalues.
         *
         * @param element The element to add to the pool
         */
        void add(T&& element) {
            int64_t var_id = hegel::internal::pool_add(tc_, pool_id_);
            pool_.emplace(var_id, std::move(element));
        }

        /**
         * @brief Returns the number of variables in the pool.
         *
         * @return The number of variables in the pool
         */
        std::size_t size() const { return pool_.size(); }

        Pool(const Pool&) = delete;

      private:
        const TestCase& tc_;
        int64_t pool_id_;
        std::map<int64_t, T> pool_;

        friend class VariablesGenerator<T>;
    };

    /// @cond INTERNAL
    // Concrete IGenerator for variables.
    template <typename T> class VariablesGenerator : public IGenerator<T> {
      public:
        explicit VariablesGenerator(Pool<T>& p, bool consume)
            : p_(p), consume_(consume) {}

        T do_draw(const TestCase& tc) const override {
            int64_t variable =
                hegel::internal::draw_variable(tc, p_.pool_id_, consume_);

            auto it = p_.pool_.find(variable);
            // GCOVR_EXCL_START
            if (it == p_.pool_.end()) {
                throw std::runtime_error(
                    "Pool state diverged between the engine and the "
                    "client, or a bug in the pool bookkeeping.");
            }
            // GCOVR_EXCL_STOP
            if (consume_) {
                auto val = std::move(it->second);
                p_.pool_.erase(it);
                return val;
            }
            return it->second;
        }

      private:
        Pool<T>& p_;
        bool consume_;
    };
    /// @endcond

    /// @name Variable pools
    /// @{

    /**
     * @brief Returns a value from the pool and removes it.
     *
     * @code{.cpp}
        hegel::test([](hegel::TestCase& tc) {
            gs::Pool<int> pool = gs::Pool<int>(tc);
            std::set<int> original_set =
                tc.draw(gs::sets(gs::integers<int>(), {.max_size = 10}));

            for (int num : original_set) {
                pool.add(num);
            }

            std::set<int> returned_set;
            for (int i = 0; i < original_set.size(); i++) {
                returned_set.insert(tc.draw(gs::values_consumed(pool)));
            }

            assert(original_set == returned_set);
        });
     * @endcode
     *
     * @tparam T Element type
     * @param p The pool to draw from
     * @return An element of the pool
     */
    template <typename T> Generator<T> values_consumed(Pool<T>& p) {
        return Generator<T>(new VariablesGenerator<T>(p, true));
    }

    /**
     * @brief Returns a value from the pool without removing it.
     *
     * @code{.cpp}
        hegel::test([](hegel::TestCase& tc) {
            gs::Pool<int> pool = gs::Pool<int>(tc);
            uint8_t sz = tc.draw(gs::integers<uint8_t>());
            std::set<int> original_set =
                tc.draw(gs::sets(gs::integers<int>(), {.max_size = sz}));

            for (int num : original_set) {
                pool.add(num);
            }

            for (int i = 0; i < original_set.size(); i++) {
                tc.draw(gs::values_reusable(pool));
            }

            assert(pool.size() == original_set.size());
        });
     * @endcode
     *
     * @tparam T Element type
     * @param p The pool to draw from
     * @return An element of the pool
     */
    template <typename T> Generator<T> values_reusable(Pool<T>& p) {
        return Generator<T>(new VariablesGenerator<T>(p, false));
    }

    /// @}

} // namespace hegel::stateful
