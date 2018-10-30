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
template<class T> class Channel;
template<class T> class Send_channel;
template<class T> class Receive_channel;
class Channel_base;
class Channel_operation;
using Channel_size = std::ptrdiff_t;
using boost::optional;
using std::exception_ptr;
template<class T> class Future;


/*
    Task

    A Task is a lightweight cooperative thread implemented by a stackless
    coroutine.  Tasks are multiplexed onto operating system threads by a
    Scheduler.  A Task can suspend its execution without blocking the stack
    of the thread on which it was invoked, thus freeing the Scheduler to re-
    allocate the thread to another ready Task.
*/
class Task : boost::equality_comparable<Task> {
public:
    // Names/Types
    class Promise;
    using promise_type      = Promise;
    using Handle            = std::experimental::coroutine_handle<promise_type>;
    using Initial_suspend   = std::experimental::suspend_always;
    using Final_suspend     = std::experimental::suspend_always;

    class Promise {
    public: 
        // Coroutine Functions
        Task            get_return_object();
        Initial_suspend initial_suspend() const;
        Final_suspend   final_suspend() const;
    
        // Channel Operation Selection
        template<Channel_size N> bool                           select(Channel_operation (&ops)[N], Task::Handle);
        bool                                                    select(Channel_operation*, Channel_operation*, Task::Handle);
        bool                                                    select(Channel_size);
        Channel_size                                            selected();
        template<Channel_size N> static optional<Channel_size>  try_select(Channel_operation (&ops)[N]);


        bool wait_any(Channel_operation*, Channel_operation*);
        Channel_size 
    
    private:
        // Names/Types
        using Mutex = std::mutex;
        using Lock  = std::unique_lock<Mutex>;

        class Channel_sort {
        public:
            // Construct/Copy/Move/Destroy
            Channel_sort(Channel_operation*, Channel_operation*);
            Channel_sort(const Channel_sort&) = delete;
            Channel_sort& operator=(const Channel_sort&) = delete;
            ~Channel_sort();
        
            // Dismiss Guard
            void dismiss();
        
        private:
            // Data
            Channel_operation* first;
            Channel_operation* last;
        };

        class Channel_locks {
        public:
            // Construct/Copy/Move/Destroy
            Channel_locks(Channel_operation*, Channel_operation*);
            Channel_locks(const Channel_locks&) = delete;
            Channel_locks& operator=(const Channel_locks&) = delete;
            ~Channel_locks();
        
        private:
            // Data
            Channel_operation* first;
            Channel_operation* last;
        };

        class Enqueued_selection {
        public:
            // I/O
            bool            put(Channel_size pos);
            Channel_size    get();

        private:
            // Data
            Mutex                   mutex; // TODO: Could an atomic be used instead?
            optional<Channel_size>  buffer;
        };

        // Channel Operation Selection
        static optional<Channel_size>   select_ready(Channel_operation*, Channel_operation*);
        static Channel_size             count_ready(const Channel_operation*, const Channel_operation*);
        static Channel_operation*       pick_ready(Channel_operation*, Channel_operation*, Channel_size nready);
        static Channel_size             pick_random(Channel_size min, Channel_size max);
        static void                     enqueue(Channel_operation*, Channel_operation*, Task::Handle);
        static void                     dequeue(Channel_operation*, Channel_operation*, Task::Handle);

        // Channel Operation Synronization
        static void save_positions(Channel_operation*, Channel_operation*);
        static void sort_channels(Channel_operation*, Channel_operation*);
        static void lock(Channel_operation*, Channel_operation*);
        static void unlock(Channel_operation*, Channel_operation*);
        static void restore_positions(Channel_operation*, Channel_operation*);

        // Data
        optional<Channel_size>  readyco;    // ready selection
        Channel_operation*      firstenqco; // enqueued (ordered by channel)
        Channel_operation*      lastenqco;  // "
        Enqueued_selection      selenqco;
    };

    // Construct/Move/Destroy
    explicit Task(Handle = nullptr);
    Task(Task&&);
    Task& operator=(Task&&);
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task();

    // Observers
    Handle handle() const;

    // Execution
    void run();

    // Comparisons
    friend bool operator==(const Task&, const Task&);

private:
    // Move/Destroy
    void        steal(Task*);
    static void destroy(Task*);

    // Data
    Handle coro;
};


