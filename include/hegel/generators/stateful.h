#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
            // GCOVR_EXCL_START
            if (pool_.find(var_id) != pool_.end()) {
                throw std::runtime_error("unexpected variable id in map");
            }
            // GCOVR_EXCL_STOP
            pool_.emplace(var_id, element);
        }

        /**
         * @brief Adds @p element to the pool. Overload for moving rvalues.
         *
         * @param element The element to add to the pool
         */
        void add(T&& element) {
            int64_t var_id = hegel::internal::pool_add(tc_, pool_id_);
            // GCOVR_EXCL_START
            if (pool_.find(var_id) != pool_.end()) {
                throw std::runtime_error("unexpected variable id in map");
            }
            // GCOVR_EXCL_STOP
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

    /**
     * @brief A rule is one possible action in a stateful test.
     *
     * @tparam T The type of the state
     */
    template <typename T> class Rule {
      public:
        /**
         * @brief Declares a new Rule
         *
         * @param name name of the rule
         * @param step function representing the step a rule takes. It mutates
         * the state in place, drawing any arguments it needs from the test
         * case. Always check preconditions with @c assume() before mutating. A
         * rule that rejects after mutating leaves the state partially modified.
         */
        explicit Rule(std::string name, std::function<void(TestCase&, T&)> step)
            : name_(std::move(name)), step_(std::move(step)) {}

        /**
         * @brief Returns the name of the rule.
         *
         * @return const std::string&
         */
        const std::string& name() const { return name_; }
        /**
         * @brief Returns the underlying step function of the rule.
         *
         * @return const std::function<void(TestCase&, T&)>&
         */
        const std::function<void(TestCase&, T&)>& step() const { return step_; }

      private:
        std::string name_;
        std::function<void(TestCase&, T&)> step_;
    };

    /**
     * @brief An invariant is a predicate that must hold at any point in the
     * stateful test. They are evaluated on the initial state and after every
     * valid step. The invariant function should throw when the invariant is
     * violated.
     *
     * @tparam T The type of the state
     */
    template <typename T> class Invariant {
      public:
        /**
         * @brief Declare a new invariant.
         *
         * @param name
         * @param invariant
         */
        explicit Invariant(std::string name,
                           std::function<void(const T&)> invariant)
            : name_(std::move(name)), invariant_(std::move(invariant)) {}

        /**
         * @brief Returns the name of the invariant.
         *
         * @return const std::string&
         */
        const std::string& name() const { return name_; }
        /**
         * @brief Returns the function representing the predicate of the
         * invariant.
         *
         * @return const std::function<void(const T&)>&
         */
        const std::function<void(const T&)>& invariant() const {
            return invariant_;
        }

      private:
        std::string name_;
        std::function<void(const T&)> invariant_;
    };

    /// @cond INTERNAL
    // check if invariants hold on a given state
    template <typename T>
    void check_invariants(TestCase& tc, const std::string& origin,
                          const T& state,
                          const std::vector<Invariant<T>>& invariants) {
        for (const auto& inv : invariants) {
            try {
                (inv.invariant())(state);
            } catch (...) {
                tc.note("Invariant " + inv.name() + " violated " + origin);
                throw;
            }
        }
    }
    /// @endcond

    /**
     * @brief Executes a stateful test by repeatedly applying randomly chosen @p
     * rules to @p state, checking each of the @p invariants before the first
     * step and after every valid step. Rules mutate @p state in place. Raises
     * @p std::invalid_argument if @p rules is empty.
     *
     * @tparam T The type of the state
     * @param tc The test case object
     * @param state The state, initialized by the caller and mutated by the
     * rules
     * @param rules The list of rules the test can apply
     * @param invariants The list of invariants
     */
    template <typename T>
    void run(TestCase& tc, T& state, const std::vector<Rule<T>>& rules,
             const std::vector<Invariant<T>>& invariants) {
        if (rules.empty()) {
            throw std::invalid_argument(
                "Cannot run a state machine with no rules.");
        }
        bool is_single = internal::is_single_test_case(tc);
        int64_t max_steps = is_single ? std::numeric_limits<int64_t>::max()
                                      : internal::stateful_step_count(tc);

        std::vector<std::string> rule_names;
        rule_names.reserve(rules.size());
        for (const Rule<T>& rule : rules)
            rule_names.push_back(rule.name());

        std::vector<std::string> invariant_names;
        invariant_names.reserve(invariants.size());
        for (const Invariant<T>& invariant : invariants)
            invariant_names.push_back(invariant.name());

        check_invariants(tc, "in the initial state", state, invariants);

        auto must_stop = [=](int64_t steps_run) -> std::optional<bool> {
            if (is_single) {
                return false;
            } else if (steps_run >= max_steps) {
                return true;
            } else if (steps_run <= 0) {
                return false;
            } else {
                return std::nullopt;
            }
        };
        int64_t state_machine_id =
            internal::new_state_machine(tc, rule_names, invariant_names);
        int64_t steps_run = 0;
        int64_t num_steps_succeeded = 0;
        double p_stop = std::pow(2.0, -16);

        while (true) {
            internal::start_span(tc, internal::SpanLabel::StatefulRule);
            if (internal::draw_boolean(tc, p_stop, must_stop(steps_run))) {
                if (num_steps_succeeded == 0) {
                    tc.reject();
                }
                break;
            } else {
                steps_run++;
                try {
                    int64_t next_rule_idx =
                        internal::draw_rule(tc, state_machine_id);
                    const Rule<T>& rule = rules[next_rule_idx];
                    tc.note("Step " + std::to_string(steps_run) + ": " +
                            rule.name());

                    rule.step()(tc, state);
                    check_invariants(tc,
                                     "after step " + std::to_string(steps_run),
                                     state, invariants);
                    internal::stop_span(tc);
                    num_steps_succeeded++;
                } catch (const internal::HegelReject&) {
                    tc.note("Rule stopped early due to violated assumption");
                    internal::stop_span(tc, true);
                } catch (...) {
                    internal::stop_span(tc);
                    throw;
                }
            }
        }
    }
    /// @}

} // namespace hegel::stateful
