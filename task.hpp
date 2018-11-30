//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/task.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/task.hpp,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_CONCURRENCY_TASK_HPP
#define ISPTECH_CONCURRENCY_TASK_HPP

#include "isptech/config.hpp"
#include "boost/operators.hpp"
#include "boost/optional.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <exception>
#include <experimental/coroutine>
#include <iterator>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
#define NOMINMAX
#include <windows.h>


/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


/*
    Names/Types
*/
template<class T> class Channel;
template<class T> class Send_channel;
template<class T> class Receive_channel;
template<class T> class Future;
class Channel_base;
class Channel_operation;
using Channel_size = std::ptrdiff_t;
using Time_point = std::chrono::steady_clock::time_point;
using boost::optional;
using std::exception_ptr;
using std::chrono::nanoseconds;
#pragma warning(disable: 4455)
using std::chrono::operator""ns;
using std::vector;


/*
    Task

    A Task is a lightweight cooperative thread implemented by a stackless
    coroutine.  Tasks are multiplexed onto operating system threads by a
    Scheduler.  A Task can suspend its execution without blocking the thread
    on which it was invoked, thus freeing the Scheduler to re-allocate the
    thread to another ready Task.
*/
class Task : boost::equality_comparable<Task> {
public:
    // Names/Types
    class Promise;
    using promise_type      = Promise;
    using Handle            = std::experimental::coroutine_handle<promise_type>;
    using Initial_suspend   = std::experimental::suspend_always;
    using Final_suspend     = std::experimental::suspend_always;

    enum class Status { ready, suspended, done };

    class Select_status {
    public:
        // Construct
        Select_status() = default;
        Select_status(Channel_size pos, bool comp);

        // Observers
        Channel_size    position() const;
        bool            is_complete() const;

    private:
        // Data
        Channel_size    selpos;
        bool            iscomp;
    };

    // Construct/Move/Destroy
    explicit Task(Handle = nullptr);
    Task(Task&&);
    Task& operator=(Task&&);
    friend void swap(Task&, Task&);
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task();

    // Observers
    Handle handle() const;

    // Execution
    Status resume();

    // Synchronization
    void unlock();

    // Comparisons
    friend bool operator==(const Task&, const Task&);

    // Friends
    friend class Channel_operation; // for Channel_lock :(
    template<class T> friend class Future;

private:
    // Names/Types
    using Mutex = std::mutex;
    using Lock  = std::unique_lock<Mutex>;

    class Channel_lock {
    public:
        // Construct/Copy/Destory
        explicit Channel_lock(Channel_base*);
        Channel_lock(const Channel_lock&) = delete;
        Channel_lock& operator=(const Channel_lock&) = delete;
        ~Channel_lock();

    private:
        // Data
        Channel_base* chanp;
    };

    class Channel_selection {
    public:
        // Construct/Copy 
        Channel_selection() = default;
        Channel_selection(const Channel_selection&) = delete;
        Channel_selection& operator=(const Channel_selection&) = delete;

        // Selection Operations
        bool                            select(Task::Handle, Channel_operation*, Channel_operation*);
        Select_status                   select(Task::Handle, Channel_size pos);
        Channel_size                    selected() const;
        static optional<Channel_size>   try_select(Channel_operation*, Channel_operation*);

    private:
        // Names/Types
        class Lock_guard {
        public:
            Lock_guard(Channel_operation*, Channel_operation*);
            Lock_guard(const Lock_guard&) = delete;
            Lock_guard& operator=(const Lock_guard&) = delete;
            ~Lock_guard();
        
        private:
            // Data
            Channel_operation* first;
            Channel_operation* last;
        };

        class Sort_guard {
        public:
            Sort_guard(Channel_operation*, Channel_operation*);
            Sort_guard(const Sort_guard&) = delete;
            Sort_guard& operator=(const Sort_guard&) = delete;
            ~Sort_guard();
        
        private:
            // Data
            Channel_operation* first;
            Channel_operation* last;
        };

        // Selection
        static optional<Channel_size>   select_ready(Channel_operation*, Channel_operation*);
        static Channel_size             count_ready(const Channel_operation*, const Channel_operation*);
        static Channel_operation*       pick_ready(Channel_operation*, Channel_operation*, Channel_size nready);
        static Channel_size             enqueue(Task::Handle, Channel_operation*, Channel_operation*);
        static Channel_size             dequeue(Task::Handle, Channel_operation*, Channel_operation*, Channel_size selected);

        // Sorting
        static void save_positions(Channel_operation*, Channel_operation*);
        static void sort_channels(Channel_operation*, Channel_operation*);
        static void restore_positions(Channel_operation*, Channel_operation*);

        // Data
        Channel_operation*      begin;
        Channel_operation*      end;
        Channel_size            nenqueued;
        optional<Channel_size>  winner;
    };

    class Future_selection {
    public:
        // Construct/Copy
        Future_selection() = default;
        Future_selection(const Future_selection&) = delete;
        Future_selection& operator=(const Future_selection&) = delete;

