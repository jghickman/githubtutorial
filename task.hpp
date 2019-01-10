//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/coroutine/task.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2018/12/18 21:55:18 $
//  File Path           : $Source: //ftwgroups/data/iappa/CVSROOT/isptech_cvs/isptech/coroutine/task.hpp,v $
//  Source of funding   : IAPPA
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//

#ifndef ISPTECH_COROUTINE_TASK_HPP
#define ISPTECH_COROUTINE_TASK_HPP

#include "isptech/config.hpp"
#include "boost/operators.hpp"
#include "boost/optional.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstddef>
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
#include <windows.h>


/*
    Information and Sensor Processing Technology Coroutine Library
*/
namespace Isptech   {
namespace Coroutine {


void print(const char*);


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
class Scheduler;
using Time_point = std::chrono::steady_clock::time_point;
using Time_receiver = Receive_channel<Time_point>;
class Timer;
using boost::optional;
using std::exception_ptr;
using std::chrono::nanoseconds;
#pragma warning(disable: 4455)
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
    using Handle            = std::experimental::coroutine_handle<Promise>;
    using Initial_suspend   = std::experimental::suspend_always;
    using Final_suspend     = std::experimental::suspend_always;

    enum class State : int { ready, waiting, done };

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

    // Construct/Copy/Destroy
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
    State resume();

    // Conversions
    explicit operator bool() const;

    // Comparisons
    friend bool operator==(const Task&, const Task&);

    // Friends
    friend class Scheduler;
    friend class Channel_operation;

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

    class Operation_selector {
    public:
        // Construct/Copy 
        Operation_selector() = default;
        Operation_selector(const Operation_selector&) = delete;
        Operation_selector& operator=(const Operation_selector&) = delete;

        // Selection
        bool                    select(Task::Promise*, const Channel_operation*, const Channel_operation*);
        Channel_size            selected() const;
        optional<Channel_size>  try_select(const Channel_operation*, const Channel_operation*);

        // Event Processing
        Select_status notify_complete(Task::Promise*, Channel_size pos);

    private:
        // Names/Types
        class Operation_view : boost::totally_ordered<Operation_view> {
        public:
            // Construct
            Operation_view() = default;
            Operation_view(const Channel_operation*, Channel_size pos);

            // Execution
            bool is_ready() const;
            void enqueue(Task::Promise*) const;
            bool dequeue(Task::Promise*) const;
            void execute() const;

            // Observers
            Channel_base*   channel() const;
            Channel_size    position() const;

            // Comparisons
            bool operator==(Operation_view) const;
            bool operator< (Operation_view) const;

        private:
            // Data
            const Channel_operation*    pop;
            Channel_size                index;
        };

        using Operation_vector = std::vector<Operation_view>;

        class Transform_unique {
        public:
            // Construct
            Transform_unique(const Channel_operation*, const Channel_operation*, Operation_vector*);

        private:
            // Transformation
            static void transform(const Channel_operation*, const Channel_operation*, Operation_vector* outp);
            static void remove_duplicates(Operation_vector*);
        };

        class Channel_locks {
        public:
            explicit Channel_locks(const Operation_vector&);
            Channel_locks(const Channel_locks&) = delete;
            Channel_locks& operator=(const Channel_locks&) = delete;
            ~Channel_locks();

        private:
            // Iteration
            template<class T> static void for_each_channel(const Operation_vector&, T f);

            // Synchronization
            static void lock(Channel_base*);
            static void unlock(Channel_base*);

            // Data
            const Operation_vector& ops;
        };

        // Selection
        static optional<Channel_size>   select_ready(const Operation_vector&);
        static Channel_size             count_ready(const Operation_vector&);
        static Channel_size             pick_ready(const Operation_vector&, Channel_size nready);
        static Channel_size             enqueue(Task::Promise*, const Operation_vector&);
        static Channel_size             dequeue(Task::Promise*, const Operation_vector&, Channel_size selected);

