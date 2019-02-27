//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/coroutine/task.hpp
//

//
//  IAPPA CM Revision # : $Revision: 1.9 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2019/01/18 23:06:47 $
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


/*
    Names/Types
*/
template<class T>   class Channel;
template<class T>   class Send_channel;
template<class T>   class Receive_channel;
template<>          class Channel<void>;
template<>          class Send_channel<void>;
template<>          class Receive_channel<void>;
template<class T>   class Future;
class Channel_base;
class Channel_operation;
using Channel_size = std::ptrdiff_t;
class Scheduler;
using Time          = std::chrono::steady_clock::time_point;
using Duration      = std::chrono::nanoseconds;
using Time_channel  = Channel<Time>;
class Timer;
using boost::optional;
using std::exception_ptr;
#pragma warning(disable: 4455)


/*
    Task

    A Task is a lightweight cooperative thread implemented by a stackless
    coroutine.  Tasks are multiplexed onto operating system threads by a
    Scheduler.  A Task can suspend its execution without blocking the thread
    on which it was invoked, thus freeing the Scheduler to re-assign the
    thread to another ready Task.

    TODO:  Creating a "task local" storage capability (a la
    std::thread_local) would make it possible to decouple the implementation
    of an open-ended number of synchronization constructs (e.g., channels,
    futures, etc.) from the Task itself (see boost::thread_specific_ptr for
    ideas).
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
        template<class T> bool  select_any(Task::Promise*, const Future<T>*, const Future<T>*, optional<Duration>);
        template<class T> bool  select_all(Task::Promise*, const Future<T>*, const Future<T>*, optional<Duration>);
        optional<Channel_size>  selection() const;
        bool                    is_selected() const;

        // Event Processing
        bool notify_channel_readable(Task::Promise*, Channel_size chan);
        bool notify_timer_expired(Task::Promise*, Time);
        bool notify_timer_canceled();

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
            void dequeue(Task::Promise*, Channel_size pos) const;
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
            static void dequeue_unlocked(Task::Promise*, const Channel_wait&, Channel_size pos);
            static bool is_enqueued(const Channel_wait&, const Channel_wait&);

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

        struct dequeue_locked {
            // Construct/Apply
            dequeue_locked(Task::Promise*, const Future_wait_vector&, const Channel_wait_vector&);
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

        struct enqueue_not_ready {
            // Construct/Apply
            enqueue_not_ready(Task::Promise*, const Future_wait_vector&, const Channel_wait_vector&);
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
            Channel_size            notify_readable(Task::Promise*, Channel_size chan);

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
            void start(Task::Promise*, Duration) const;
            void cancel(Task::Promise*) const;

            // Observers
            bool is_running() const;    // clock is ticking
            bool is_canceled() const;   // cancelation pending
            bool is_active() const;     // running or cancelation pending

            // Event Handling
            void notify_expired(Time) const;
            void notify_canceled() const;

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
        Channel_size            quota; // # currently being waiting for
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
        template<class T> void  wait_all(const Future<T>*, const Future<T>*, optional<Duration>);
        template<class T> void  wait_any(const Future<T>*, const Future<T>*, optional<Duration>);
        optional<Channel_size>  selected_future() const;
        bool                    selected_futures() const;

        /*
            Event Processing

            TODO: Seems like it would be better to let the task resume itself
            rather than make the caller do it (e.g., better encapsulation,
            more consistent with OS threads).
        */
        Select_status   notify_operation_complete(Channel_size pos);
        bool            notify_channel_readable(Channel_size pos);
        bool            notify_timer_expired(Time);
        bool            notify_timer_canceled();

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
    Channel Operation

    Users can select a channel operation from set of alternatives.  If
    multiple operations are ready, a random selection is made.  The need to
    select an operation from a set of channels with disparate element types
    suggests an operation interface that is independent of element types, so
    perhaps the ideal design would employ virtual functions.  But since that
    would imply dynamic memory allocation, we temporarily avoid that approach
    in favor of type-unsafe interfaces (with which users needn't directly
    interact).
    
    TODO:  Employ the small object optimization to make inheritance affordable?
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
    Channel Operation Selection Awaitable
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
    Channel Operation Selection
