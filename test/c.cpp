#include <stdio.h>
#include <vector>

struct Foo {
    int k;
    void print(){printf("k=%d\n",k);}
};

int main(){
    std::vector<Foo> vec;
    vec.push_back(Foo{.k = 12});
    for (auto& f : vec) f.k = 24;
    vec[0].print();
    return 0;
}