        // Selection Operations
        template<class T> bool  wait_any(Task::Handle, const Future<T>*, const Future<T>*, const optional<nanoseconds>&);
        template<class T> bool  wait_all(Task::Handle, const Future<T>*, const Future<T>*, const optional<nanoseconds>&);
        bool                    complete_timer(Task::Handle, Time_point);
        bool                    cancel_timer();
        Select_status           select_channel(Task::Handle, Channel_size pos);
        Channel_size            selected() const;

        // Friends
        template<class T> friend class Future;

    private:
        // Names/Types
        enum class Type { any, all };
        static const Channel_size wait_success{1};

        class Channel_wait {
        public:
            // Construct
            Channel_wait() = default;
            Channel_wait(Channel_base*, Channel_size fpos);

            // Enqueue/Dequeue
            void enqueue(Task::Handle, Channel_size pos) const;
            bool dequeue(Task::Handle, Channel_size pos) const;
            void dequeue_locked(Task::Handle, Channel_size pos) const;

            // Observers
            bool            is_ready() const;
            Channel_base*   channel() const;
            Channel_size    future() const;

            // Synchronization
            void lock_channel() const;
            void unlock_channel() const;

        private:
            // Data
            Channel_base*   chanp;
            Channel_size    futpos;
        };

        using Channel_wait_vector = std::vector<Channel_wait>;

        class Future_wait {
        public:
            // Construct
            Future_wait() = default;
            Future_wait(bool* readyp, Channel_size vpos, Channel_size epos);

            // Channels
            Channel_size value() const;
            Channel_size error() const;

            // Enqueue/Dequeue
            void enqueue(Task::Handle, const Channel_wait_vector&) const;
            bool dequeue(Task::Handle, const Channel_wait_vector&) const;
            void dequeue_locked(Task::Handle, const Channel_wait_vector&) const;

            // Completion
            void complete(Task::Handle, const Channel_wait_vector&, Channel_size pos) const;
            bool is_ready(const Channel_wait_vector&) const;

        private:
            // Data
            Channel_size    vchan;
            Channel_size    echan;
            bool*           isreadyp;
        };

        using Future_wait_vector = std::vector<Future_wait>;

        class Channel_locks {
        public:
            // Construct/Copy/Destroy
            explicit Channel_locks(Channel_wait_vector*);
            Channel_locks(const Channel_locks&) = delete;
            Channel_locks& operator=(const Channel_locks&) = delete;
            ~Channel_locks();

        private:
            // Data
            Channel_wait_vector* waitsp;
        };

        class Channel_sort {
        public:
            // Construct
            explicit Channel_sort(Channel_wait_vector*);
            Channel_sort(const Channel_sort&) = delete;
            Channel_sort& operator=(const Channel_sort&) = delete;

        private:
            // Data
            Channel_wait_vector* waitsp;
        };

        class Future_transform {
        public:
            // Construct
            template<class T> Future_transform(const Future<T>*, const Future<T>*, Future_wait_vector*, Channel_wait_vector*);
        };

        class Transform_and_lock {
        public:
            // Construct
            template<class T> Transform_and_lock(const Future<T>*, const Future<T>*, Future_wait_vector*, Channel_wait_vector*);

        private:
            // Data
            Future_transform    transform;
            Channel_sort        sort;
            Channel_locks       lock;
        };

        class Timer {
        public:
            // Construct
            Timer();
            Timer(const Timer&) = delete;
            Timer& operator=(const Timer&) = delete;

            // Execution
            void start(Task::Handle, nanoseconds duration) const;
            void complete(Time_point) const;
            void cancel(Task::Handle) const;
            void complete_cancel() const;
            void clear() const;
            bool is_active() const;
            bool is_completed() const;
            bool is_cancelled() const;

        private:
            // Constants
            enum class State { inactive, running, cancel_pending, cancel_complete, complete };

            // Data
            mutable State state;
        };

        // Selection
        static Channel_size             complete(Task::Handle, const Future_wait_vector&, const Channel_wait_vector&, Channel_size chanpos);
        static Channel_size             count_ready(const Future_wait_vector&, const Channel_wait_vector&);
        static void                     dequeue_all(Task::Handle, const Future_wait_vector&, const Channel_wait_vector&);
        static Channel_size             dequeue_not_ready(Task::Handle, const Future_wait_vector&, const Channel_wait_vector&);
        static void                     enqueue_all(Task::Handle, const Future_wait_vector&, const Channel_wait_vector&);
        static Channel_size             enqueue_not_ready(Task::Handle, const Future_wait_vector&, const Channel_wait_vector&);
        static optional<Channel_size>   pick_ready(const Future_wait_vector&, const Channel_wait_vector&, Channel_size nready);
        static optional<Channel_size>   select_ready(const Future_wait_vector&, const Channel_wait_vector&);
        static void                     sort_channels(Channel_wait_vector*);

        // Data
        Type                    type;
        Future_wait_vector      futures;
        Channel_wait_vector     channels;
        Channel_size            nenqueued;
        optional<Channel_size>  result;
        Timer                   timer;
    };

