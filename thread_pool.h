#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <queue>
#include <iostream>
#include <mutex>
#include <future>
#include <vector>
#include <functional>
#include <utility>

using namespace std;

template <typename T>
class SafeQueue {
private:
	queue<T> safe_queue;

	mutex queue_mutex;

public:
	SafeQueue() {}
	SafeQueue(SafeQueue&& other) {}
	~SafeQueue() {}

	bool empty() {
		unique_lock<mutex> lock(queue_mutex);

		return safe_queue.empty();
	}

	int size() {
		unique_lock<mutex> lock(queue_mutex);

		return safe_queue.empty();
	}

	void enqueue(T &t) {
		unique_lock<mutex> lock(queue_mutex);

		safe_queue.emplace(t);
	}

	bool dequeue(T &t) {
		unique_lock<mutex> lock(queue_mutex);

		if (safe_queue.empty()) {
			return false;
		}
		t = move(safe_queue.front());
		safe_queue.pop();

		return true;
	}
};

class ThreadPool {
private:
	bool worker_shutdown; // 判断线程池是否关闭
	mutex worker_conditional_mutex; // 线程环境锁，保证从任务队列中取出任务时不会产生冲突
	vector<thread> worker_thread; // 工作线程队列
	SafeQueue<function<void()>> worker_queue; // 任务队列
	condition_variable worker_conditional_lock; // 线程环境锁条件变量，控制线程休眠或唤醒

	// 线程工作类
	class ThreadWorker {
	private:
		int worker_id; // 工作id

		ThreadPool* worker_pool; // 所属线程池

	public:
		// 构造函数
		ThreadWorker(ThreadPool* pool, const int id) : worker_id(id), worker_pool(pool) {

		}

		// 重载()
		void operator()() {
			function<void()> func; // 定义一个void函数类
			
			bool dequeuing; // 判断是否正在取出队列中元素

			// 判断线程池是否关闭，没有关闭则从任务队列中循环提取任务
			while (!worker_pool->worker_shutdown) {
				{
					// 为线程环境加锁
					unique_lock<mutex> lock(worker_pool->worker_conditional_mutex);
 
					// 如果任务队列为空，则阻塞当前线程
					if (worker_pool->worker_queue.empty()) {
						worker_pool->worker_conditional_lock.wait(lock);
					}

					// 取出任务队列中的元素
					dequeuing = worker_pool->worker_queue.dequeue(func);
				}

				// 如果成功取出，则执行任务函数

				if (dequeuing) {
					func();
				}
			}
		}

	};

public:
	// 线程池构造函数
	ThreadPool(const int n_thread = 4) : worker_thread(vector<thread> (n_thread)), worker_shutdown(false) {

	}
	
	~ThreadPool() {
		this->shutdown();
	}

	// 初始化线程池
	void init() {
		for (int i = 0; i < worker_thread.size(); i++) {
			// 分配工作线程
			worker_thread.at(i) = thread(ThreadWorker{this, i});
		}
	}

	void shutdown() {
		worker_shutdown = true;
		// 唤醒所有工作线程
		worker_conditional_lock.notify_all();

		for (int i = 0; i < worker_thread.size(); i++) {
			// 判断线程是否还没执行完
			if (worker_thread.at(i).joinable()) {
				// 将线程加入等待队列
				worker_thread.at(i).join();
			}
		}
	}

	// 将一个函数提交到池后由池异步执行
	template <typename F, typename... Args>
	// auto enqueue(F&& f, Args&&... args) -> future<typename result_of<F(Args...)>::type>
	auto submit(F &&f, Args &&... args) -> future<decltype(f(args...))> {
		// 连接函数和参数定义，特殊函数类型，避免左右值错误
		// using return_type = typename std::result_of<F(Args...)>::type;
		function<decltype(f(args...))()> func = bind(forward<F>(f), forward<Args>(args)...);
		
		// 将其封装成共享指针以便能够复制构造
		auto task_ptr = make_shared<packaged_task<decltype(f(args...))()>>(func);

		// 将任务打包成 void 函数
		// m_queue.enqueue([task_ptr]() {
		//   (*task_ptr)();
		// });
		function<void()> warpper_func = [task_ptr]() {
			(*task_ptr)();
		}; 
		// 队列通用安全封包函数，并压入安全队列
		worker_queue.enqueue(warpper_func);

		// 唤醒一个等待中的线程
		worker_conditional_lock.notify_one();

		// 返回先前注册的任务指针
		return task_ptr->get_future();
	}

};

#endif