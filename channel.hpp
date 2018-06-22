//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/config.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/config.hpp,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_CONCURRENCY_CHANNEL_HPP
#define ISPTECH_CONCURRENCY_CHANNEL_HPP

#include "isptech/config.hpp"
#include "boost/operators.hpp"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <experimental/coroutine>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>
#include <utility>


/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


/*
    Names/Types
*/
class Scheduler;
using Channel_size = std::size_t;
using std::experimental::suspend_always;
using std::experimental::suspend_never;
using std::tuple;


/*
    Goroutine Launcher
*/
template<class GoFun, class... ArgTypes> void go(GoFun&&, ArgTypes&&...);


/*
    Goroutine
*/
class Goroutine : boost::equality_comparable<Goroutine> {
public:
    // Names/Types
    class Promise;
    using promise_type  = Promise;
    using Handle        = std::experimental::coroutine_handle<Promise>;

    struct Final_suspend {
        // Awaitable Operations
        bool await_ready();
        bool await_suspend(Handle);
        void await_resume();
    };

    class Promise {
    public:
        // Construct/Destroy
        Promise();
        ~Promise();

        // Coroutine Functions
        Goroutine       get_return_object();
        suspend_always  initial_suspend() const;
        Final_suspend   final_suspend() const;
        void            return_void();

        // Completion
        void done();
        bool is_done() const;

    private:
        // Data
        bool isdone;
    };

    // Construct/Move/Copy/Destroy
    explicit Goroutine(Handle = nullptr);
    Goroutine(Goroutine&&);
    Goroutine& operator=(Goroutine&&);
    Goroutine(const Goroutine&) = delete;
    Goroutine& operator=(const Goroutine&) = delete;
    ~Goroutine();

    // Handle Management
    void        reset(Handle);
    void        release();
    bool        is_owner() const;
    Handle      handle() const;
    explicit    operator bool() const;

    // Execution
    void run();

    // Comparisons
    friend bool operator==(const Goroutine&, const Goroutine&);

private:
    // Helpers
    void        steal(Goroutine*);
    static void destroy(Goroutine*);

    // Data
    Handle  coro{nullptr};
    bool    isowner;
};


/*
    Send Channel
*/
template<class T>
class Send_channel : boost::equality_comparable<Send_channel<T>> {
public:
    // Names/Types
    class Awaitable_send;
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;
    using Value         = T;

    // Construct/Move/Copy
    Send_channel(Interface_ptr = Interface_ptr());
    Send_channel& operator=(Send_channel);
    template<class U> friend void swap(Send_channel<U>&, Send_channel<U>&);

    // Buffer Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable_send  send(const T&) const;
    Awaitable_send  send(T&&) const;
    bool            try_send(const T&) const;
    bool            try_send(T&&) const;
    void            sync_send(const T&) const;
    void            sync_send(T&&) const;

    // Conversions
    explicit operator bool() const;

    // Comparisons
    template<class U> friend bool operator==(const Send_channel<U>&, const Send_channel<U>&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Send Channel Interface
*/
template<class T>
class Send_channel<T>::Interface {
public:
    // Destroy
    virtual ~Interface() {}

    // Buffer Size and Capacity
    virtual Channel_size size() const = 0;
    virtual Channel_size capacity() const = 0;

    // Channel Operations
    virtual bool send(T* datap, Goroutine::Handle sender) = 0;
    virtual bool try_send(const T&) = 0;
    virtual bool try_send(T&&) = 0;
    virtual void sync_send(T* datap) = 0;
};


/*
    Awaitable Channel Send
*/
template<class T>
class Send_channel<T>::Awaitable_send {
public:
    // Construct
    Awaitable_send(Interface*, const T&);
    Awaitable_send(Interface*, T&& x);

    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Goroutine::Handle sender);
    void await_resume();

private:
    // Data
    Interface*  channelp;
    T           data;
};


/*
    Receive Channel
*/
template<class T>
class Receive_channel : boost::equality_comparable<Receive_channel<T>> {
public:
    // Names/Types
    class Awaitable_receive;
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;
    using Value         = T;

    // Construct/Move/Copy
    Receive_channel(Interface_ptr = Interface_ptr());
    Receive_channel& operator=(Receive_channel);
    template<class U> friend void swap(Receive_channel<T>&, Receive_channel<T>&);

    // Buffer Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable_receive   receive() const;
    tuple<T,bool>       try_receive() const;
    T                   sync_receive() const;

    // Conversions
    explicit operator bool() const;