    // Random Number Generation
    static Channel_size random(Channel_size min, Channel_size max);

public:
    // Names/Types
    class Promise {
    public: 
        // Construct/Copy
        Promise();
        Promise(const Promise&) = delete;
        Promise& operator=(const Promise&) = delete;

        // Coroutine Functions
        Task            get_return_object();
        Initial_suspend initial_suspend() const;
        Final_suspend   final_suspend();
    
        // Channel Operation Selection
        template<Channel_size N> void   select(Channel_operation (&ops)[N]);
        void                            select(Channel_operation*, Channel_operation*);
        Select_status                   select_operation(Channel_size);
        Channel_size                    selected_operation() const;
        static optional<Channel_size>   try_select(Channel_operation*, Channel_operation*);

        // Channel Event Selection
        Select_status select_readable(Channel_size pos);

        // Future Selection
        template<class T> void  wait_all(const Future<T>*, const Future<T>*, const optional<nanoseconds>&);
        template<class T> void  wait_any(const Future<T>*, const Future<T>*, const optional<nanoseconds>&);
        Channel_size            selected_future() const;
        bool                    complete_timer(Time_point);
        bool                    cancel_timer();

        // Execution
        void    make_ready();
        Status  status() const;

        // Synchronization
        void unlock();

    private:
        // Execution
        void suspend(Lock*);

        // Data
        mutable Mutex       mutex;
        Channel_selection   channels;
        Future_selection    futures;
        Status              taskstat;
    };

private:
    // Data
    Handle coro;
};


/*
    Task Launch
*/
template<class TaskFun, class... Args> void                                 start(TaskFun, Args&&...);
template<class Fun, class... Args> Future<std::result_of_t<Fun(Args&&...)>> async(Fun, Args&&...);
//template<class Fun> Future<std::result_of_t<Fun()>>                         async(Fun);


/*
    Channel Base
*/
class Channel_base {
public:
    // Copy/Move/Destory
    Channel_base() = default;
    Channel_base(const Channel_base&) = delete;
    Channel_base& operator=(const Channel_base&) = delete;
    virtual ~Channel_base() = default;

    // Non-Blocking Send/Receive
    virtual bool is_writable() const = 0;
    virtual void write(void* valuep) = 0;
    virtual void write(const void* valuep) = 0;
    virtual bool is_readable() const = 0;
    virtual void read(void* valuep) = 0;

    // Blocking Send/Receive
    virtual void enqueue_write(Task::Handle, Channel_size pos, void* valuep) = 0;
    virtual void enqueue_write(Task::Handle, Channel_size pos, const void* valuep) = 0;
    virtual bool dequeue_write(Task::Handle, Channel_size pos) = 0;
    virtual void enqueue_read(Task::Handle, Channel_size pos, void* valuep) = 0;
    virtual bool dequeue_read(Task::Handle, Channel_size pos) = 0;

    // Waiting
    virtual void enqueue_readable_wait(Task::Handle, Channel_size pos) = 0;
    virtual bool dequeue_readable_wait(Task::Handle, Channel_size pos) = 0;

    // Synchronization
    virtual void lock() = 0;
    virtual void unlock() = 0;
};


/*
    Channel Operations
*/
template<class T> Channel<T> make_channel(Channel_size capacity=0);


/*
    Channel
*/
template<class T>
class Channel : boost::totally_ordered<Channel<T>> {
public:
    // Names/Types
    class Send_awaitable;
    class Receive_awaitable;
    using Value = T;

    // Construct/Move
    Channel() = default;
    Channel(Channel&&);
    Channel& operator=(Channel&&);
    friend Channel make_channel<T>(Channel_size capacity);
    inline friend void swap(Channel& x, Channel& y) { swap(x.pimpl, y.pimpl;); }

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;

    // Non-Blocking Send/Receive
    Send_awaitable      send(const T&) const;
    Send_awaitable      send(T&&) const;
    Receive_awaitable   receive() const;
    bool                try_send(const T&) const;
    optional<T>         try_receive() const;

    // Blocking Send/Receive (move definitions outside class body in VS 2017)
    inline friend void  blocking_send(const Channel& c, const T& x) { c.pimpl->blocking_send(&x); }
    inline friend void  blocking_send(const Channel& c, T&& x)      { c.pimpl->blocking_send(&x); }
    inline friend T     blocking_receive(const Channel& c)          { return c.pimpl->blocking_receive(); }

    // Operation Selection
    Channel_operation make_send(const T&) const;
    Channel_operation make_send(T&&) const;
    Channel_operation make_receive(T*) const;
    
    // Conversions
    explicit operator bool() const;

    // Comparisons
    inline friend bool operator==(const Channel& x, const Channel& y) { return x.pimpl == y.pimpl; }
    inline friend bool operator< (const Channel& x, const Channel& y) { return x.pimpl < y.pimpl; }

    // Friends
    friend class Send_channel<T>;
    friend class Receive_channel<T>;

private:
    // Names/Types
    using Mutex     = std::mutex;
    using Lock      = std::unique_lock<Mutex>;
    using Condition = std::condition_variable;

