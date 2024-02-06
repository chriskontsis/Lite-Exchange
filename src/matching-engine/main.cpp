#include <memory>
#include <unordered_map>
#include <list>
#include <iostream>


struct A {
    int v = 5;
};

std::list<std::shared_ptr<A>> lst;

void f(std::shared_ptr<A> a)
{
    lst.push_back(std::move(a));
}

int main()
{
    auto a1 = std::make_shared<A>(); 
    f(a1);
    lst.back()->v = 2;
    std::cout << a1->v << '\n';
    std::cout << lst.back()->v << '\n';
    std::cout << a1.get()->v << '\n';
}