*/
template<Channel_size N> Channel_select_awaitable   select(const Channel_operation (&ops)[N]);
Channel_select_awaitable                            select(const Channel_operation*, const Channel_operation*);
template<Channel_size N> optional<Channel_size>     try_select(const Channel_operation (&ops)[N]);
optional<Channel_size>                              try_select(const Channel_operation*, const Channel_operation*);


/*
    Channel Construction
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
    template <class T> friend Channel<T> make_channel<T>(Channel_size capacity);
    inline friend void swap(Channel& x, Channel& y) { swap(x.pimpl, y.pimpl); }

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;
    bool            is_full() const;

    // Non-Blocking Send/Receive
    Send_awaitable      send(const T&) const;
    Send_awaitable      send(T&&) const;
    Receive_awaitable   receive() const;
    Channel_operation   make_send(const T&) const;
    Channel_operation   make_send(T&&) const;
    Channel_operation   make_receive(T*) const;
    bool                try_send(const T&) const;
    optional<T>         try_receive() const;

    // Blocking Send/Receive (move out of body if >= VS '17)
    inline friend void  blocking_send(const Channel& c, const T& x) { c.pimpl->blocking_send(&x); }
    inline friend void  blocking_send(const Channel& c, T&& x)      { c.pimpl->blocking_send(&x); }
    inline friend T     blocking_receive(const Channel& c)          { return c.pimpl->blocking_receive(); }

    // Conversions
    explicit operator bool() const;

    // Comparisons
    inline friend bool operator==(const Channel& x, const Channel& y) { return x.pimpl == y.pimpl; }
    inline friend bool operator< (const Channel& x, const Channel& y) { return x.pimpl < y.pimpl; }

    // Friends
    friend class Send_channel<T>;
    friend class Receive_channel<T>;
    friend class Channel<void>;
    friend class Send_channel<void>;
    friend class Receive_channel<void>;

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
        static bool notify_channel_readable(Task::Promise*, Channel_size pos, Mutex*);

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
        Channel_size    capacity() const;
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
            if (x.readyp != y.readyp) return false;
            if (x.rvalp != y.rvalp) return false;
            if (x.lvalp != y.lvalp) return false;
            if (x.taskp != y.taskp) return false;
            if (x.oper != y.oper) return false;
            return true;
        }
    
    private:
        // Selection
        template<class U> static bool select(Task::Promise*, Channel_size pos, T* lvalp, const T* rvalp, U* recvbufp, Mutex*);

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
        template<class U> static bool select(Task::Promise*, Channel_size pos, T* valp, U* sendbufp, Mutex*);

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

    /*
        TODO:  Should be a resusable internal library component parameterized
        on the lock type.
    */
    class Unlock_sentry {
    public:
        // Construct/Copy/Destroy
        explicit Unlock_sentry(Mutex*);
        Unlock_sentry(const Unlock_sentry&) = delete;
        Unlock_sentry& operator=(const Unlock_sentry&) = delete;
        ~Unlock_sentry();

    private:
        // Data
        Mutex* mutexp;
    };

    // Names/Types
    using Sender_queue      = Io_queue<Sender>;
    using Receiver_queue    = Io_queue<Receiver>;

    class Impl : public Channel_base {
    public:
        // Construct
        explicit Impl(Channel_size bufsize);

        // Size and Capacity
        Channel_size    size() const;
        Channel_size    capacity() const;
        bool            is_empty() const;
        bool            is_full() const;

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

        // Non-Blocking I/O
        void read(void* valuep) override;
        void write(const void* rvaluep) override;
        void write(void* lvaluep) override;
        bool is_readable() const override;
        bool is_writable() const override;

        // Blocking I/O
        void enqueue_read(Task::Promise*, Channel_size pos, void* valuep) override;
        bool dequeue_read(Task::Promise*, Channel_size pos) override;
        void enqueue_write(Task::Promise*, Channel_size pos, const void* rvaluep) override;
        void enqueue_write(Task::Promise*, Channel_size pos, void* lvaluep) override;
        bool dequeue_write(Task::Promise*, Channel_size pos) override;

        // Event Waiting
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

    // Operation Completion
    static Task::Select_status notify_operation_complete(Task::Promise* taskp, Channel_size pos, Mutex*);

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
    Send_channel() = default;
    Send_channel& operator=(Send_channel);
    inline friend void swap(Send_channel& x, Send_channel& y) { swap(x.pimpl, y.pimpl); }

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;
    bool            is_full() const;

    // Non-Blocking Channel Operations
    Awaitable           send(const T&) const;
    Awaitable           send(T&&) const;
    bool                try_send(const T&) const;
    Channel_operation   make_send(const T&) const;
    Channel_operation   make_send(T&&) const;

    // Blocking Channel Operations (move out of body if >= VS '17)
    inline friend void blocking_send(const Send_channel& c, const T& x) { c.pimpl->blocking_send(&x); }
    inline friend void blocking_send(const Send_channel& c, T&& x)      { c.pimpl->blocking_send(&x); }

    // Conversions
    Send_channel(Channel<T>);
    Send_channel& operator=(Channel<T>);
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
    Receive_channel() = default;
    Receive_channel& operator=(Receive_channel);
    inline friend void swap(Receive_channel& x, Receive_channel& y) { swap(x.pimpl, y.pimpl);  }

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;
    bool            is_full() const;

    // Non-Blocking Channel Operations
    Awaitable           receive() const;
    optional<T>         try_receive() const;
    Channel_operation   make_receive(T*) const;

    // Blocking Channel Operations (move out of body if >= VS '17)
    inline friend T blocking_receive(const Receive_channel& c) { return c.pimpl->blocking_receive(); }

    // Conversions
    Receive_channel(Channel<T>);
    Receive_channel& operator=(Channel<T>);
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
    Channel of "void"
*/
template<>
class Channel<void> : boost::totally_ordered<Channel<void>> {
public:
    // Names/Types
    class Awaitable;
    using Send_awaitable    = Awaitable;
    using Receive_awaitable = Awaitable;
    using Value             = void;

    // Construct/Copy/Move
    Channel() = default;
    Channel(const Channel&) = default;
    Channel& operator=(const Channel&) = default;
    Channel(Channel&&);
    Channel& operator=(Channel&&);
    friend void swap(Channel&, Channel&);
    template <class T> friend Channel<T> make_channel<T>(Channel_size capacity);

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;
    bool            is_full() const;

    // Non-Blocking Send/Receive
    Awaitable           send() const;
    Awaitable           receive() const;
    Channel_operation   make_send() const;
    Channel_operation   make_receive() const;
    bool                try_send() const;
    bool                try_receive() const;

    // Blocking Send/Receive
    friend void blocking_send(const Channel&);
    friend void blocking_receive(const Channel&);

    // Conversions
    explicit operator bool() const;

    // Comparisons
    friend bool operator==(const Channel&, const Channel&);
    friend bool operator< (const Channel&, const Channel&);

    // Friends
    friend class Send_channel<void>;
    friend class Receive_channel<void>;

private:
    // Names/Types
    class Impl : public Channel<char>::Impl {
    public:
        // Construct
        explicit Impl(Channel_size bufsize);

        // Non-Blocking Send/Receive
        Awaitable           send();
        Awaitable           receive();
        Channel_operation   make_send();
        Channel_operation   make_receive();
        bool                try_send();
        bool                try_receive();

        // Blocking Send/Receive
        void blocking_send();
        void blocking_receive();

    private:
        // Data
        char scratch;
    };

    using Impl_ptr = std::shared_ptr<Impl>;

    // Construct
    Channel(Impl_ptr);

    // Data
    Impl_ptr pimpl;
};


/*
    Channel of "void" Awaitable
*/
class Channel<void>::Awaitable {
public:
    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Task::Handle);
    void await_resume();

private:
    // Construct
    Awaitable(const Channel_operation&);

    // Friends
    friend class Impl;

    // Data
    Channel_operation operation[1];
};