    class Readable_wait : boost::equality_comparable<Readable_wait> {
    public:
        // Construct
        Readable_wait() = default;
        Readable_wait(Task::Handle, Channel_size pos);

        // Identity
        Task::Handle task() const;
        Channel_size position() const;

        // Selection
        void notify(Mutex*) const;

        // Comparisons
        friend bool operator==(const Readable_wait& x, const Readable_wait& y) {
            if (x.task() != y.task()) return false;
            if (x.position() != y.position()) return false;
            return true;
        }

    private:
        // Selection
        static void select(Task::Handle task, Channel_size chan);

        // Data
        mutable Task::Handle    taskh;
        Channel_size            waitpos;
    };

    class Buffer {
    public:
        // Construct
        explicit Buffer(Channel_size maxsize);

        // Size and Capacity
        Channel_size    size() const;
        Channel_size    max_size() const;
        bool            is_empty() const;
        bool            is_full() const;

        // Queue Operations
        template<class U> bool push(U&&, Mutex*);
        template<class U> bool push_silent(U&&);
        template<class U> bool pop(U*);

        // Waiters
        void enqueue(const Readable_wait&);
        bool dequeue(const Readable_wait&);

    private:
        // Data
        std::queue<T>               q;
        Channel_size                sizemax;
        std::deque<Readable_wait>   readers;
    };

    class Sender : boost::equality_comparable<Sender> {
    public:
        // Construct
        Sender(Task::Handle, Channel_size pos, const T* rvaluep);
        Sender(Task::Handle, Channel_size pos, T* lvaluep);
        Sender(Condition* waitp, const T* rvaluep);
        Sender(Condition* waitp, T* lvaluep);
    
        // Observers
        Task::Handle task() const;
        Channel_size operation() const;

        // Completion
        template<class U> bool dequeue(U* recvbufp, Mutex*) const;

        // Comparisons
        inline friend bool operator==(const Sender& x, const Sender& y) {
            if (x.condp != y.condp) return false;
            if (x.rvalp != y.rvalp) return false;
            if (x.lvalp != y.lvalp) return false;
            if (x.taskh != y.taskh) return false;
            if (x.oper != y.oper) return false;
            return true;
        }
    
    private:
        // Selection
        template<class U> static bool select(Task::Handle, Channel_size pos, T* lvalp, const T* rvalp, U* recvbufp);

        // Data Transefer
        template<class U> static void   move(T* lvalp, const T* rvalp, U* destp);
        static void                     move(T* lvalp, const T* rvalp, Buffer* destp);
    
        // Data
        mutable Task::Handle    taskh; // poor Handle const usage forces mutable
        Channel_size            oper;
        const T*                rvalp;
        T*                      lvalp;
        Condition*              readyp;
    };
    
    class Receiver : boost::equality_comparable<Receiver> {
    public:
        // Construct
        Receiver(Task::Handle, Channel_size pos, T* valuep);
        Receiver(Condition* waitp, T* valuep);

        // Observers
        Task::Handle task() const;
        Channel_size operation() const;
    
        // Selection
        template<class U> bool dequeue(U* sendbufp, Mutex*) const;
    
        // Comparisons
        inline friend bool operator==(const Receiver& x, const Receiver& y) {
            if (x.readyp != y.readyp) return false;
            if (x.valp != y.valp) return false;
            if (x.taskh != y.taskh) return false;
            if (x.oper != y.oper) return false;
            return true;
        }
    
    private:
        // Selection
        template<class U> static bool select(Task::Handle, Channel_size pos, T* valp, U* sendbufp);

        // Data
        mutable Task::Handle    taskh; // poor Handle const usage forces mutable
        Channel_size            oper;
        T*                      valp;
        Condition*              readyp;
    };

    template<class U> 
    class Io_queue {
    private:
        // Names/Types
        using Waiter_deque = std::deque<U>;

        struct waiter_eq {
            waiter_eq(Task::Handle h, Channel_size pos) : task{h}, oper{pos} {}
            bool operator()(const U& waiter) const {
                return waiter.task() == task && waiter.operation() == oper;
            }
            Task::Handle task;
            Channel_size oper;
        };

        // Data
        Waiter_deque waiters;

    public:
        // Names/Types
        using Waiter    = U;
        using Iterator  = typename Waiter_deque::iterator;

        // Iterators
        Iterator end();

        // Size and Capacity
        bool is_empty() const;

        // Queue Operations
        void        push(const Waiter&);
        Waiter      pop();
        void        erase(Iterator);
        Iterator    find(Task::Handle, Channel_size pos);
        bool        is_found(const Waiter&) const;
    };

    // Names/Types
    using Sender_queue      = Io_queue<Sender>;
    using Receiver_queue    = Io_queue<Receiver>;

    class Impl final : public Channel_base {
    public:
        // Construct
        explicit Impl(Channel_size);

        // Size and Capacity
        Channel_size    size() const;
        Channel_size    capacity() const;
        bool            is_empty() const;