        // Data
        Operation_vector        operations;
        Channel_size            nenqueued;
        optional<Channel_size>  winner;
    };

    class Future_selector {
    public:
        // Construct/Copy
        Future_selector() = default;
        Future_selector(const Future_selector&) = delete;
        Future_selector& operator=(const Future_selector&) = delete;

        // Selection
        template<class T> bool  select_any(Task::Promise*, const Future<T>*, const Future<T>*, optional<nanoseconds>);
        template<class T> bool  select_all(Task::Promise*, const Future<T>*, const Future<T>*, optional<nanoseconds>);
        optional<Channel_size>  selection() const;
        bool                    is_selected() const;

        // Event Processing
        bool notify_channel_readable(Task::Promise*, Channel_size chan);
        bool notify_timer_expired(Task::Promise*, Time_point);
        bool notify_timer_cancelled();

    private:
        // Names/Types
        class Wait_setup;

        class Channel_wait {
        public:
            // Construct
            Channel_wait() = default;
            Channel_wait(Channel_base*, Channel_size future);
    
            // Enqueue/Dequeue
            void enqueue(Task::Promise*, Channel_size pos) const;
            bool dequeue(Task::Promise*, Channel_size pos) const;
            bool is_enqueued() const;

            // Selection and Event Handling
            void complete() const;
            bool is_ready() const;

            // Observers
            Channel_base*   channel() const;
            Channel_size    future() const;

        private:
            // Data
            Channel_base*   chanp;
            Channel_size    fpos;
            mutable bool    is_enq{false};
        };

        class Channel_wait_lock {
        public:
            // Construct/Copy/Destroy
            explicit Channel_wait_lock(const Channel_wait&);
            Channel_wait_lock(const Channel_wait_lock&) = delete;
            Channel_wait_lock& operator=(const Channel_wait_lock&) = delete;
            ~Channel_wait_lock();

        private:
            // Data
            const Channel_wait& wait;
        };

        using Channel_wait_vector = std::vector<Channel_wait>;

        class Future_wait {
        public:
            // Construct
            Future_wait() = default;
            Future_wait(bool* readyp, Channel_size vchan, Channel_size echan);

            // Enqueue/Dequeue
            void enqueue(Task::Promise*, const Channel_wait_vector&) const;
            bool dequeue(Task::Promise*, const Channel_wait_vector&) const;
            bool dequeue_unlocked(Task::Promise*, const Channel_wait_vector&) const;

            // Selection and Event Handling
            bool complete(Task::Promise*, const Channel_wait_vector&, Channel_size pos) const;
            bool is_ready(const Channel_wait_vector&) const;

            // Observers
            Channel_size value() const;
            Channel_size error() const;

            // Comparisons
            bool operator==(const Future_wait&) const;
            bool operator< (const Future_wait&) const;

        private:
            // Dequeue
            static bool         dequeue_unlocked(Task::Promise*, const Channel_wait_vector&, Channel_size pos);
            static Channel_size other(Channel_size pos, Channel_size pos1, Channel_size pos2);

            // Data
            bool*           signalp;
            Channel_size    vpos;
            Channel_size    epos;
        };

        // Names/Types
        using Future_wait_vector = std::vector<Future_wait>;
        using Future_wait_index = std::vector<Future_wait_vector::size_type>;

        class Channel_locks {
        public:
            // Construct/Copy
            Channel_locks() = default;
            Channel_locks(const Channel_locks&) = delete;
            Channel_locks& operator=(const Channel_locks&) = delete;

            // Lock/Unlock
            void acquire(const Future_wait_vector&, const Channel_wait_vector&, const Future_wait_index&);
            void release();

            // Conversions
            explicit operator bool() const;

        private:
            // Names/Types
            using Channel_vector = std::vector<Channel_base*>;