/*
    Send Channel of "void"
*/
template<>
class Send_channel<void> : boost::totally_ordered<Send_channel<void>> {
public:
    // Names/Types
    using Value     = Channel<void>::Value;
    using Awaitable = Channel<void>::Awaitable;

    // Construct/Copy
    Send_channel() = default;
    Send_channel& operator=(Send_channel);
    friend void swap(Send_channel&, Send_channel&);

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;
    bool            is_full() const;

    // Non-Blocking Channel Operations
    Awaitable           send() const;
    bool                try_send() const;
    Channel_operation   make_send() const;

    // Blocking Channel Operations
    friend void blocking_send(const Send_channel&);

    // Conversions
    Send_channel(Channel<void>);
    Send_channel& operator=(Channel<void>);
    explicit operator bool() const;

    // Comparisons
    friend bool operator==(const Send_channel&, const Send_channel&);
    friend bool operator< (const Send_channel&, const Send_channel&);

private:
    // Data
    Channel<void>::Impl_ptr pimpl;
};


/*
    Receive Channel of "void"
*/
template<>
class Receive_channel<void> : boost::totally_ordered<Receive_channel<void>> {
public:
    // Names/Types
    using Value     = Channel<void>::Value;
    using Awaitable = Channel<void>::Awaitable;

