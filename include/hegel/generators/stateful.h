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
 * @brief Stateful (model-based) property testing.
 *
 * A stateful test exercises a system through a sequence of randomly chosen
 * actions ("rules") applied to a state. Rules are constructed with @ref Rule
 * from a name and a step function that performs one application of the rule,
 * drawing any arguments it needs from the test case and mutating the state.
 * Invariants are predicates on the state evaluated before any step is run and
 * after every successful step. They throw when violated.
 *
 * To run a state machine, call @ref run inside a @ref hegel::test. Examples in
 * this documentation assume the alias `namespace gs = hegel::generators;`.
 *
 * Example: an integer stack.
 *
 * @code{.cpp}
    hegel::test([](hegel::TestCase& tc) {
        auto push = hegel::stateful::Rule<std::vector<int>>(
            "push", [](hegel::TestCase& tc, std::vector<int>& stack) {
                stack.push_back(tc.draw(
                    gs::integers<int>({.min_value = 0, .max_value = 100})));
            });
        auto pop = hegel::stateful::Rule<std::vector<int>>(
            "pop", [](hegel::TestCase& tc, std::vector<int>& stack) {
                tc.assume(!stack.empty());
                stack.pop_back();
            });

        std::vector<int> stack;
        hegel::stateful::run(tc, stack, {push, pop}, {});
    });
 * @endcode
 */
namespace hegel::stateful {

    template <typename T> class VariablesGenerator;

    /**
     * @brief A pool of previously generated values.  A pool lets data flow from
     * one rule to another, so a rule can act on a handle or identifier that an
     * earlier rule produced rather than on a freshly drawn value.
     *
     * Create one with its constructor and populate it with @ref add. To draw
     * from the pool, use the generators @ref values_reusable (returns a value
     * without removing it) and @ref values_consumed (removes and returns a
     * value).
     *
     * A @ref Pool is neither copyable nor movable. A state that embeds a pool
     * is passed to @ref run by reference and never copied.
     *
     * Example: a resource allocator. The @c alloc rule creates a fresh resource
     * and deposits it in the pool. The @c free rule draws one of those resource
     * back out and releases it.
     *
     * @code{.cpp}
        struct State {
            std::set<int> live;
            hegel::stateful::Pool<int> handles;
        };

        auto alloc = hegel::stateful::Rule<State>(
            "alloc", [&next_handle](hegel::TestCase&, State& s) {
                int h = next_handle++;
                s.handles.add(h);
                s.live.insert(h);
            });
        auto free = hegel::stateful::Rule<State>(
            "free", [](hegel::TestCase& tc, State& s) {
                // draws a handle a prior alloc put in the pool
                int h = tc.draw(hegel::stateful::values_consumed(s.handles));
                s.live.erase(h);
            });
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
     * @code{.cpp}
        auto push = hegel::stateful::Rule<std::vector<int>>(
            "push", [](hegel::TestCase& tc, std::vector<int>& stack) {
                stack.push_back(tc.draw(gs::integers<int>()));
            });
     * @endcode
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
     * On a failing replay, each applied rule prints as @c "Step N: <name>". A
     * violated invariant prints @c "Invariant <name> violated after step M" or
     * @c "Invariant <name> violated in the initial state".
     *
     * @code{.txt}
        Step 1: add
        Step 2: add
        Invariant nonneg violated after step 2
     * @endcode
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
            }
            if (steps_run >= max_steps) {
                return true;
            }
            if (steps_run <= 0) {
                return false;
            }
            return std::nullopt;
        };
        int64_t state_machine_id =
            internal::new_state_machine(tc, rule_names, invariant_names);
        int64_t steps_run = 0;
        int64_t num_steps_succeeded = 0;
        double p_stop = std::pow(2.0, -16);

        while (true) {
            internal::start_span(tc, internal::SpanLabel::StatefulRule);
            if (internal::draw_boolean(tc, p_stop, must_stop(steps_run),
                                       /*silent=*/true)) {
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

                    // nest the draws the step makes under its "Step N" header.
                    {
                        internal::NoteIndentScope indent(tc);
                        rule.step()(tc, state);
                    }
                    check_invariants(tc,
                                     "after step " + std::to_string(steps_run),
                                     state, invariants);
                    internal::stop_span(tc);
                    num_steps_succeeded++;
                } catch (const internal::HegelReject&) {
                    tc.note("Rule stopped early due to violated assumption.");
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
