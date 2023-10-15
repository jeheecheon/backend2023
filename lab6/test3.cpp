#include <iostream>
#include <chrono>
#include <thread>

using namespace std;

void f1()
{
    cout << "f1: " << this_thread::get_id() << endl;
    this_thread::sleep_for(chrono::milliseconds(10 * 1000));
    cout << "f1: woke up" << endl;
}

void f2(int arg)
{
    cout << "f2: " << arg << endl;
}

int main()
{
    thread t1;
    thread t2(f1);
    thread t3(f2, 10);
    cout << "C++ id: " << t3.get_id() << endl;
    cout << "Native id: " << t3.native_handle() << endl;

    t2.join();
    t3.join();

    return 0;
}