    // Construct/Copy
    Receive_channel() = default;
    Receive_channel& operator=(Receive_channel);
    friend void swap(Receive_channel&, Receive_channel&);

    // Size and Capacity
    Channel_size    size() const;
    Channel_size    capacity() const;
    bool            is_empty() const;
    bool            is_full() const;

    // Non-Blocking Channel Operations
    Awaitable           receive() const;
    bool                try_receive() const;
    Channel_operation   make_receive() const;

    // Blocking Channel Operations
    friend void blocking_receive(const Receive_channel&);

    // Conversions
    Receive_channel(Channel<void>);
    Receive_channel& operator=(Channel<void>);
    explicit operator bool() const;

    // Comparisons
    friend bool operator==(const Receive_channel&, const Receive_channel&);
    friend bool operator< (const Receive_channel&, const Receive_channel&);

    // Friends
    friend class Future<void>;

private:
    // Data
    Channel<void>::Impl_ptr pimpl;
};


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

    // Operators
    Awaitable operator co_await();

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
    Future "void"
*/
template<>
class Future<void> : boost::equality_comparable<Future<void>> {
public:
    // Names/Types
    class Awaitable;
    using Value             = void;
    using Value_receiver    = Receive_channel<void>;
    using Error_receiver    = Receive_channel<exception_ptr>;

    // Construct/Copy
    Future();
    Future(Value_receiver, Error_receiver);
    Future(const Future&) = delete;
    Future& operator=(const Future&) = delete;
    Future(Future&&);
    Future& operator=(Future&&);
    friend void swap(Future&, Future&);

    // Result Access
    Awaitable   get();
    bool        try_get();

    // Observers
    bool is_valid() const;

    // Operators
    Awaitable operator co_await();

    // Comparisons
    friend bool operator==(const Future&, const Future&);

    // Friends
    friend class Awaitable;
    friend class Task;

private:
    // Result Access
    bool is_ready() const;
    void get_ready();

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
    Future "void" Awaitable
*/
class Future<void>::Awaitable {
public:
    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Task::Handle);
    void await_resume();

    // Friends
    friend class Future<void>;

private:
    // Construct
    Awaitable(Future*);

    // Data
    Future*             selfp;
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
    All_futures_awaitable(const Future<T>*, const Future<T>*, Duration);

    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Task::Handle);
    bool await_resume();

private:
    // Data
    const Future<T>*    first;
    const Future<T>*    last;
    optional<Duration>  timeout;
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
    Any_future_awaitable(const Future<T>*, const Future<T>*, Duration);

    // Awaitable Operations
    bool                    await_ready();
    bool                    await_suspend(Task::Handle);
    optional<Channel_size>  await_resume();

private:
    // Data
    const Future<T>*    first;
    const Future<T>*    last;
    optional<Duration>  timeout;
    Task::Handle        task;
};


/*
    Future Waiting
*/
template<class T> Any_future_awaitable<T>                   wait_any(const std::vector<Future<T>>&);
template<class T, Channel_size N> Any_future_awaitable<T>   wait_any(const Future<T> (&fs)[N]);
template<class T> Any_future_awaitable<T>                   wait_any(const Future<T>*, const Future<T>*);
template<class T> Any_future_awaitable<T>                   wait_any(const std::vector<Future<T>>&, Duration);
template<class T, Channel_size N> Any_future_awaitable<T>   wait_any(const Future<T> (&fs)[N], Duration);
template<class T> Any_future_awaitable<T>                   wait_any(const Future<T>*, const Future<T>*, Duration);
template<class T> All_futures_awaitable<T>                  wait_all(const std::vector<Future<T>>&);
template<class T, Channel_size N> All_futures_awaitable<T>  wait_all(const Future<T> (&fs)[N]);
template<class T> All_futures_awaitable<T>                  wait_all(const Future<T>*, const Future<T>*);
template<class T> All_futures_awaitable<T>                  wait_all(const std::vector<Future<T>>&, Duration);
template<class T, Channel_size N> All_futures_awaitable<T>  wait_all(const Future<T> (&fs)[N], Duration);
template<class T> All_futures_awaitable<T>                  wait_all(const Future<T>*, const Future<T>*, Duration);


