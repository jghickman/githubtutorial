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
using boost::optional;
using std::exception_ptr;


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
    template<class T> friend class Future;

private:
    // Names/Types
    using Mutex = std::mutex;
    using Lock  = std::unique_lock<Mutex>;

    class Selection_status {
    public:
        // Construct
        Selection_status();
        Selection_status(Channel_size pos, bool comp);

        // Observers
        Channel_size    position() const;
        bool            is_complete() const;

    private:
        // Data
        Channel_size    selpos;
        bool            iscomp;
    };

    class Channel_selection {
    public:
        // Selection
        bool                                                    select(Channel_operation*, Channel_operation*, Handle task);
        Selection_status                                        select(Channel_size);
        Channel_size                                            selected();
        template<Channel_size N> static optional<Channel_size>  try_select(Channel_operation (&ops)[N]);

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
        
            // Dismissal
            void dismiss();
        
        private:
            // Data
            Channel_operation* first;
            Channel_operation* last;
        };

        // Selection
        static optional<Channel_size>   select_ready(Channel_operation*, Channel_operation*);
        static Channel_size             count_ready(const Channel_operation*, const Channel_operation*);
        static Channel_operation*       pick_ready(Channel_operation*, Channel_operation*, Channel_size nready);
        static Channel_size             enqueue(Channel_operation*, Channel_operation*, Handle task);
        static Channel_size             dequeue(Channel_operation*, Channel_operation*, Channel_size chosen, Handle task);

        // Sorting
        static void save_positions(Channel_operation*, Channel_operation*);
        static void sort_channels(Channel_operation*, Channel_operation*);
        static void restore_positions(Channel_operation*, Channel_operation*);

        // Data
        Channel_operation*      begin;
        Channel_operation*      end;
        optional<Channel_size>  chosen;
        Channel_size            nenqueued;
        Channel_size            ndequeued;
    };

    class Future_selection {
    public:
        // Selection
        template<class T> bool  wait_any(const Future<T>*, const Future<T>*, Handle task);
        template<class T> bool  wait_all(const Future<T>*, const Future<T>*, Handle task);
        Selection_status        notify_ready(Channel_size chanop);
        Channel_size            ready();

        // Friends
        template<class T> friend class Future;

    private:
        // Names/Types
        class Channel_wait {
        public:
            // Construct
            Channel_wait();
            Channel_wait(Channel_base*, bool* rdyflagp, Channel_size fpos);

            // Channel_waiting
            bool is_ready() const;
            void enqueue(Handle task, Channel_size pos) const;
            void dequeue(Handle task, Channel_size pos) const;
            void complete() const;

            // Observers
            Channel_base*   channel() const;
            Channel_size    future() const;

            // Synchronization
            void lock_channel() const;
            void unlock_channel() const;

        private:
            // Data
            Channel_base*   channelp;
            bool*           readyp;
            Channel_size    futpos;
        };

        using Channel_wait_vector   = std::vector<Channel_wait>;
        using Channel_wait_constptr = Channel_wait_vector::const_iterator;

        template<class T>
        class Transform {
        public:
            // Construct
            Transform(const Future<T>*, const Future<T>*, Channel_wait_vector*);

        private:
            // Transformation
            static Channel_wait value_wait(const Future<T>*, Channel_size pos);
            static Channel_wait error_wait(const Future<T>*, Channel_size pos);
            static void         sort_channels(Channel_wait_vector*);
            static void         transform(const Future<T>*, const Future<T>*, Channel_wait_vector::iterator out);
        };

        class Channel_locks {
        public:
            // Construct/Destroy
            explicit Channel_locks(const Channel_wait_vector*);
            ~Channel_locks();

        private:
            // Data
            const Channel_wait_vector* waitsp;
        };

        class Future_sort {
        public:
            // Construct/Destroy
            explicit Future_sort(Channel_wait_vector*);
            ~Future_sort();

        private:
            // Data
            Channel_wait_vector* waitsp;
        };

        // Selection
        static void                     enqueue(const Channel_wait_vector&, Handle task);
        static Channel_size             enqueue_not_ready(const Channel_wait_vector&, Handle task);
        static void                     dequeue(const Channel_wait_vector&);
        static Channel_size             count_ready(const Channel_wait_vector&);
        static Channel_wait_constptr    pick_ready(const Channel_wait_vector&, Channel_size nready);
        static optional<Channel_size>   select_ready(const Channel_wait_vector&);
        static Channel_size             future_count(const Channel_wait_vector&);

        // Data
        Channel_wait_vector     waits;
        optional<Channel_size>  chosen;
        Channel_size            nenqueued;  // futures (not channels)
        Channel_size            ndequeued;  // "
    };

    // Random Number Generation
    static Channel_size random(Channel_size min, Channel_size max);

