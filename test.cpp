#include <iostream>
#include <vector>

using namespace std;

struct test {
    int lala;
};

void foo (vector<test> &v)
{
    struct test t;
    t.lala = 10;
    v.emplace_back(t);
}

int main()
{
    vector<test> v;
    foo(v);
    
    for (auto t : v) {
        cout << t.lala << endl;
    }

    return 1;
}