        // Send/Receive
        template<class U> Send_awaitable    awaitable_send(U* valuep);
        Receive_awaitable                   awaitable_receive();
        bool                                try_send(const T&);
        optional<T>                         try_receive();
        template<class U> void              blocking_send(U* valuep);
        T                                   blocking_receive();

        // Operation Construction
        Channel_operation make_send(const T* valuep);
        Channel_operation make_send(T* valuep);
        Channel_operation make_receive(T* valuep);

        // I/O Readiness
        bool is_readable() const override;
        bool is_writable() const override;

        // Non-Blocking I/O
        void read(void* valuep) override;
        void write(const void* rvaluep) override;
        void write(void* lvaluep) override;

        // Blocking I/O
        void enqueue_read(Task::Handle, Channel_size pos, void* valuep) override;
        bool dequeue_read(Task::Handle, Channel_size pos) override;
        void enqueue_write(Task::Handle, Channel_size pos, const void* rvaluep) override;
        void enqueue_write(Task::Handle, Channel_size pos, void* lvaluep) override;
        bool dequeue_write(Task::Handle, Channel_size pos) override;

        // Waiting
        void enqueue_readable_wait(Task::Handle, Channel_size pos) override;
        bool dequeue_readable_wait(Task::Handle, Channel_size pos) override;

        // Synchronization
        void lock() override;
        void unlock() override;

    private:
        // Non-Blocking I/O
        template<class U> bool read_element(U* valuep, Buffer*, Sender_queue*, Mutex*);
        template<class U> bool write_element(U* valp, Buffer*, Receiver_queue*, Mutex*);

        // Blocking I/O
        void                            enqueue_read(Task::Handle, Channel_size pos, T* valuep);
        template<class U> void          enqueue_write(Task::Handle, Channel_size pos, U* valuep);
        template<class U> static bool   dequeue(Receiver_queue*, U* sendbufp, Mutex*);
        template<class U> static bool   dequeue(Sender_queue*, U* recvbufp, Mutex*);
        template<class U> static bool   dequeue(U* waitqp, Task::Handle, Channel_size pos);
        static void                     wait_for_sender(Receiver_queue*, T* recvbufp, Lock*);
        template<class U> static void   wait_for_receiver(Sender_queue*, U* sendbufp, Lock*);

        // Data
        Buffer          buffer;
        Sender_queue    senders;
        Receiver_queue  receivers;
        mutable Mutex   mutex;
    };

    using Impl_ptr = std::shared_ptr<Impl>;

    // Construct
    Channel(Impl_ptr);

    // Data
    Impl_ptr pimpl;
};


/*
    Channel Send Awaitable
*/
template<class T>
class Channel<T>::Send_awaitable {
public:
    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Task::Handle);
    void await_resume();

private:
    // Friends
    friend class Impl;

    // Construct
    template<class U> Send_awaitable(Impl*, U* valuep);

    // Data
    Channel_operation send[1];
};


/*
    Channel Receive Awaitable
*/
template<class T>
class Channel<T>::Receive_awaitable {
public:
    // Awaitable Operations
    bool    await_ready();
    bool    await_suspend(Task::Handle);
    T&&     await_resume();

private:
    // Friends
    friend class Impl;

    // Construct
    explicit Receive_awaitable(Impl*);

    // Data
    T                   value;
    Channel_operation   receive[1];
};


/*
    Send Channel
*/
template<class T>
class Send_channel : boost::totally_ordered<Send_channel<T>> {
public:
    // Names/Types
    using Value     = typename Channel<T>::Value;
    using Awaitable = typename Channel<T>::Send_awaitable;

    // Construct/Move/Copy
    Send_channel();
    Send_channel& operator=(Send_channel);
    inline friend void swap(Send_channel& x, Send_channel& y) { swap(x.pimpl, y.pimpl); }

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;

    // Non-Blocking Channel Operations
    Awaitable   send(const T&) const;
    Awaitable   send(T&&) const;
    bool        try_send(const T&) const;

    // Blocking Channel Operations (can move outside class body in VS 2017)
    inline friend void blocking_send(const Send_channel& c, const T& x) { c.pimpl->blocking_send(&x); }
    inline friend void blocking_send(const Send_channel& c, T&& x)      { c.pimpl->blocking_send(&x); }

    // Selectable Operations
    Channel_operation make_send(const T&) const;
    Channel_operation make_send(T&&) const;

    // Conversions
    Send_channel(const Channel<T>&);
    Send_channel& operator=(const Channel<T>&);
    explicit operator bool() const;

    // Comparisons
    inline friend bool operator==(const Send_channel& x, const Send_channel& y) { return x.pimpl == y.pimpl; }
    inline friend bool operator< (const Send_channel& x, const Send_channel& y) { return x.pimpl < y.pimpl; }

private:
    // Data
    typename Channel<T>::Impl_ptr pimpl;
};


