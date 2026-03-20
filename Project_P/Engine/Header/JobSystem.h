#pragma once

#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <atomic>
#include <vector>

NS_BEGIN(Engine)

class ENGINE_DLL CJobSystem
{
public:
	static CJobSystem& GetInstance()
	{
		static CJobSystem instance;
		return instance;
	}

	void Schedule(std::function<void()> _job)
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			++m_pendingCount;
			m_queue.push(std::move(_job));
		}
		m_cvWork.notify_one();
	}

	void WaitAll()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cvDone.wait(lock, [this] { return m_pendingCount.load() == 0; });
	}

private:
	CJobSystem()
		: m_pendingCount(0)
		, m_shutdown(false)
	{
		const unsigned int threadCount = (std::max)(1u, std::thread::hardware_concurrency() - 1u);
		m_workers.reserve(threadCount);
		for (unsigned int i = 0; i < threadCount; ++i)
			m_workers.emplace_back([this] { WorkerLoop(); });
	}

	~CJobSystem()
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_shutdown = true;
		}
		m_cvWork.notify_all();
		for (auto& t : m_workers)
			t.join();
	}

	void WorkerLoop()
	{
		while (true)
		{
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cvWork.wait(lock, [this] { return m_shutdown || !m_queue.empty(); });
				if (m_shutdown && m_queue.empty())
					return;
				job = std::move(m_queue.front());
				m_queue.pop();
			}

			job();

			{
				std::unique_lock<std::mutex> lock(m_mutex);
				if (--m_pendingCount == 0)
					m_cvDone.notify_all();
			}
		}
	}

	std::vector<std::thread> m_workers;
	std::queue<std::function<void()>> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_cvWork;
	std::condition_variable m_cvDone;
	std::atomic<int> m_pendingCount;
	bool m_shutdown;
};

class ENGINE_DLL CRenderThread
{
public:
	static CRenderThread& GetInstance()
	{
		static CRenderThread instance;
		return instance;
	}

	void Submit(std::function<void()> _renderFn)
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_job = std::move(_renderFn);
			m_hasJob = true;
		}
		m_cv.notify_all();
	}

	void WaitIdle()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cv.wait(lock, [this] { return !m_hasJob && !m_running; });
	}

private:
	CRenderThread()
		: m_hasJob(false)
		, m_running(false)
		, m_shutdown(false)
	{
		m_thread = std::thread([this] { ThreadLoop(); });
	}

	~CRenderThread()
	{
		WaitIdle();
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_shutdown = true;
		}
		m_cv.notify_all();
		m_thread.join();
	}

	void ThreadLoop()
	{
		while (true)
		{
			std::function<void()> job;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_cv.wait(lock, [this] { return m_hasJob || m_shutdown; });
				if (m_shutdown && !m_hasJob)
					return;
				job = std::move(m_job);
				m_hasJob = false;
				m_running = true;
			}

			job();

			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_running = false;
			}
			m_cv.notify_all();
		}
	}

	std::thread m_thread;
	std::function<void()> m_job;
	std::mutex m_mutex;
	std::condition_variable m_cv;
	bool m_hasJob;
	bool m_running;
	bool m_shutdown;
};

NS_END
