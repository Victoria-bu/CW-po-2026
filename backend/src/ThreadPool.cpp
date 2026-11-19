#include "ThreadPool.h"

ThreadPool::ThreadPool(int threadCount) {
  for (int i = 0; i < threadCount; i++)
    workers.emplace_back([this] { WorkerLoop(); });
}

ThreadPool::~ThreadPool() {
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    shuttingDown = true;
  }
  taskAvailable.notify_all();  // розбудити всі потоки, щоб вони завершились
  for (auto& worker : workers) worker.join();
}

void ThreadPool::AddTask(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    taskQueue.push(std::move(task));
    tasksInProgress++;
  }
  taskAvailable.notify_one(); 
}

void ThreadPool::WaitUntilDone() {
  std::unique_lock<std::mutex> lock(queueMutex);
  allTasksDone.wait(lock, [this] {
    return tasksInProgress == 0;
  });
}

void ThreadPool::WorkerLoop() {
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(queueMutex);
      taskAvailable.wait(lock, [this]() {
        return shuttingDown || !taskQueue.empty();
      });

      if (shuttingDown && taskQueue.empty()) 
        return;
      task = std::move(taskQueue.front());
      taskQueue.pop();
    }
    task(); 
       bool isAllDone = false;
    {
      std::lock_guard<std::mutex> lock(queueMutex);
      tasksInProgress--;
      isAllDone = (tasksInProgress == 0);
    }
    if (isAllDone) {
      allTasksDone.notify_all();
    }
  }
}