/*
    Receive Channel
*/
template<class T>
class Receive_channel : boost::totally_ordered<Receive_channel<T>> {
public:
    // Names/Types
    using Value     = typename Channel<T>::Value;
    using Awaitable = typename Channel<T>::Receive_awaitable;

    // Construct/Move/Copy
    Receive_channel();
    Receive_channel& operator=(Receive_channel);
    inline friend void swap(Receive_channel& x, Receive_channel& y) { swap(x.pimpl, y.pimpl);  }

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;

    // Non-Blocking Channel Operations
    Awaitable   receive() const;
    optional<T> try_receive() const;

    // Blocking Channel Operations
    inline friend T blocking_receive(const Receive_channel& c) { return c.pimpl->blocking_receive(); }

    // Selection
    Channel_operation make_receive(T*);

    // Conversions
    Receive_channel(const Channel<T>&);
    Receive_channel& operator=(const Channel<T>&);
    explicit operator bool() const;

    // Comparisons
    inline friend bool operator==(const Receive_channel& x, const Receive_channel& y) { return x.pimpl == y.pimpl; }
    inline friend bool operator< (const Receive_channel& x, const Receive_channel& y) { return x.pimpl < y.pimpl; }

    // Friends
    template <class T> friend class Future;

private:
    // Data
    typename Channel<T>::Impl_ptr pimpl;
};


/*
    Channel Operation

    Users can call select() or try_select() to choose an operation for
    execution from a set of alternatives.  If more than one alternative
    is ready, a choice is made at random.  Selecting from a set of channels
    with disparate element types requires an operation interface that is
    independent element types, so perhaps the ideal design would involve
    channels which construct type-safe operation implementations using
    inheritance to hide the element type.  But that would imply dynamic
    memory allocation, so we temporarily avoid that approach in favor of
    type-unsafe interfaces with which users needn't directly interact.
    Perhaps the small object optimization could be employed in the future
    to realize a better design.
*/
class Channel_operation {
public:
    // Names/Types
    enum class Type { none, send, receive };

    // Construct/Move/Copy/Destroy
    Channel_operation();
    Channel_operation(Channel_base*, const void* rvaluep); // send copy
    Channel_operation(Channel_base*, void* lvaluep, Type); // send movable/receive

    // Channel
    Channel_base*       channel();
    const Channel_base* channel() const;

    // Selection
    bool is_ready() const;
    void execute();
    void enqueue(Task::Handle);
    bool dequeue(Task::Handle);

    // Position
    void            position(Channel_size);
    Channel_size    position() const;

private:
    // Data
    Channel_base*   chanp;
    Type            kind;
    const void*     rvalp;
    void*           lvalp;
    Channel_size    pos{0};
};


/*
    Channel Select Awaitable
*/
class Channel_select_awaitable {
public:
    // Construct
    Channel_select_awaitable(Channel_operation*, Channel_operation*);

    // Awaitable Operations
    bool            await_ready();
    bool            await_suspend(Task::Handle);
    Channel_size    await_resume();

private:
    // Data
    Task::Handle        task;
    Channel_operation*  first;
    Channel_operation*  last;
};


/*
    Channel Selection
*/
template<Channel_size N> Channel_select_awaitable   select(Channel_operation (&ops)[N]);
Channel_select_awaitable                            select(Channel_operation*, Channel_operation*);
template<Channel_size N> optional<Channel_size>     try_select(Channel_operation (&ops)[N]);
optional<Channel_size>                              try_select(Channel_operation*, Channel_operation*);


/*
    Future     
*/
template<class T>
class Future : boost::equality_comparable<Future<T>> {
public:
    // Names/Types
    class Awaitable;
    using Value             = T;
    using Value_receiver    = Receive_channel<T>;
    using Error_receiver    = Receive_channel<exception_ptr>;

    // Construct/Copy/Move
    Future();
    Future(Value_receiver, Error_receiver);
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    Future(Future&&);
    Future& operator=(Future&&);
    inline friend void swap(Future& x, Future& y) {
        using std::swap;
        swap(x.vchan, y.vchan);
        swap(x.echan, y.echan);
        swap(x.isready, y.isready);
    }

    // Result Access
    Awaitable   get();
    optional<T> try_get();

    // Observers
    bool is_valid() const;

    // Comparisons
    inline friend bool operator==(const Future& x, const Future& y) {
        if (x.vchan != y.vchan) return false;
        if (x.echan != y.echan) return false;
        if (x.isready != y.isready) return false;
        return true;
    }

    // Friends
    friend class Awaitable;

private:
    // Result Access
    bool    is_ready() const;
    T       get_ready();

    // Task Waiting
    Channel_base*   value() const;
    Channel_base*   error() const;
    bool*           ready() const;

    // Friends
    friend class Task::Future_selection::Future_transform;

    // Data
    Value_receiver  vchan;
    Error_receiver  echan;
    mutable bool    isready;
};


/*
    Future Awaitable
*/
template<class T>
class Future<T>::Awaitable {
public:
    // Awaitable Operations
    bool    await_ready();
    bool    await_suspend(Task::Handle);
    T       await_resume();