            // Channel Processing
            template<class F> static void   for_each_unique(const Channel_vector&, F func);
            static void                     transform(const Future_wait_vector&, const Channel_wait_vector&, const Future_wait_index&, Channel_vector*);
            static void                     sort(Channel_vector*);
            static void                     lock(const Channel_vector&);
            static void                     unlock(const Channel_vector&);
            static void                     lock_channel(Channel_base*);
            static void                     unlock_channel(Channel_base*);

            // Data
            Channel_vector channels;
        };

        struct Enqueue_not_ready {
            // Construct/Apply
            Enqueue_not_ready(Task::Promise*, const Future_wait_vector&, const Channel_wait_vector&);
            Channel_size operator()(Channel_size n, Channel_size i) const;

            // Data
            Task::Promise*              taskp;
            const Future_wait_vector&   futures;
            const Channel_wait_vector&  channels;
        };

        struct Dequeue_locked {
            // Construct/Apply
            Dequeue_locked(Task::Promise*, const Future_wait_vector&, const Channel_wait_vector&);
            Channel_size operator()(Channel_size n, Channel_size i) const;

            // Data
            Task::Promise*              taskp;
            const Future_wait_vector&   futures;
            const Channel_wait_vector&  channels;
        };

        struct dequeue_unlocked {
            // Construct/Apply
            dequeue_unlocked(Task::Promise*, const Future_wait_vector&, const Channel_wait_vector&);
            Channel_size operator()(Channel_size n, Channel_size i) const;

            // Data
            Task::Promise*              taskp;
            const Future_wait_vector&   futures;
            const Channel_wait_vector&  channels;
        };

        class Future_set {
        public:
            // Construct/Copy
            Future_set() = default;
            Future_set(const Future_set&) = delete;
            Future_set& operator=(const Future_set&) = delete;

            // Assignment
            template<class T> void assign(const Future<T>*, const Future<T>*);

            // Size and Capacity
            Channel_size    size() const;
            bool            is_empty() const;

            // Enqueue/Dequeue
            Channel_size enqueue(Task::Promise*);
            Channel_size dequeue(Task::Promise*);
            Channel_size enqueued() const;

            // Selection and Event Handling
            optional<Channel_size>  select_ready();
            Channel_size            notify_ready(Task::Promise*, Channel_size chan);

            // Synchronization
            void lock_channels();
            void unlock_channels();

        private:
            // Assignment
            template<class T> static void   transform(const Future<T>*, const Future<T>*, Future_wait_vector*, Channel_wait_vector*);
            static void                     init(Future_wait_index*, const Future_wait_vector&);
            static void                     index_unique(const Future_wait_vector&, Future_wait_index*);
            static void                     sort(Future_wait_index*, const Future_wait_vector&);
            static void                     remove_duplicates(Future_wait_index*, const Future_wait_vector&);

            // Enqueue/Dequeue
            static Enqueue_not_ready    enqueue(Task::Promise*, const Future_wait_vector&, const Channel_wait_vector&);
            static Dequeue_locked       dequeue(Task::Promise*, const Future_wait_vector&, const Channel_wait_vector&);

            // Completion
            static Channel_size             count_ready(const Future_wait_index&, const Future_wait_vector&, const Channel_wait_vector&);
            static optional<Channel_size>   pick_ready(const Future_wait_index&, const Future_wait_vector&, const Channel_wait_vector&, Channel_size nready);

            // Data
            Channel_wait_vector channels;
            Future_wait_vector  futures;
            Future_wait_index   index;
            Channel_size        nenqueued; // futures
            Channel_locks       locks;
        };

        class Wait_setup {
        public:
            // Construct/Copy/Destroy
            template<class T> Wait_setup(const Future<T>*, const Future<T>*, Future_set*);
            Wait_setup(const Wait_setup&) = delete;
            Wait_setup& operator=(const Wait_setup&) = delete;
            ~Wait_setup();

        private:
            // Data
            Future_set* futuresp;
        };

        class Timer {
        public:
            // Execution
            void start(Task::Promise*, nanoseconds duration) const;
            void cancel(Task::Promise*) const;

