#pragma once
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

// Потоки створюються один раз і чекають задачі в черзі
class ThreadPool {
 public:
  explicit ThreadPool(int threadCount);
  ~ThreadPool();

  void AddTask(std::function<void()> task); 
  void WaitUntilDone(); 

 private:
  void WorkerLoop(); //що робить кожен потік

  std::vector<std::thread> workers;
  std::queue<std::function<void()>> taskQueue;
  std::mutex queueMutex; //захищає чергу й лічильник
  std::condition_variable taskAvailable; 
  std::condition_variable allTasksDone;   
  int tasksInProgress = 0;              
  bool shuttingDown = false;              
};