    // Friends
    friend class Future;

private:
    // Construct
    Awaitable(Future*);

    // Data
    Future*             selfp;
    T                   v;
    exception_ptr       ep;
    Channel_operation   ops[2];
};


/*
    Future All Awaitable
*/
template<class T>
class Future_all_awaitable {
public:
    // Construct
    Future_all_awaitable(const Future<T>*, const Future<T>*);

    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Task::Handle);
    void await_resume();

private:
    // Data
    const Future<T>* first;
    const Future<T>* last;
};


/*
    Future All Timed Awaitable
*/
template<class T>
class Future_all_timed_awaitable {
public:
    // Construct
    Future_all_timed_awaitable(const Future<T>*, const Future<T>*, nanoseconds maxtime);

    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Task::Handle);
    bool await_resume();

private:
    // Data
    const Future<T>*        first;
    const Future<T>*        last;
    optional<nanoseconds>   time;
    Task::Handle            task;
};


/*
    Future Any Awaitable
*/
template<class T>
class Future_any_awaitable {
public:
    // Construct
    Future_any_awaitable(const Future<T>*, const Future<T>*);
    Future_any_awaitable(const Future<T>*, const Future<T>*, nanoseconds maxtime);

    // Awaitable Operations
    bool            await_ready();
    bool            await_suspend(Task::Handle);
    Channel_size    await_resume();

private:
    // Data
    const Future<T>*        first;
    const Future<T>*        last;
    optional<nanoseconds>   time;
    Task::Handle            task;
};


/*
    Future Waiting
*/
template<class T> Future_any_awaitable<T>       wait_any(const vector<Future<T>>&);
template<class T> Future_any_awaitable<T>       wait_any(const vector<Future<T>>&, nanoseconds maxtime);
template<class T> Future_any_awaitable<T>       wait_any(const Future<T>*, const Future<T>*);
template<class T> Future_any_awaitable<T>       wait_any(const Future<T>*, const Future<T>*, nanoseconds maxtime);
template<class T> Future_all_awaitable<T>       wait_all(const vector<Future<T>>&);
template<class T> Future_all_awaitable<T>       wait_all(const Future<T>*, const Future<T>*);
template<class T> Future_all_timed_awaitable<T> wait_all(const vector<Future<T>>&, nanoseconds maxtime);
template<class T> Future_all_timed_awaitable<T> wait_all(const Future<T>*, const Future<T>*, nanoseconds maxtime);
static const Channel_size wait_fail = -1;


/*
    Scheduler

    A Scheduler multiplexes Tasks onto operating system threads.

    TODO: Should this concept should be generalized using virtual functions
    to accommodate distinct scheduling policies?
*/
class Scheduler {
public:
    // Construct/Destroy
    explicit Scheduler(int nthreads=0);
    Scheduler(const Scheduler&) = delete;
    Scheduler& operator=(const Scheduler&) = delete;
    ~Scheduler();

    // Execution
    void submit(Task&&);
    void resume(Task::Handle);

    // Timers
    void start_timer(Task::Handle, nanoseconds);
    void cancel_timer(Task::Handle);

private:
    // Names/Types
    using Thread    = std::thread;
    using Mutex     = std::mutex;
    using Lock      = std::unique_lock<Mutex>;
    using Condition = std::condition_variable;

    class Task_queue {
    public:
        // Construct/Copy
        Task_queue() = default;
        Task_queue(const Task_queue&) = delete;
        Task_queue& operator=(const Task_queue&) = delete;

        // Queue Operations
        void            push(Task&&);
        optional<Task>  pop();
        bool            try_push(Task&&);
        optional<Task>  try_pop();
        void            interrupt();
    
    private:
        // Queue Operations
        static Task pop_front(std::deque<Task>*);
    
        // Data
        std::deque<Task>    tasks;
        bool                is_interrupt{false};
        mutable Mutex       mutex;
        Condition           ready;
    };

    class Task_queue_array {
    private:
        // Names/Types
        using Queue_vector = std::vector<Task_queue>;

    public:
        // Names/Types
        using Size = Queue_vector::size_type;
    
        // Construct/Copy
        explicit Task_queue_array(Size n);
        Task_queue_array(const Task_queue_array&) = delete;
        Task_queue_array& operator=(const Task_queue_array&) = delete;
    
        // Size
        Size size() const;
    
        // Queue Operations
        void            push(Task&&);
        void            push(Size qpref, Task&&);
        optional<Task>  pop(Size qpref);
        void            interrupt();

    private:
        // Queue Operations
        static void push(Queue_vector*, Size qpref, Task&&);

        // Data
        Queue_vector        qs;
        std::atomic<Size>   nextq{0};
    };

    class Suspended_tasks {
    public:
        // Construct/Copy
        Suspended_tasks() = default;
        Suspended_tasks(const Suspended_tasks&) = delete;
        Suspended_tasks& operator=(const Suspended_tasks&) = delete;

        // Task Operations
        void insert(Task&&);
        Task release(Task::Handle);
    