            // Observers
            bool is_running() const;    // clock is ticking
            bool is_cancelled() const;  // cancellation pending
            bool is_active() const;     // running or cancellation pending

            // Event Handlers
            void notify_expired(Time_point) const;
            void notify_cancelled() const;

        private:
            // Constants
            enum State : int { inactive, running, cancel_pending };

            // Data
            mutable State state{inactive};
        };

        // Selection
        static bool is_ready(const Future_set&, Timer);

        // Data
        Future_set              futures;
        Channel_size            quota; // # currently being waited on
        Timer                   timer;
        optional<Channel_size>  result;
    };

    // Random Number Generation
    static Channel_size random(Channel_size min, Channel_size max);

    // Synchronization
    void unlock();

public:
    // Names/Types
    class Promise {
    public: 
        // Construct/Copy
        Promise();
        Promise(const Promise&) = delete;
        Promise& operator=(const Promise&) = delete;

        // Channel Operation Selection
        template<Channel_size N> void   select(const Channel_operation (&ops)[N]);
        void                            select(const Channel_operation*, const Channel_operation*);
        static optional<Channel_size>   try_select(const Channel_operation*, const Channel_operation*);
        Channel_size                    selected_operation() const;

        // Future Selection
        template<class T> void  wait_all(const Future<T>*, const Future<T>*, optional<nanoseconds> deadline=optional<nanoseconds>());
        template<class T> void  wait_any(const Future<T>*, const Future<T>*, optional<nanoseconds> deadline=optional<nanoseconds>());
        optional<Channel_size>  selected_future() const;
        bool                    selected_futures() const;

        // Event Processing
        Select_status   notify_operation_complete(Channel_size pos);
        bool            notify_channel_readable(Channel_size pos);
        bool            notify_timer_expired(Time_point);
        bool            notify_timer_cancelled();

        // Execution
        void    make_ready();
        State   state() const;

        // Synchronization
        void unlock();

        // Coroutine Functions
        Task            get_return_object();
        Initial_suspend initial_suspend() const;
        Final_suspend   final_suspend();

    private:
        // Execution
        void suspend(Lock*);

        // Data
        mutable Mutex       mutex;
        Operation_selector  operations;
        Future_selector     futures;
        State               taskstate;
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


/*
    Channel Base
*/
class Channel_base {
public:
    // Copy/Destory
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

    /*
        TODO:  Is there better name than "pos" for the enqueue/dequeue
        interfaces below (e.g., id, wait(er), read(er), or write(r))?
    */

    // Blocking Send/Receive
    virtual void enqueue_write(Task::Promise*, Channel_size pos, void* valuep) = 0;
    virtual void enqueue_write(Task::Promise*, Channel_size pos, const void* valuep) = 0;
    virtual bool dequeue_write(Task::Promise*, Channel_size pos) = 0;
    virtual void enqueue_read(Task::Promise*, Channel_size pos, void* valuep) = 0;
    virtual bool dequeue_read(Task::Promise*, Channel_size pos) = 0;

    // Waiting
    virtual void enqueue_readable_wait(Task::Promise*, Channel_size pos) = 0;
    virtual bool dequeue_readable_wait(Task::Promise*, Channel_size pos) = 0;

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

    // Construct/Copy
    Channel() = default;
    Channel(const Channel&) = default;
    Channel& operator=(const Channel&) = default;
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

    class Readable_waiter : boost::equality_comparable<Readable_waiter> {
    public:
        // Construct
        Readable_waiter() = default;
        Readable_waiter(Task::Promise*, Channel_size chan);

        // Identity
        Task::Promise*  task() const;
        Channel_size    channel() const;

        // Selection
        void notify(Mutex*) const;

        // Comparisons
        friend bool operator==(const Readable_waiter& x, const Readable_waiter& y) {
            if (x.task() != y.task()) return false;
            if (x.channel() != y.channel()) return false;
            return true;
        }

