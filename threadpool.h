#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <deque>
#include <functional>   //for std::function
#include <atomic>
#include <utility> //for std::forward

using namespace std;


class ThreadPool{
    private:

        mutex m_deque;                                  //mutex for deque of tasks
        condition_variable cv_consumers;                //cv for sync consumer threads
        vector<thread> consumers;                       //consumer threads
        deque<function<void()>> tasks;                  //deque of tasks, i.e. functions
        atomic<bool> stop{false};                       //atomic variable to stop the pool, false by default


    public:

        //ThreadPool Constructor
        ThreadPool(int numThreads) {
            //setup the consumer threads
            //for now no functions in the deque
            
            for (int i = 0; i < numThreads; i++){

                //each thread executes an function having an infinite loop in which the threads waits (with cv) for tasks to be assigned, and executes ("consumes") them
                consumers.emplace_back([this] {

                    while (true){ //I cannot put while(!stop.load()) because there's the risk threads will exit immediately without consuming all remaining tasks in the deque, when stop is set to false

                        function<void()> task;

                        {
                            //wait until a new task to consume is supplied to the thread
                            unique_lock<mutex> lock(m_deque); //cv wants unique_lock !

                            cv_consumers.wait(lock, [this] {
                                return !tasks.empty() || stop.load();
                            });

                            //check if stopping conditions are met
                            //so, stop = true and the tasks deque is empty, if not we have to yet execute the last tasks before stopping the pool!
                            if (stop.load() && tasks.empty()){
                                return;
                            }

                            //or, extract the task from the deque
                            //it's more efficient to use a MAO rather than a CAO, so move the task instead of copying it
                            //FIFO policy
                            task = move(tasks.front());
                            tasks.pop_front();
                        }

                        //finally, execute the task
                        task();
                    }
                });
            }
        }

        //ThreadPool Destructor
        ~ThreadPool(){
            //atomically set stop to true 
            stop.store(true);
            //wake up all consumer threads to make them relize they have to stop asap
            cv_consumers.notify_all();

            //wait for them
            for (thread &t: consumers){
                t.join();
            }

        }


        //function to add a new task to the deque
        //it's a function template and can accept everything, both rvalue and lvalue, without writing separate overload functions
        //in fact, F &&f is a forwarding/universal reference in this context (with templates)
        template<typename F>
        void enqueue(F &&f){
            //add f to the tasks deque
            {
                lock_guard<mutex> lock(m_deque);

                if (stop.load()){
                    //error: user wanted to add a new task to a deque of a stopped pool
                    throw runtime_error("Error! Cannot assign a new task to a stoppe thread pool!");
                }


                //use std::forward to efficiently add the task f to tasks. This is because argument f is ofcourse an lvalue because it's declared,
                //but if the original function, passed to enqueue, was an rvalue, we want to preserve this form and thus, thanks to using emplace and not push,
                //directly construct it into the tasks rather then copying it
                tasks.emplace_back(forward<F>(f));
            }

            //notify one thread that a new task is available
            cv_consumers.notify_one();
        }
     
};