    private:
        // Names/Types
        struct handle_equal {
            // Construct
            explicit handle_equal(Task::Handle);
            bool operator()(const Task&) const;
            Task::Handle h;
        };
    
        // Data
        std::vector<Task>   tasks;
        mutable Mutex       mutex;
    };

    class Timers {
    public:
        // Construct/Copy/Destroy
        Timers();
        Timers(const Timers&) = delete;
        Timers& operator=(const Timers&) = delete;
        ~Timers();

        // Timer Operations
        void start(Task::Handle, nanoseconds duration);
        void cancel(Task::Handle);

    private:
        /*
            The clock definition is platform dependent; it should be
            monotonic with adequate resolution.  std::chrono::steady_clock
            has nanosecond resolution on Windows.
        */
        using Clock = std::chrono::steady_clock;

        class Request {
        public:
            // Construct
            Request(Task::Handle, nanoseconds); // start
            explicit Request(Task::Handle);     // cancel

            // Observers
            Task::Handle    task() const;
            nanoseconds     time() const;

            // Types
            bool is_start() const;
            bool is_cancel() const;

        private:
            // Data
            Task::Handle    taskh;
            nanoseconds     duration;
        };

        using Request_queue = std::queue<Request>;

        struct Alarm : boost::totally_ordered<Alarm> {
            // Construct
            Alarm() = default;
            Alarm(Task::Handle h, Time_point t) : task{h}, expiry{t} {}

            // Comparisons
            inline friend bool operator==(const Alarm& x, const Alarm& y) {
                if (x.task != y.task) return false;
                if (x.expiry != y.expiry) return false;
                return true;
            }

            inline friend bool operator< (const Alarm& x, const Alarm& y) {
                return x.expiry < y.expiry;
            }

            // Data
            Task::Handle    task;
            Time_point      expiry;
        };

        class Alarm_queue {
        private:
            // Names/Types
            using Alarm_vector = std::vector<Alarm>;

            struct task_eq {
                explicit task_eq(Task::Handle h) : task{h} {}
                bool operator()(const Alarm& a) const {
                    return a.task == task;
                }
                Task::Handle task;
            };

            // Data
            Alarm_vector alarms;

        public:
            // Names/Types
            using Iterator = Alarm_vector::const_iterator;

            // Iterators
            Iterator begin() const;
            Iterator end() const;

            // Size
            int     size() const;
            bool    is_empty() const;

            // Queue Functions
            void    push(const Alarm&);
            Alarm   pop();
            void    erase(Iterator);

            // Element Access
            const Alarm&    front() const;
            Iterator        find(Task::Handle) const;
        };

        using Windows_handle = HANDLE;

        class Windows_handles {
        public:
            // Constants
            enum : int { request_pos, timer_pos, interrupt_pos, size };

            // Construct/Copy/Destroy
            Windows_handles();
            Windows_handles(const Windows_handles&) = delete;
            Windows_handles& operator=(const Windows_handles&) = delete;
            ~Windows_handles();

            // Synchronization
            int     wait_any(Lock*) const;
            void    signal_request() const;
            void    signal_interrupt() const;

            // Observers
            Windows_handle timer() const;

        private:
            // Destroy
            static void close(Windows_handle* hs, int n = size);

            // Data
            Windows_handle hs[size];
        };

        // Execution
        void run_thread();

        // Request Processing
        static Request  make_start(Task::Handle, nanoseconds duration);
        static Request  make_cancel(Task::Handle);
        static void     process_requests(Windows_handle timer, Request_queue*, Alarm_queue*);
        static void     process_request(Windows_handle timer, const Request&, Alarm_queue*, Time_point now);

        // Alarm Activation/Cancellation
        static Alarm    make_alarm(const Request&, Time_point now);
        static void     activate_alarm(Windows_handle timer, const Request&, Alarm_queue*, Time_point now);
        static void     remove_alarm(Windows_handle timer, Alarm_queue*, Alarm_queue::Iterator, Time_point now);
        static void     cancel_alarm(Windows_handle timer, const Request&, Alarm_queue*, Time_point now);
        static void     notify_cancel(const Alarm&);

        // Alarm Processing
        static void process_alarms(Windows_handle timer, Alarm_queue*, Lock*);
        static void notify_complete(const Alarm&, Time_point now, Lock*);
        static bool is_expired(const Alarm&, Time_point now);

        // Timer Functions
        static void set_timer(Windows_handle timer, const Alarm&, Time_point now);
        static void cancel_timer(Windows_handle timer);

        // Data
        Request_queue   requestq;
        Alarm_queue     alarmq;
        Windows_handles handles;
        mutable Mutex   mutex;
        Thread          thread;
    };
    
    // Execution
    void run_tasks(unsigned qpos);

    // Data
    Task_queue_array    ready;
    Suspended_tasks     suspended;
    std::vector<Thread> processors;
    Timers              timers;
};


extern Scheduler scheduler;


}   // Concurrency
}   // Isptech


#include "isptech/concurrency/task.inl"

#endif  // ISPTECH_CONCURRENCY_TASK_HPP