    private:
        // Selection
        static void notify(Task::Promise*, Channel_size pos);

        // Data
        Task::Promise*  taskp;
        Channel_size    chanpos;
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
        void enqueue(const Readable_waiter&);
        bool dequeue(const Readable_waiter&);

    private:
        // Data
        std::queue<T>               elemq;
        Channel_size                sizemax;
        std::deque<Readable_waiter> readers;
    };

    class Sender : boost::equality_comparable<Sender> {
    public:
        // Construct
        Sender(Task::Promise*, Channel_size pos, const T* rvaluep);
        Sender(Task::Promise*, Channel_size pos, T* lvaluep);
        Sender(Condition* waitp, const T* rvaluep);
        Sender(Condition* waitp, T* lvaluep);
    
        // Observers
        Task::Promise* task() const;
        Channel_size operation() const;

        // Completion
        template<class U> bool dequeue(U* recvbufp, Mutex*) const;

        // Comparisons
        inline friend bool operator==(const Sender& x, const Sender& y) {
            if (x.condp != y.condp) return false;
            if (x.rvalp != y.rvalp) return false;
            if (x.lvalp != y.lvalp) return false;
            if (x.taskp != y.taskp) return false;
            if (x.oper != y.oper) return false;
            return true;
        }
    
    private:
        // Selection
        template<class U> static bool select(Task::Promise*, Channel_size pos, T* lvalp, const T* rvalp, U* recvbufp);

        // Data Transefer
        template<class U> static void   move(T* lvalp, const T* rvalp, U* destp);
        static void                     move(T* lvalp, const T* rvalp, Buffer* destp);
    
        // Data
        Task::Promise*  taskp;
        Channel_size    oper;
        const T*        rvalp;
        T*              lvalp;
        Condition*      readyp;
    };
    
    class Receiver : boost::equality_comparable<Receiver> {
    public:
        // Construct
        Receiver(Task::Promise*, Channel_size pos, T* valuep);
        Receiver(Condition* waitp, T* valuep);

        // Observers
        Task::Promise*  task() const;
        Channel_size    operation() const;
    
        // Selection
        template<class U> bool dequeue(U* sendbufp, Mutex*) const;
    
        // Comparisons
        inline friend bool operator==(const Receiver& x, const Receiver& y) {
            if (x.readyp != y.readyp) return false;
            if (x.valp != y.valp) return false;
            if (x.taskp != y.taskp) return false;
            if (x.oper != y.oper) return false;
            return true;
        }
    
    private:
        // Selection
        template<class U> static bool select(Task::Promise*, Channel_size pos, T* valp, U* sendbufp);

        // Data
        Task::Promise*  taskp;
        Channel_size    oper;
        T*              valp;
        Condition*      readyp;
    };

    template<class U> 
    class Io_queue {
    private:
        // Names/Types
        using Waiter_deque = std::deque<U>;

        struct waiter_eq {
            waiter_eq(Task::Promise* tp, Channel_size pos) : taskp{tp}, oper{pos} {}
            bool operator()(const U& waiting) const {
                return waiting.task() == taskp && waiting.operation() == oper;
            }
            Task::Promise*  taskp;
            Channel_size    oper;
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
        Iterator    find(Task::Promise*, Channel_size pos);
        bool        is_found(const Waiter&) const;
    };

    // Names/Types
    using Sender_queue      = Io_queue<Sender>;
    using Receiver_queue    = Io_queue<Receiver>;

    class Impl final : public Channel_base {
    public:
        // Construct
        explicit Impl(Channel_size bufsize);

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
        void enqueue_read(Task::Promise*, Channel_size pos, void* valuep) override;
        bool dequeue_read(Task::Promise*, Channel_size pos) override;
        void enqueue_write(Task::Promise*, Channel_size pos, const void* rvaluep) override;
        void enqueue_write(Task::Promise*, Channel_size pos, void* lvaluep) override;
        bool dequeue_write(Task::Promise*, Channel_size pos) override;