    // Comparisons
    template<class U> friend bool operator==(const Receive_channel<U>&, const Receive_channel<U>&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Receive Channel Interface
*/
template<class T>
class Receive_channel<T>::Interface {
public:
    // Destroy
    virtual ~Interface() {}

    // Buffer Size and Capacity
    virtual Channel_size size() const = 0;
    virtual Channel_size capacity() const = 0;

    // Channel Operations
    virtual bool            receive(T* datap, Goroutine::Handle receiver) = 0;
    virtual tuple<T,bool>   try_receive() = 0;
    virtual void            sync_receive(T* datap) = 0;
};


/*
    Awaitable Channel Receive
*/
template<class T>
class Receive_channel<T>::Awaitable_receive {
public:
    // Construct
    explicit Awaitable_receive(Interface*);

    // Awaitable Operations
    bool    await_ready();
    bool    await_suspend(Goroutine::Handle receiver);
    T&&     await_resume();

private:
    // Data
    Interface*  channelp;
    T           data;
};


/*
    Channel
*/
template<class T>
class Channel : boost::equality_comparable<Channel<T>> {
public:
    // Names/Types
    class Interface :
        public Send_channel<T>::Interface,
        public Receive_channel<T>::Interface {
    };
    using Interface_ptr     = std::shared_ptr<Interface>;
    using Awaitable_send    = typename Send_channel<T>::Awaitable_send;
    using Awaitable_receive = typename Receive_channel<T>::Awaitable_receive;
    using Value             = T;

    // Construct/Move/Copy
    Channel(Interface_ptr = Interface_ptr());
    Channel& operator=(Channel);
    template<class U> friend void swap(Channel<U>&, Channel<U>&);

    // Buffer Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable_send      send(const T&) const;
    Awaitable_receive   receive() const;
    bool                try_send(const T&) const;
    bool                try_send(T&&) const;
    tuple<T,bool>       try_receive() const;
    void                sync_send(const T&) const;
    void                sync_send(T&&) const;
    T                   sync_receive() const;

    // Conversions
    explicit operator bool() const;
    operator Send_channel<T>() const;
    operator Receive_channel<T>() const;

    // Comparisons
    template<class U> friend bool operator==(const Channel<U>&, const Channel<U>&);

private:
    // Data
    Interface_ptr ifacep;
};


/*
    Channel Construction
*/
template<class T> Channel<T> make_channel(Channel_size capacity = 0);


namespace Detail {


/*
    Names/Types
*/
using std::condition_variable;


/*
    Channel Waiter

        - A waiting goroutine and the data being sent/received.
*/
template<class T>
class Channel_waiter {
public:
    // Construct
    Channel_waiter(Goroutine::Handle, T* datp);
    Channel_waiter(condition_variable* foreignerp, T* datp);

    // Observers
    Goroutine::Handle   internal() const;
    condition_variable* foreign() const;

    // Waiter Functions
    void resume();

    // Data
    T* datap;

private:
    // Data
    Goroutine::Handle   g;
    condition_variable* foreignp{nullptr};
};


/*
    Channel Waiter Queue
*/
template<class T>
class Channel_waiterqueue {
public:
    // Size and Capacity
    bool is_empty() const;

    // Queue Operations
    void                push(const Channel_waiter<T>&);
    Channel_waiter<T>   pop();
    bool                find(const condition_variable*) const;

private:
    // Names/Types
    struct waiter_equal {
        waiter_equal(const condition_variable*);
        bool operator()(const Channel_waiter<T>&) const;
        const condition_variable* foreignp;
    };

    // Data
    std::deque<Channel_waiter<T>> waiters;
};


/*
    Work Queue
*/
class Workqueue {
public:
    // Queue Operations
    void                    push(Goroutine&&);
    tuple<Goroutine,bool>   pop();
    bool                    try_push(Goroutine&&);
    tuple<Goroutine,bool>   try_pop();
    void                    interrupt();

private:
    // Names/Types
    class Goroutine_queue {
    public:
        void        push(Goroutine&&);
        Goroutine   pop();
        bool        is_empty() const;

    private:
        // Data
        std::deque<Goroutine> elems;
    };

    using Lock = std::unique_lock<std::mutex>;

    // Data
    Goroutine_queue         q;
    bool                    is_interrupt{false};
    std::mutex              mutex;
    std::condition_variable ready;
};


/*
    Work Queue Array
*/
class Workqueue_array {
private:
    // Names/Types
    using Queue_vector = std::vector<Workqueue>;

public:
    // Names/Types
    using Size = Queue_vector::size_type;

    // Construct
    explicit Workqueue_array(Size n);

    // Size
    Size size() const;

    // Queue Operations
    void                    push(Goroutine&&);
    void                    push(Size threadq, Goroutine&&);
    tuple<Goroutine,bool>   pop(Size threadq);
    void                    interrupt();

private:
    // Data
    Queue_vector        queues;
    std::atomic<Size>   nextqueue{0};
};


/*
    Goroutine List
*/
class Goroutine_list {
public:
    // Construct
    void        insert(Goroutine&&);
    Goroutine   release(Goroutine::Handle);

private:
    // Names/Types
    using Goroutine_vector  = std::vector<Goroutine>;
    using Goroutine_ptr     = Goroutine_vector::iterator;
    using Mutex             = std::mutex;
    using Lock              = std::unique_lock<Mutex>;

    struct handle_equal {
        // Construct
        explicit handle_equal(Goroutine::Handle);
        bool operator()(const Goroutine&) const;
        Goroutine::Handle h;
    };

    // Data
    Goroutine_vector    gs;
    Mutex               mutex;
};


}   // Detail


/*
    Goroutine Scheduler
*/
class Scheduler {
public:
    // Construct/Destroy
    Scheduler();
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    ~Scheduler();

    // Execution
    void submit(Goroutine&&);
    void suspend(Goroutine::Handle); // TODO:  should these require Goroutine's?
    void resume(Goroutine::Handle);

private:
    // Names/Types
    using thread = std::thread;

    // Execution
    void run_work(unsigned threadq);

    // Data
    Detail::Workqueue_array workqueues;
    std::vector<thread>     workers;
    Detail::Goroutine_list  suspended;
};


extern Scheduler scheduler;


}   // Concurrency
}   // Isptech


#include "isptech/concurrency/channel.inl"

#endif  // ISPTECH_CONCURRENCY_CHANNEL_HPP