/*
    Timer
*/
class Timer : boost::totally_ordered<Timer> {
public:
    // Names/Types
    using Awaitable = Time_channel::Receive_awaitable;

    // Construct/Copy/Destroy
    Timer() = default;
    explicit Timer(Duration);
    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;
    Timer(Timer&&);
    Timer& operator=(Timer&&);
    friend void swap(Timer&, Timer&);
    ~Timer();

    // Timer Functions
    bool stop();
    bool reset(Duration);
    bool is_active() const;
    bool is_ready() const;

    // Non-Blocking Receive
    Awaitable           receive();
    optional<Time>      try_receive();
    Channel_operation   make_receive(Time*);

    // Blocking Receive
    friend Time blocking_receive(Timer&);

    // Conversions
    explicit operator bool() const;

    // Comparisons
    friend bool operator==(const Timer&, const Timer&);
    friend bool operator< (const Timer&, const Timer&);

private:
    // Construct
    static Time_channel make_timer(Scheduler*, Duration);
    static bool         is_valid(Duration);

    // Data
    Time_channel chan;
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

    // Task Execution
    void submit(Task);
    void resume(Task::Promise*);

    /* 
        Timers

        TODO:  The timer functions don't feel appropriate as a public
        interface on the Scheduler, and making them private would might
        create circular dependencies beween the Task and the Scheduler
        (although this should probably be evaluated as temporary solution).
        If it's possible to find a better overall design (e.g., perhaps
        using task-specific data?) which decouples basic Task functionality
        (resume/suspend) from components requiring synchronization (e.g.,
        channel I/O, future waiting), that could be a big help.
    */
    void start_timer(Task::Promise*, Duration);
    void cancel_timer(Task::Promise*);

    // Friends
    friend class Timer;

private:
    // Names/Types
    using Thread    = std::thread;
    using Mutex     = std::mutex;
    using Lock      = std::unique_lock<Mutex>;
    using Condition = std::condition_variable;

    /*
        TODO:  Should be a resusable internal library component parameterized
        on the lock type.
    */
    class Unlock_sentry {
    public:
        // Construct/Copy/Destroy
        explicit Unlock_sentry(Lock*);
        Unlock_sentry(const Unlock_sentry&) = delete;
        Unlock_sentry& operator=(const Unlock_sentry&) = delete;
        ~Unlock_sentry();

    private:
        // Data
        Lock* lockp;
    };

    // TODO: Consider using values rather than rvalue references as sinks
    // in the Task_queue interfaces below?
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

    class Task_queues {
    private:
        // Names/Types
        using Queue_vector = std::vector<Task_queue>;

    public:
        // Names/Types
        using Size = Queue_vector::size_type;
    
        // Construct/Copy
        explicit Task_queues(Size nqueues);
        Task_queues(const Task_queues&) = delete;
        Task_queues& operator=(const Task_queues&) = delete;
    
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

        // Future Wait Timers
        void start(Task::Promise*, Duration);
        void cancel(Task::Promise*);

        // User Timers
        void start(const Time_channel&, Duration);
        bool reset(const Time_channel&, Duration);
        bool stop(const Time_channel&);

    private:
        /*
            The clock should be monotonic with adequate resolution.
            std::chrono::steady_clock has nanosecond resolution on Windows.
        */
        using Clock = std::chrono::steady_clock;

        /*
            TODO:  Alarms could benefit from virtual functions and the small
            object optimization:

                Alarm a = make_alarm(xxx);
                a.expire(now);
        */
        struct Alarm : boost::totally_ordered<Alarm> {
            // Construct
            Alarm() = default;
            Alarm(Task::Promise* tskp, Time t) : taskp{tskp}, time{t} {}
            Alarm(Time_channel c, Time t)
                : taskp{nullptr}, channel{std::move(c)}, time{t} {}