        // Waiting
        void enqueue_readable_wait(Task::Promise*, Channel_size pos) override;
        bool dequeue_readable_wait(Task::Promise*, Channel_size pos) override;

        // Synchronization
        void lock() override;
        void unlock() override;

    private:
        // Non-Blocking I/O
        template<class U> bool read(U* valuep, Buffer*, Sender_queue*, Mutex*);
        template<class U> bool write(U* valuep, Buffer*, Receiver_queue*, Mutex*);

        // Blocking I/O
        void                            enqueue_read(Task::Promise*, Channel_size pos, T* valuep);
        template<class U> void          enqueue_write(Task::Promise*, Channel_size pos, U* valuep);
        template<class U> static bool   dequeue(Receiver_queue*, U* sendbufp, Mutex*);
        template<class U> static bool   dequeue(Sender_queue*, U* recvbufp, Mutex*);
        template<class U> static bool   dequeue(U* waitqp, Task::Promise*, Channel_size pos);
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
    T       await_resume();

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

    // Construct/Copy
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

    // Construct/Copy
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
    is ready, a random choice is made.  Selecting from a set of channels
    with disparate element types requires an operation interface that is
    independent of element types, so perhaps the ideal design would involve
    channels which construct type-safe operation implementations using
    inheritance to hide the element type.  But that would imply dynamic
    memory allocation, so we temporarily avoid that approach in favor of
    type-unsafe interfaces with which users needn't directly interact.
    Perhaps the small object optimization could be employed in the future
    to realize a better design.
*/
class Channel_operation : boost::totally_ordered<Channel_operation> {
public:
    // Names/Types
    enum class Type : int { none, send, receive };

    // Construct/Copy
    Channel_operation();
    Channel_operation(Channel_base*, const void* rvaluep); // send copy
    Channel_operation(Channel_base*, void* lvaluep, Type); // send movable/receive

    // Execution
    bool is_ready() const;
    void enqueue(Task::Promise*, Channel_size pos) const;
    bool dequeue(Task::Promise*, Channel_size pos) const;
    void execute() const;

    // Observers
    Channel_base* channel() const;

    // Comparisons
    friend inline bool operator==(const Channel_operation& x, const Channel_operation& y) {
        if (x.chanp != y.chanp) return false;
        if (x.kind != y.kind) return false;
        return true;
    }

    inline friend bool operator< (const Channel_operation& x, const Channel_operation& y) {
        if (x.chanp < y.chanp) return true;
        if (y.chanp < x.chanp) return false;
        if (x.kind < y.kind) return true;
        return false;
    }

private:
    // Data
    Channel_base*   chanp;
    Type            kind;
    const void*     rvalp;
    void*           lvalp;
};


/*
    Channel Select Awaitable
*/
class Channel_select_awaitable {
public:
    // Construct
    Channel_select_awaitable(const Channel_operation*, const Channel_operation*);

    // Awaitable Operations
    bool            await_ready();
    bool            await_suspend(Task::Handle);
    Channel_size    await_resume();

private:
    // Data
    Task::Handle                task;
    const Channel_operation*    first;
    const Channel_operation*    last;
};


/*
    Channel Selection
*/
template<Channel_size N> Channel_select_awaitable   select(const Channel_operation (&ops)[N]);
Channel_select_awaitable                            select(const Channel_operation*, const Channel_operation*);
template<Channel_size N> optional<Channel_size>     try_select(const Channel_operation (&ops)[N]);
optional<Channel_size>                              try_select(const Channel_operation*, const Channel_operation*);


/*
    Future

    TODO: Provide a non-member blocking_get()
*/
template<class T>
class Future : boost::equality_comparable<Future<T>> {
public:
    // Names/Types
    class Awaitable;
    using Value             = T;
    using Value_receiver    = Receive_channel<T>;
    using Error_receiver    = Receive_channel<exception_ptr>;

