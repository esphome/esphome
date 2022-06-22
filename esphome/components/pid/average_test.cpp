#include <iostream>
using namespace std;
#include <deque>

// this is a list of derivative values for smoothing.
std::deque<float> dl;


float weighted_average(std::deque<float> &list, float new_value, int samples) {
    // if only 1 sample needed, clear the list and return
    if (samples == 1) {
        list.clear();
        return new_value;
    }

    // add the new item to the list
    list.push_front(new_value);

    // keep only 'samples' readings, by popping off the back of the list
    while (list.size() > samples)
        list.pop_back();

    // calculate and return the average of all values in the list
    float sum = 0;
    for (auto &elem : list)
        sum += elem;
    return sum / list.size();
}

void test(string str, float actual, float expected) {
    if (actual==expected) {
        cout << "OK: " << str << "\n";
    } else {
        cout << "Fail: " << str << "\n";
    }
}

int main() {

    dl.push_front(1.0);
    dl.push_front(1.0);
    dl.push_front(1.0);
    test("test 1", weighted_average(dl,5,5), 2);

    dl.clear();
    test("test 2", weighted_average(dl,5,5), 5);

    dl.clear();
    dl.push_front(1.0);
    dl.push_front(2.0);
    dl.push_front(3.0);
    dl.push_front(4.0);
    test("test 3", weighted_average(dl,5,5), 3);

    test("test 4", weighted_average(dl,6,5), 4);

    test("test 5", weighted_average(dl,7,3), 6);

}