            // Comparisons
            inline friend bool operator==(const Alarm& x, const Alarm& y) {
                if (x.taskp != y.taskp) return false;
                if (x.channel != y.channel) return false;
                if (x.time != y.time) return false;
                return true;
            }

            inline friend bool operator< (const Alarm& x, const Alarm& y) {
                return x.time < y.time;
            }

            // Data
            Task::Promise*  taskp;
            Time_channel    channel;
            Time            time;
         };

        class Alarm_queue {
        private:
            // Names/Types
            using Alarm_deque = std::deque<Alarm>;

            struct Task_eq {
                explicit Task_eq(Task::Promise* tp) : taskp{tp} {}
                bool operator()(const Alarm& a) const {
                    return a.taskp == taskp;
                }
                Task::Promise* taskp;
            };

            struct Channel_eq {
                explicit Channel_eq(const Time_channel& c) : chan{c} {}
                bool operator()(const Alarm& a) const {
                    return a.channel == chan;
                }
                const Time_channel& chan;
            };

            // Alarm Comparison
            static Task_eq      id_eq(Task::Promise*);
            static Channel_eq   id_eq(const Time_channel&);

            // Data
            Alarm_deque alarms;

        public:
            // Names/Types
            using Iterator          = Alarm_deque::iterator;
            using Const_iterator    = Alarm_deque::const_iterator;

            // Iterators
            Iterator begin();
            Iterator end();

            // Size
            int     size() const;
            bool    is_empty() const;

            // Queue Functions
            Iterator    push(const Alarm&);
            void        reschedule(Iterator, Time expiry);
            Alarm       pop();
            void        erase(Iterator);

            // Element Access
            const Alarm&                front() const;
            template<class T> Iterator  find(const T& alarmid);

            // Observers
            Time next_expiry() const;
        };

        using Windows_handle = HANDLE;

        enum : int { timer_handle, interrupt_handle };

        class Windows_handles {
        public:
            // Construct/Copy/Destroy
            Windows_handles();
            Windows_handles(const Windows_handles&) = delete;
            Windows_handles& operator=(const Windows_handles&) = delete;
            ~Windows_handles();

            // Synchronization
            int     wait_any(Lock*) const;
            void    signal_interrupt() const;

            // Observers
            Windows_handle timer() const;

        private:
            // Constants
            static const int count{2};

            // Destroy
            static void close(Windows_handle* hs, int n = count);

            // Data
            Windows_handle hs[count];
        };

        // Execution
        void run_thread();

        // Alarm Management
        template<class T> void          sync_start(const T& id, Duration);
        template<class T> static void   start_alarm(const T& id, Duration, Alarm_queue*, Windows_handle timer);
        template<class T> bool          sync_cancel(const T& id);
        static bool                     cancel(Task::Promise*, Alarm_queue::Iterator, Alarm_queue*, Windows_handle timer);
        static bool                     cancel(Time_channel, Alarm_queue::Iterator, Alarm_queue*, Windows_handle timer);
        static void                     remove_canceled(Alarm_queue::Iterator, Alarm_queue*, Windows_handle timer);
        static void                     reschedule(Alarm_queue::Iterator, Duration, Alarm_queue*, Windows_handle timer);

         // Ready Alarm Processing
        static void process_ready(Alarm_queue*, Windows_handle timer, Lock*);
        static void signal_ready(Alarm_queue*, Time now, Lock*);
        static void signal_ready(const Alarm&, Time now, Lock*);
        static void signal_alarm(Task::Promise*, Time now, Lock*);
        static void signal_alarm(const Time_channel&, Time now);
        static bool notify_timer_expired(Task::Promise*, Time now, Lock*);
        static bool is_ready(const Alarm&, Time now);

        // Timer Functions
        static void set_timer(Windows_handle timer, const Alarm&, Time now);
        static void cancel_timer(Windows_handle timer);

        // Data
        Alarm_queue     alarmq;
        Windows_handles handles;
        mutable Mutex   mutex;
        Thread          thread;
    };
    
    // Task Execution
    void run_tasks(unsigned q);

    // User Timers
    void start_timer(const Time_channel&, Duration);
    bool stop_timer(const Time_channel&);
    bool reset_timer(const Time_channel&, Duration);

    // Data
    Task_queues         ready;
    Waiting_tasks       waiting;
    Timers              timers;
    std::vector<Thread> threads;
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
