#ifndef EXAMPLE_EXAMPLE_TOOLS_HPP_
#define EXAMPLE_EXAMPLE_TOOLS_HPP_

#include <mutex> // std::mutex
#include <queue> // std::queue
#include <functional> // std::function
#include <utility> // std::pair
#include <thread> // std::thread
#include <memory> // std::shared_ptr
#include <chrono> // std::chrono::milliseconds

namespace extools {

class TaskPool {

private:
	std::queue<std::function<void(void)>> todoQueue;
	std::mutex todoQueueLock;
	volatile size_t threadCount = 0;
	std::mutex threadCountLock;
	size_t maxThreads;

private:

	inline void spawnWorker() {

		std::thread([this]() {
       		// std::cout << "thread function\n";
			auto& queueLock = this->todoQueueLock;
			auto& queue = this->todoQueue;
			
			bool hasFunc;
			std::function<void()> func;
			
			for (;;) {
				queueLock.lock();

				if (!queue.empty()) {
					hasFunc = true;
					func = queue.front();
					queue.pop();
				} else {
					hasFunc = false;
				}

				queueLock.unlock();
				if (!hasFunc) {
					break;
				}

				func();
			}

			this->unregisterWorker();

		}).detach();

	}

	inline bool tryRegisterWorker() {
		threadCountLock.lock();
		bool registered;
		if (threadCount < maxThreads) {
			threadCount = threadCount+1;
			// std::cout << "Register Worker ["<< threadCount << "]" << std::endl;
			registered = true;
		} else {
			registered = false;
		}
		threadCountLock.unlock();
		return registered;
	}

	inline void unregisterWorker() {
		// std::cout << "Unregister Worker" << std::endl;
		threadCountLock.lock();
		threadCount = threadCount-1;
		threadCountLock.unlock();
	}

public:
	inline TaskPool(size_t maxThreads)
		: maxThreads(maxThreads) {}
	
	inline ~TaskPool() {
		sync();
	}

	inline void enqueue(std::function<void(void)> func) {
		todoQueueLock.lock();
		todoQueue.push(func);
		todoQueueLock.unlock();
		if (todoQueue.size() > 4 || threadCount <= 0) {
			if (tryRegisterWorker()) {
				spawnWorker();
			}
		}
	}

	inline void sync() {
		// std::cout << "Sync" << std::endl;
		while (threadCount > 0) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

};

} // namespace extools

#endif // EXAMPLE_EXAMPLE_TOOLS_HPP_