    // Construct/Copy
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
    friend class Task;

private:
    // Result Access
    bool    is_ready() const;
    T       get_ready();

    // Task Waiting
    Channel_base*   value_channel() const;
    Channel_base*   error_channel() const;
    bool*           ready_flag() const;

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
    All Futures Awaitable
*/
template<class T>
class All_futures_awaitable {
public:
    // Construct
    All_futures_awaitable(const Future<T>*, const Future<T>*);
    All_futures_awaitable(const Future<T>*, const Future<T>*, nanoseconds maxtime);

    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Task::Handle);
    bool await_resume();

private:
    // Data
    const Future<T>*    first;
    const Future<T>*    last;
    nanoseconds         time;
    Task::Handle        task;
};


/*
    Any Future Awaitable
*/
template<class T>
class Any_future_awaitable {
public:
    // Construct
    Any_future_awaitable(const Future<T>*, const Future<T>*);
    Any_future_awaitable(const Future<T>*, const Future<T>*, nanoseconds maxtime);

    // Awaitable Operations
    bool                    await_ready();
    bool                    await_suspend(Task::Handle);
    optional<Channel_size>  await_resume();

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
template<class T> Any_future_awaitable<T>                   wait_any(const vector<Future<T>>&);
template<class T, Channel_size N> Any_future_awaitable<T>   wait_any(const Future<T> (&fs)[N]);
template<class T> Any_future_awaitable<T>                   wait_any(const Future<T>*, const Future<T>*);
template<class T> Any_future_awaitable<T>                   wait_any(const vector<Future<T>>&, nanoseconds maxtime);
template<class T, Channel_size N> Any_future_awaitable<T>   wait_any(const Future<T> (&fs)[N], nanoseconds maxtime);
template<class T> Any_future_awaitable<T>                   wait_any(const Future<T>*, const Future<T>*, nanoseconds maxtime);
template<class T> All_futures_awaitable<T>                  wait_all(const vector<Future<T>>&);
template<class T, Channel_size N> All_futures_awaitable<T>  wait_all(const Future<T> (&fs)[N]);
template<class T> All_futures_awaitable<T>                  wait_all(const Future<T>*, const Future<T>*);
template<class T> All_futures_awaitable<T>                  wait_all(const vector<Future<T>>&, nanoseconds maxtime);
template<class T, Channel_size N> All_futures_awaitable<T>  wait_all(const Future<T> (&fs)[N], nanoseconds maxtime);
template<class T> All_futures_awaitable<T>                  wait_all(const Future<T>*, const Future<T>*, nanoseconds maxtime);


/*
    Timer
*/
class Timer : boost::totally_ordered<Timer> {
public:
    // Construct/Copy/Destroy
    Timer();
    explicit Timer(nanoseconds);
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&);
    Timer& operator=(Timer&&);
    ~Timer();

    // Modifiers
    bool stop();
    bool reset(nanoseconds);

    // Channel
    const Time_receiver& channel() const;

private:
    // Data
    Time_receiver chan;
};


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
    void submit(Task);
    void resume(Task::Promise*);

    // Timers
    void start_timer(Task::Promise*, nanoseconds duration);
    void cancel_timer(Task::Promise*);

private:
    // Names/Types
    using Thread    = std::thread;
    using Mutex     = std::mutex;
    using Lock      = std::unique_lock<Mutex>;
    using Condition = std::condition_variable;

    // TODO: Consider using values rather than rvalue references as sinks
    // below?

    class Task_queue {
    public:
        // Construct/Copy
        Task_queue() = default;
        Task_queue(const Task_queue&) = delete;
        Task_queue& operator=(const Task_queue&) = delete;

        // Queue Operations
        void push(Task&&);
        Task pop();
        bool try_push(Task&&);
        Task try_pop();
        void interrupt();
    
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
        explicit Task_queue_array(Size nqueues);
        Task_queue_array(const Task_queue_array&) = delete;
        Task_queue_array& operator=(const Task_queue_array&) = delete;
    
