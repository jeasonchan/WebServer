/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft GPL 2.0
 */ 
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

template<class T>
class BlockDeque
{
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

private:
    std::deque<T> deq_;

    size_t capacity_;

    std::mutex mtx_;

    bool isClose_;

    std::condition_variable condConsumer_;

    std::condition_variable condProducer_;
};

template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    if(MaxCapacity <= 0) exit(-1);
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    {
        std::lock_guard<std::mutex> locker(mtx_);
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

template<class T>
void BlockDeque<T>::push_back(const T &item) {
    {
        std::unique_lock<std::mutex> locker(mtx_);
        while(deq_.size() >= capacity_) {
            condProducer_.wait(locker);
        }
        deq_.push_back(item);
    }
    condConsumer_.notify_all();
}

template<class T>
void BlockDeque<T>::push_front(const T &item) {
    {
        std::unique_lock<std::mutex> locker(mtx_);
        while(deq_.size() >= capacity_) {
            condProducer_.wait(locker);
        }
        deq_.push_front(item);
    }
    condConsumer_.notify_all();
}

template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template<class T>
bool BlockDeque<T>::pop(T &item) {
    {
        std::unique_lock<std::mutex> locker(mtx_);
        while(deq_.empty()){
            condConsumer_.wait(locker);
            if(isClose_ == true){
                return false;
            }
        }
        item = deq_.front();
        deq_.pop_front();
    }
    condProducer_.notify_all();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    {   
        std::unique_lock<std::mutex> locker(mtx_);
        while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            item = nullptr;
            return false;
        }
        if(isClose_ == true){
            return false;
        }
        }
        item = deq_.front();
        deq_.pop_front();
    }
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H