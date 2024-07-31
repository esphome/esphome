#pragma once

#include <queue>
#include <unordered_set>
#include <functional>

namespace esphome {
    namespace opentherm {

        template<typename T>
        class unique_queue {
        public:
            bool push(const T &t) {
                if (lookup.insert(t).second) {
                    store.push(t);
                    return true;
                }

                return false;
            }

            bool empty() const {
                return store.empty();
            }

            const T &front() const {
                return store.front();
            }

            T &front() {
                return store.front();
            }

            void pop() {
                lookup.erase(store.front());
                store.pop();
            }

        private:
            std::unordered_set <T> lookup;
            std::queue <T> store;
        };

    }
}