        // Size
        Size size() const;
    
        // Queue Operations
        void push(Task&&);
        void push(Size qpref, Task&&);
        Task pop(Size qpref);
        void interrupt();

    private:
        // Queue Operations
        static void push(Queue_vector*, Size qpref, Task&&);

        // Data
        Queue_vector        qs;
        std::atomic<Size>   nextq{0};
    };

    class Waiting_tasks {
    public:
        // Construct/Copy
        Waiting_tasks() = default;
        Waiting_tasks(const Waiting_tasks&) = delete;
        Waiting_tasks& operator=(const Waiting_tasks&) = delete;

        // Task Operations
        void insert(Task&&);
        Task release(Task::Handle);
    
    private:
        // Names/Types
        struct handle_eq {
            // Construct
            explicit handle_eq(Task::Handle);
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
        void start(Task::Promise*, nanoseconds duration);
        void cancel(Task::Promise*);

    private:
        /*
            The clock should be monotonic with adequate resolution.
            std::chrono::steady_clock has nanosecond resolution on Windows.
        */
        using Clock = std::chrono::steady_clock;

        class Request {
        public:
            // Construct
            Request(Task::Promise*, nanoseconds); // start wait
            explicit Request(Task::Promise*);     // cancel wait

            // Types
            bool is_start() const;
            bool is_cancel() const;

            // Observers
            Task::Promise*  task() const;
            nanoseconds     time() const;

        private:
            // Data
            Task::Promise*  taskp;
            nanoseconds     duration;
        };

        using Request_queue = std::queue<Request>;
        using Time_channel  = Channel<Time_point>;

        struct Alarm : boost::totally_ordered<Alarm> {
            // Construct
            Alarm() = default;
            Alarm(Task::Promise* tskp, Time_point t) : taskp{tskp}, expiry{t} {}
            Alarm(Time_channel c, Time_point t, nanoseconds p = nanoseconds::zero())
                : channel{std::move(c)} , expiry{t} , period{p} {}

            // Comparisons
            inline friend bool operator==(const Alarm& x, const Alarm& y) {
                if (x.taskp != y.taskp) return false;
                if (x.channel != y.channel) return false;
                if (x.expiry != y.expiry) return false;
                if (x.period != y.period) return false;
                return true;
            }

            inline friend bool operator< (const Alarm& x, const Alarm& y) {
                return x.expiry < y.expiry;
            }

            // Data
            Task::Promise*  taskp;
            Time_channel    channel;
            Time_point      expiry;
            nanoseconds     period;
        };

        class Alarm_queue {
        private:
            // Names/Types
            using Alarm_vector = std::vector<Alarm>;

            struct task_eq {
                explicit task_eq(Task::Promise* tp) : taskp{tp} {}
                bool operator()(const Alarm& a) const {
                    return a.taskp == taskp;
                }
                Task::Promise* taskp;
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
            Iterator        find(Task::Promise*) const;
        };

        using Windows_handle = HANDLE;

        enum : int { request_handle, timer_handle, interrupt_handle };

        class Windows_handles {
        public:
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
            // Constants
            static const int count{3};

            // Destroy
            static void close(Windows_handle* hs, int n = count);

            // Data
            Windows_handle hs[count];
        };

        // Execution
        void run_thread();

        // Request Processing
        static Request  make_start(Task::Promise*, nanoseconds duration);
        static Request  make_cancel(Task::Promise*);
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
    void run_tasks(unsigned q);

    // Data
    Task_queue_array    ready;
    std::vector<Thread> processors;
    Waiting_tasks       waiting;
    Timers              timers;
};


/*
    The Global Scheduler

    TODO: Investigate the merits of supporting distinct scheduling
    implementations.
*/
extern Scheduler scheduler;


}   // Coroutine
}   // Isptech


#include "isptech/coroutine/task.inl"

#endif  // ISPTECH_COROUTINE_TASK_HPP

//  $CUSTOM_FOOTER$
