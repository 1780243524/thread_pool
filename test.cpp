#include <iostream>
#include <random>
#include "thread_pool.h"

using namespace std;

random_device rd;

mt19937 mt(rd());

uniform_int_distribution<int> dist(-1000, 1000);

auto rnd = bind(dist, mt);

void simulate_hard_computation() {
	this_thread::sleep_for(chrono::milliseconds(2000 + rnd()));
}

void multiply(const int a, const int b) {
	simulate_hard_computation();
	const int res = a * b;
	cout << a << " * " << b << " = " << res << endl;
}

void multiply_output(int& out, const int a, const int b) {
	simulate_hard_computation();
	out = a * b;
	cout << a << " * " << b << " = " << out << endl;
}

int multiply_return(const int a, const int b) {
	simulate_hard_computation();
	const int res = a * b;
	cout << a << " * " << b << " = " << res << endl;
	return res;
}

int sleep_mine() {
	simulate_hard_computation();
	return 1;
}

void example() {
	ThreadPool pool(3);

	pool.init();

	for (int i = 0; i <= 3; i++) {
		for (int j = 1; j <= 10; j++) {
			// pool.submit(multiply, i, j);
			pool.submit([](int a, int b) {
				simulate_hard_computation();
				int res = a * b;
				cout << a << " * " << b << " = " << res << endl;
			}, i, j);
		}
	}

	int output_ref;
	auto future1 = pool.submit(multiply_output, ref(output_ref), 5, 6);

	future1.get();
	cout << "Last operation result is equals to " << output_ref << endl;

	auto future2 = pool.submit(multiply_return, 5, 3);

	int res = future2.get();
	cout << "Last operation result is equals to " << res << endl;

	pool.shutdown();
}

void test() {
	ThreadPool pool(100);

	pool.init();

	auto future1 = pool.submit(sleep_mine);

	cout << "test " << endl;

	int res = future1.get();

	cout << res << endl;
}

int main() {
	example();
	// test();
	return 0;
}