public:
    // Names/Types
    class Promise {
    public: 
        // Construct
        Promise();

        // Coroutine Functions
        Task            get_return_object();
        Initial_suspend initial_suspend() const;
        Final_suspend   final_suspend();
    
        // Channel Operation Selection
        template<Channel_size N> void                           select(Channel_operation (&ops)[N]);
        void                                                    select(Channel_operation*, Channel_operation*);
        bool                                                    select(Channel_size);
        Channel_size                                            selected();
        template<Channel_size N> static optional<Channel_size>  try_select(Channel_operation (&ops)[N]);

        // Waiting
        template<class T> void  wait_all(const Future<T>*, const Future<T>*);
        template<class T> void  wait_any(const Future<T>*, const Future<T>*);
        Channel_size            ready_future();
        bool                    notify_ready(Channel_size pos);

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
template<class Fun> Future<std::result_of_t<Fun()>>                         async(Fun);
//template<class Fun, class... Args> Future<std::result_of_t<Fun(Args&&...)>> async(Fun, Args&&...);


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
    virtual void enqueue_write(Task::Handle, Channel_size selpos, void* valuep) = 0;
    virtual void enqueue_write(Task::Handle, Channel_size selpos, const void* valuep) = 0;
    virtual bool dequeue_write(Task::Handle, Channel_size selpos) = 0;
    virtual void enqueue_read(Task::Handle, Channel_size selpos, void* valuep) = 0;
    virtual bool dequeue_read(Task::Handle, Channel_size selpos) = 0;

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
    Channel();
    friend Channel make_channel<T>(Channel_size capacity);
    Channel& operator=(Channel);
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

    // Blocking Send/Receive (can move outside class body in VS 2017)
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
    using Mutex                 = std::mutex;
    using Lock                  = std::unique_lock<Mutex>;
    using Condition_variable    = std::condition_variable;

    class Readable_wait : boost::equality_comparable<Readable_wait> {
    public:
        // Construct
        Readable_wait();
        Readable_wait(Task::Handle, Channel_size pos);

        // Identity
        Task::Handle task() const;
        Channel_size position() const;

        // Notification
        void notify(Mutex*) const;

        // Comparisons
        friend bool operator==(const Readable_wait& x, const Readable_wait& y) {
            if (x.task() != y.task()) return false;
            if (x.position() != y.position()) return false;
            return true;
        }

    private:
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
        template<class U> void  push(U&&, Mutex*);
        template<class U> void  push_silent(U&&);
        void                    pop(optional<T>*);
        void                    pop(T*);

        // Waiters
        void enqueue(const Readable_wait&);
        void dequeue(const Readable_wait&);

    private:
        // Data
        std::queue<T>               q;
        Channel_size                sizemax;
        std::deque<Readable_wait>   readers;
    };

    class Sender : boost::equality_comparable<Sender> {
    public:
        // Construct
        Sender(Task::Handle, Channel_size selpos, const T* rvaluep);
        Sender(Task::Handle, Channel_size selpos, T* lvaluep);
        Sender(Condition_variable*, const T* rvaluep);
        Sender(Condition_variable*, T* lvaluep);
    
        // Completion
        template<class U> bool dequeue(U* recvbufp, Mutex*) const;

        // Comparisons
        inline friend bool operator==(const Sender& x, const Sender& y) {
            if (x.threadcondp != y.threadcondp) return false;
            if (x.rvalp != y.rvalp) return false;
            if (x.lvalp != y.lvalp) return false;
            if (x.taskh != y.taskh) return false;
            if (x.chanop != y.chanop) return false;
            return true;
        }
    
    private:
        // Selection
        template<class U> static bool select(Task::Handle, Channel_size chanop, T* lvalp, const T* rvalp, U* recvbufp);

        // Data Transefer
        template<class U> static void   move(T* lvalp, const T* rvalp, U* destp);
        static void                     move(T* lvalp, const T* rvalp, Buffer* destp);
    
        // Data
        mutable Task::Handle    taskh; // poor Handle const usage forces mutable
        Channel_size            chanop;
        const T*                rvalp;
        T*                      lvalp;
        Condition_variable*     threadcondp;
    };
    
    class Receiver : boost::equality_comparable<Receiver> {
    public:
        // Construct
        Receiver(Task::Handle, Channel_size cop, T* valuep);
        Receiver(Condition_variable*, T* valuep);
    
        // Selection
        template<class U> bool dequeue(U* valuep, Mutex*) const;
    
        // Comparisons
        inline friend bool operator==(const Receiver& x, const Receiver& y) {
            if (x.threadcondp != y.threadcondp) return false;
            if (x.valp != y.valp) return false;
            if (x.taskh != y.taskh) return false;
            if (x.chanop != y.chanop) return false;
            return true;
        }
    
    private:
        // Selection
        template<class U> static bool select(Task::Handle, Channel_size chanop, U* valuep);

        // Data
        mutable Task::Handle    taskh; // poor Handle const usage forces mutable
        Channel_size            chanop;
        T*                      valp;
        Condition_variable*     threadcondp;
    };

    template<class U> 
    class Io_queue {
    private:
        // Names/Types
        struct operation_eq {
            operation_eq(Task::Handle h, Channel_size selpos) : task{h}, pos{selpos} {}
            bool operator()(const U& w) const {
                return w.task() == task && w.select_position() == pos;
            }
            Task::Handle task;
            Channel_size pos;
        };

        // Data
        std::deque<U> ws;

    public:
        // Names/Types
        using Waiter    = U;
        using Iterator  = typename std::deque<U>::iterator;

        // Iterators
        Iterator end();

        // Size and Capacity
        bool is_empty() const;

        // Queue Operations
        void        push(const Waiter&);
        Waiter      pop();
        void        erase(Iterator);
        Iterator    find(Task::Handle, Channel_size selpos);
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
        void dequeue_readable_wait(Task::Handle, Channel_size pos) override;

        // Synchronization
        void lock() override;
        void unlock() override;

    private:
        // Non-Blocking I/O
        void                    read(T* valuep);
        template<class U> void  write(U* valuep);

        // Blocking I/O
        void                            enqueue_read(Task::Handle, Channel_size chanop, T* valuep);
        template<class U> void          enqueue_write(Task::Handle, Channel_size chanop, U* valuep);
        template<class U> static bool   dequeue(Receiver_queue*, U* sendbufp, Mutex*);
        template<class U> static bool   dequeue(Sender_queue*, U* recvbufp, Mutex*);
        template<class U> static bool   dequeue(U* waitqp, Task::Handle, Channel_size chanop);
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
    class Interface;
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
    Channel Selection Awaitable
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
    Channel_base*   value_channel() const;
    Channel_base*   error_channel() const;
    bool*           ready_flag() const;

    // Friends
    friend class Task::Future_selection;

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
    // Names/Types
    using Vector = std::vector<Future<T>>;

    // Construct
    explicit Future_all_awaitable(const Vector*);
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
    Future Any Awaitable
*/
template<class T>
class Future_any_awaitable {
public:
    // Names/Types
    using Vector        = std::vector<Future<T>>;
    using Vector_size   = typename Vector::size_type;

    // Construct
    explicit Future_any_awaitable(const Vector*);
    Future_any_awaitable(const Future<T>*, const Future<T>*);

    // Awaitable Operations
    bool        await_ready();
    bool        await_suspend(Task::Handle);
    Vector_size await_resume();

private:
    // Data
    const Future<T>*    first;
    const Future<T>*    last;
    Task::Handle        task;
};


/*
    Future Waiting
*/
template<class T> Future_all_awaitable<T> wait_all(const std::vector<Future<T>>&);
template<class T> Future_any_awaitable<T> wait_any(const std::vector<Future<T>>&);


/*
    Scheduler

    A Scheduler multiplexes Tasks onto operating system threads.

    TODO: This concept should probably be generalized to accommodate
    distinct scheduling policies (consider virtual functions).
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

private:
    // Names/Types
    using Thread                = std::thread;
    using Mutex                 = std::mutex;
    using Lock                  = std::unique_lock<Mutex>;
    using Condition_variable    = std::condition_variable;

    class Task_queue {
    public:
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
        Condition_variable  ready;
    };

    class Task_queue_array {
    private:
        // Names/Types
        using Queue_vector = std::vector<Task_queue>;

    public:
        // Names/Types
        using Size = Queue_vector::size_type;
    
        // Construct
        explicit Task_queue_array(Size n);
    
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
        // Construct
        void insert(Task&&);
        Task release(Task::Handle);
    
    private:
        // Names/Types
        using Mutex = std::mutex;
        using Lock  = std::unique_lock<Mutex>;
    
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
    
    // Execution
    void run_tasks(unsigned qpos);

    // Data
    Task_queue_array    workqueues;
    std::vector<Thread> threads;
    Suspended_tasks     waiters;
};


extern Scheduler scheduler;


}   // Concurrency
}   // Isptech


#include "isptech/concurrency/task.inl"

#endif  // ISPTECH_CONCURRENCY_TASK_HPP