/*
    Task Launch
*/
template<class TaskFun, class... Args> void                                     start(TaskFun, Args&&...);
template<class Fun, class... Args> Future<std::result_of_t<Fun&&(Args&&...)>>   async(Fun, Args&&...);


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
    enum Type { none, send, receive };

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
    void dequeue(Task::Handle);

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
    Task::Promise*      promisep;
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
    virtual bool is_send_ready() const = 0;
    virtual void send_ready(void* valuep) = 0;
    virtual void send_ready(const void* valuep) = 0;
    virtual bool is_receive_ready() const = 0;
    virtual void receive_ready(void* valuep) = 0;

    // Blocking Send/Receive
    virtual void enqueue_send(Task::Handle, Channel_size selpos, void* valuep) = 0;
    virtual void enqueue_send(Task::Handle, Channel_size selpos, const void* valuep) = 0;
    virtual void dequeue_send(Task::Handle, Channel_size selpos) = 0;
    virtual void enqueue_receive(Task::Handle, Channel_size selpos, void* valuep) = 0;
    virtual void dequeue_receive(Task::Handle, Channel_size selpos) = 0;

    // Synchronization
    virtual void lock() = 0;
    virtual void unlock() = 0;
};


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

    // Construct/Move
    Channel();
    friend Channel make_channel<T>(Channel_size capacity);
    Channel& operator=(Channel);
    inline friend void swap(Channel& x, Channel& y) { swap(x.pimpl, y.pimpl;); }

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Send/Receive
    Send_awaitable      send(const T&) const;
    Send_awaitable      send(T&& x) const;
    Receive_awaitable   receive() const;
    bool                try_send(const T&) const;
    optional<T>         try_receive() const;
    void                sync_send(const T&) const;
    void                sync_send(T&&) const;
    T                   sync_receive() const;

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
        template<class U> void  push(U&&);
        void                    pop(optional<T>*);
        void                    pop(T*);

    private:
        // Data
        std::queue<T>   q;
        Channel_size    sizemax;
    };

    class Waiting_sender : boost::equality_comparable<Waiting_sender> {
    public:
        // Construct
        Waiting_sender(Task::Handle, Channel_size selpos, const T* rvaluep);
        Waiting_sender(Task::Handle, Channel_size selpos, T* lvaluep);
        Waiting_sender(Condition_variable*, const T* rvaluep);
        Waiting_sender(Condition_variable*, T* lvaluep);
    
        // Completion
        template<class U> bool dequeue(U* recvbufp) const;

        // Observers
        Task::Handle task() const;
        Channel_size select_position() const;
    
        // Comparisons
        inline friend bool operator==(const Waiting_sender& x, const Waiting_sender& y) {
            if (x.threadp != y.threadp) return false;
            if (x.rvalp != y.rvalp) return false;
            if (x.lvalp != y.lvalp) return false;
            if (x.taskh != y.taskh) return false;
            if (x.pos != y.pos) return false;
            return true;
        }
    
    private:
        // Data Transefer
        template<class U> static void   move(T* lvalp, const T* rvalp, U* destp);
        static void                     move(T* lvalp, const T* rvalp, Buffer* destp);
    
        // Data
        mutable Task::Handle    taskh;  // modify promise in dequeue()
        Channel_size            pos;    // select position
        const T*                rvalp;
        T*                      lvalp;
        Condition_variable*     threadp;
    };
    
    class Waiting_receiver : boost::equality_comparable<Waiting_receiver> {
    public:
        // Construct
        Waiting_receiver(Task::Handle, Channel_size selpos, T* valuep);
        Waiting_receiver(Condition_variable*, T* valuep);
    
        // Completion
        template<class U> bool dequeue(U* valuep) const;
    
        // Observers
        Task::Handle task() const;
        Channel_size select_position() const;
    
        // Comparisons
        inline friend bool operator==(const Waiting_receiver& x, const Waiting_receiver& y) {
            if (x.threadp != y.threadp) return false;
            if (x.valp != y.valp) return false;
            if (x.taskh != y.taskh) return false;
            if (x.pos != y.pos) return false;
            return true;
        }
    
    private:
        // Data
        mutable Task::Handle    taskh;  // modify promise in dequeue()
        Channel_size            pos;    // select position
        T*                      valp;
        Condition_variable*     threadp;
    };

    template<class U> 
    class Waitqueue {
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
    using Sender_waitqueue      = Waitqueue<Waiting_sender>;
    using Receiver_waitqueue    = Waitqueue<Waiting_receiver>;

    class Impl final : public Channel_base {
    public:
        // Construct
        explicit Impl(Channel_size);

        // Size and Capacity
        Channel_size size() const;
        Channel_size capacity() const;

        // Send/Receive
        template<class U> Send_awaitable    awaitable_send(U* valuep);
        template<class U> void              sync_send(U* valuep);
        bool                                try_send(const T&);
        Receive_awaitable                   awaitable_receive();
        T                                   sync_receive();
        optional<T>                         try_receive();

        // Operation Construction
        Channel_operation make_send(const T* valuep);
        Channel_operation make_send(T* valuep);
        Channel_operation make_receive(T* valuep);

        // Operation Readiness
        bool is_send_ready() const override;
        bool is_receive_ready() const override;

        // Non-Blocking Send/Receive
        void send_ready(const void* rvaluep) override;
        void send_ready(void* lvaluep) override;
        void receive_ready(void* valuep) override;

        // Blocking Send/Receive
        void enqueue_send(Task::Handle, Channel_size selpos, const void* rvaluep) override;
        void enqueue_send(Task::Handle, Channel_size selpos, void* lvaluep) override;
        void dequeue_send(Task::Handle, Channel_size selpos) override;
        void enqueue_receive(Task::Handle, Channel_size selpos, void* valuep) override;
        void dequeue_receive(Task::Handle, Channel_size selpos) override;

        // Synchronization
        void lock() override;
        void unlock() override;

    private:
        // Send/Receive
        template<class U> void          send_ready(U* valuep);
        void                            receive_ready(T* valuep);
        template<class U> void          enqueue_send(Task::Handle, Channel_size selpos, U* valuep);
        void                            enqueue_receive(Task::Handle, Channel_size selpos, T* valuep);
        template<class U> static bool   dequeue(Receiver_waitqueue*, U* sendbufp);
        template<class U> static bool   dequeue(Sender_waitqueue*, U* recvbufp);
        template<class U> static void   dequeue(U* waitqp, Task::Handle, Channel_size selpos);
        template<class U> static void   wait_for_receiver(Lock&, Sender_waitqueue*, U* sendbufp);
        static void                     wait_for_sender(Lock&, Receiver_waitqueue*, T* recvbufp);

        // Data
        Buffer              buffer;
        Sender_waitqueue    senders;
        Receiver_waitqueue  receivers;
        Mutex               mutex;
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
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable   send(const T&) const;
    Awaitable   send(T&&) const;
    bool        try_send(const T&) const;
    void        sync_send(const T&) const;
    void        sync_send(T&&) const;

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
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable   receive() const;
    optional<T> try_receive() const;
    T           sync_receive() const;

    // Selection
    Channel_operation make_receive(T*);

    // Conversions
    Receive_channel(const Channel<T>&);
    Receive_channel& operator=(const Channel<T>&);
    explicit operator bool() const;

    // Comparisons
    inline friend bool operator==(const Receive_channel& x, const Receive_channel& y) { return x.pimpl == y.pimpl; }
    inline friend bool operator< (const Receive_channel& x, const Receive_channel& y) { return x.pimpl < y.pimpl; }

private:
    // Data
    typename Channel<T>::Impl_ptr pimpl;
};


/*
    Future     
*/
template<class T>
class Future : boost::equality_comparable<Future<T>> {
public:
    // Names/Types
    class Awaitable;
    class Any_awaitable;
    class All_awaitable;
    using Value             = T;
    using Value_receiver    = Receive_channel<T>;
    using Error_receiver    = Receive_channel<exception_ptr>;

    // Construct/Copy/Move
    Future();
    Future(Value_receiver, Error_receiver);
    Future(const Future&) = default;
    Future& operator=(const Future&) = default;
    Future(Future&&);
    Future& operator=(Future&&);
    inline friend void swap(Future& x, Future& y) {
        using std::swap;

        swap(x.vchan, y.vchan);
        swap(x.echan, y.echan);
        swap(x.v, y.v);
        swap(x.ep, y.ep);
    }

    // Result Access
    Awaitable   get();
    bool        is_valid() const;

    // Comparisons
    inline friend bool operator==(const Future& x, const Future& y) {
        if (x.vchan != y.echan) return false;
        if (x.echan != y.echan) return false;
        return true;
    }

    // Friends
    friend class Awaitable;
    friend class Any_awaitable;
    friend class All_awaitable;

private:
    // Names/Types
    enum Operation_position { valuepos, errorpos };

    // Value Access
    bool    suspend(Task::Handle);
    T       resume(Task::Handle);
    T       result();

    // Data
    Value_receiver      vchan;
    Error_receiver      echan;
    T                   v;
    exception_ptr       ep;
    Channel_operation   ops[2];
};


/*
    Future Waiting
*/
template<class T> typename Future<T>::Any_awaitable wait_any(const std::vector<Future<T>>&);


/*
    Future Result Awaitable
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
    Future*         selfp;
    Task::Handle    task;
};


/*
    Future Any Awaitable
*/
template<class T>
class Future<T>::Any_awaitable {
public:
    // Construct
    Any_awaitable(const Future<T>*, const Future<T>*);

    // Awaitable Operations
    bool            await_ready();
    bool            await_suspend(Task::Handle);
    Channel_size    await_resume();

private:
    // Data
    const Future<T>*    first;
    const Future<T>*    last;
    Task::Handle        task;
};


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
    void suspend(Task::Handle);
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
        Mutex               mutex;
        Condition_variable  ready;
    };

    class Task_queue_array {
    private:
        // Names/Types
        using Task_queues = std::vector<Task_queue>;

    public:
        // Names/Types
        using Size = Task_queues::size_type;
    
        // Construct
        explicit Task_queue_array(Size n);
    
        // Size
        Size size() const;
    
        // Queue Operations
        void            push(Task&&);
        optional<Task>  pop(Size qpref);
        void            interrupt();
    
    private:
        // Data
        Task_queues         qs;
        std::atomic<Size>   nextq{0};
    };

    class Task_list {
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
        Mutex               mutex;
    };
    
    // Execution
    void run(unsigned qpos);

    // Data
    Task_queue_array    queues;
    std::vector<Thread> threads;
    Task_list           suspended;
};


extern Scheduler scheduler;


}   // Concurrency
}   // Isptech


#include "isptech/concurrency/task.inl"

#endif  // ISPTECH_CONCURRENCY_TASK_HPP

