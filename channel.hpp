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
#include "boost/optional.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <condition_variable>
#include <cstdlib>
#include <deque>
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
class Channel_operation;
class Channel_select_awaitable;
using Channel_size = std::ptrdiff_t;
using boost::optional;


/*
    Goroutine Launcher
*/
template<class GoFun, class... ArgTypes> void go(GoFun&&, ArgTypes&&...);


/*
    Goroutine

    A Goroutine is a lightweight thread implemented as a coroutine. Goroutine
    execution can be suspended and resumed on arbitrary operating system
    threads by a Scheduler.
*/
class Goroutine : boost::equality_comparable<Goroutine> {
public:
    // Names/Types
    class Final_suspend;
    using Initial_suspend = std::experimental::suspend_always;

    class Promise {
    public:
        // Construct
        Promise();
    
        // Coroutine Functions
        Goroutine       get_return_object();
        Initial_suspend initial_suspend() const;
        Final_suspend   final_suspend() const;
        void            return_void();
    
        // Completion
        void done();
        bool is_done() const;
    
    private:
        // Data
        bool isdone;
    };

    using promise_type  = Promise;
    using Handle        = std::experimental::coroutine_handle<Promise>;

    class Final_suspend {
    public:
        // Awaitable Operations
        bool await_ready();
        bool await_suspend(Handle);
        void await_resume();
    };

    // Construct/Move/Destroy
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
    // Move/Destroy
    void        steal(Goroutine*);
    static void destroy(Goroutine*);

    // Data
    Handle  coro{nullptr};
    bool    isowner;
};


/*
    Implementation Details
*/
namespace Detail {


/*
    Names/Types
*/
using std::condition_variable;


/*
    Channel Alternatives
*/
class Channel_alternatives {
public:
    // Names/Types
    static const Channel_size none = -1;

    // Construct/Copy/Move/Destroy
    Channel_alternatives();
    Channel_alternatives(const Channel_alternatives&) = delete;
    Channel_alternatives& operator=(const Channel_alternatives&) = delete;
    Channel_alternatives(Channel_alternatives&&);
    Channel_alternatives& operator=(Channel_alternatives&&);
    ~Channel_alternatives();

    // Selection
    template<Channel_size N> Channel_size   select(Channel_operation (&ops)[N]);
    Channel_size                            selected() const;
    void                                    enqueue(Goroutine::Handle);

private:
    // Names/Types
    struct is_ready {
        bool operator()(const Channel_operation&) const;
    };

    // Selection
    static void         save_order(Channel_operation*, Channel_operation*);
    static void         restore_order(Channel_operation*, Channel_operation*);
    static void         lock_channels(Channel_operation*, Channel_operation*);
    static void         unlock_channels(Channel_operation*, Channel_operation*);
    static Channel_size random();
    static Channel_size select(Channel_operation*, Channel_operation*);
    static void         sort_channels(Channel_operation*, Channel_operation*);
    static void         sort_positions(Channel_operation*, Channel_operation*);

    // Data
    Channel_operation*  first;
    Channel_operation*  last;
    Channel_size        selectpos;
};


/*
    Channel Buffer
*/
template<class T>
class Channel_buffer {
public:
    // Construct
    explicit Channel_buffer(Channel_size maxsize);

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

    
/*
    Waiting Sender

        A Goroutine or thread waiting on data to be accepted by a Send Channel.
*/
template<class T>
class Waiting_sender : boost::equality_comparable<Waiting_sender<T>> {
public:
    // Construct
    Waiting_sender(Goroutine::Handle, const T* rvaluep);
    Waiting_sender(Goroutine::Handle, T* lvaluep);
    Waiting_sender(condition_variable* sysreadyp, const T* rvaluep);
    Waiting_sender(condition_variable* sysreadyp, T* lvaluep);

    // Observers
    Goroutine::Handle   goroutine() const;
    condition_variable* system_signal() const;

    // Synchronization
    void release(T* receivedp) const;
    void release(Channel_buffer<T>*) const;

    // Comparisons
    template<class U> friend bool operator==(const Waiting_sender<U>&, const Waiting_sender<U>&);

private:
    // Data
    Goroutine::Handle   g;
    condition_variable* readyp;
    const T*            rvalp;
    T*                  lvalp;
};


/*
    Waiting Receiver

        A Goroutine or thread waiting on data from a Receive Channel.
*/
template<class T>
class Waiting_receiver : boost::equality_comparable<Waiting_receiver<T>> {
public:
    // Construct
    Waiting_receiver(Goroutine::Handle, T* valuep);
    Waiting_receiver(condition_variable* sysreadyp, T* valuep);

    // Observers
    Goroutine::Handle   goroutine() const;
    condition_variable* system_signal() const;

    // Synchronization
    template<class U> void release(U* valuep) const;

    // Comparisons
    template<class U> friend bool operator==(const Waiting_receiver<U>&, const Waiting_receiver<U>&);

private:
    // Data
    Goroutine::Handle   g;
    condition_variable* readyp;
    T*                  valp;
};


/*
    Wait Queue

        A queue of waiting Goroutines or threads.
*/
template<class T>
class Wait_queue {
public:
    // Names/Types
    typedef T Waiter;

    // Size and Capacity
    bool is_empty() const;

    // Queue Operations
    void    push(const Waiter&);
    Waiter  pop();
    bool    find(const Waiter&) const;

private:
    // Data
    std::deque<T> ws;
};


/*
    Work Queue
*/
class Workqueue {
public:
    // Queue Operations
    void                push(Goroutine&&);
    optional<Goroutine> pop();
    bool                try_push(Goroutine&&);
    optional<Goroutine> try_pop();
    void                interrupt();

private:
    // Names/Types
    using Mutex = std::mutex;
    using Lock  = std::unique_lock<Mutex>;

    class Goroutine_queue {
    public:
        void        push(Goroutine&&);
        Goroutine   pop();
        bool        is_empty() const;

    private:
        // Data
        std::deque<Goroutine> elems;
    };

    // Queue Operations
    static void push(Mutex&, Goroutine&&, Goroutine_queue*);

    // Data
    Goroutine_queue     q;
    bool                is_interrupt{false};
    Mutex               mutex;
    condition_variable  ready;
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
    void                push(Goroutine&&);
    optional<Goroutine> pop(Size preferred);
    void                interrupt();

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


}   // Implementation Details


/*
    Channel Operation

    A Channel Operation can be selected for execution (perhaps from a set of
    alternatives) or enqueued on the channel.
*/
class Channel_operation : boost::totally_ordered<Channel_operation> {
public:
    // Names/Types
    class Interface;
    enum Type { none, send, receive };

    // Construct/Move/Copy/Destroy
    Channel_operation();
    Channel_operation(Interface* channelp, const void* rvaluep); // send
    Channel_operation(Interface* channelp, Type, void* lvaluep); // send/receive

    // Comparisons
    friend bool operator==(const Channel_operation&, const Channel_operation&);
    friend bool operator< (const Channel_operation&, const Channel_operation&);
    
private:
    // Selection
    bool is_ready() const;
    void execute();
    void enqueue(Goroutine::Handle);

    // Friends
    friend class Detail::Channel_alternatives;

    // Data
    Type            kind;
    Interface*      chanp;
    const void*     rvalp;
    void*           lvalp;
    Channel_size    pos;
};


/*
    Channel Operation Selection
*/
template<Channel_size N> Channel_select_awaitable   select(Channel_operation (&ops)[N]);
template<Channel_size N> Channel_size               try_select(const Channel_operation (&ops)[N]);


/*
    Channel Operation Interface
*/
class Channel_operation::Interface {
public:
    // Copy/Move/Destory
    Interface() = default;
    Interface(const Interface&) = delete;
    Interface& operator=(const Interface&) = delete;
    virtual ~Interface() = default;

    // Send
    virtual bool is_sendable() const = 0;
    virtual void ready_send(void* valuep) = 0;
    virtual void ready_send(const void* valuep) = 0;
    virtual void enqueue_send(Goroutine::Handle, void* valuep) = 0;
    virtual void enqueue_send(Goroutine::Handle, const void* valuep) = 0;

    // Receive
    virtual bool is_receivable() const = 0;
    virtual void ready_receive(void* valuep) = 0;
    virtual void enqueue_receive(Goroutine::Handle, void* valuep) = 0;

    // Synchronization
    virtual void lock() = 0;
    virtual void unlock() = 0;
};


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
    template<class U> friend Channel<U> make_channel(Channel_size capacity);
    Channel& operator=(Channel);
    template<class U> friend void swap(Channel<U>&, Channel<U>&);

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
    Channel_operation make_sendable(const T&) const;
    Channel_operation make_sendable(T&&) const;
    Channel_operation make_receivable(T*) const;
    
    // Conversions
    explicit operator bool() const;

    // Comparisons
    template<class U> friend bool operator==(const Channel<U>&, const Channel<U>&);
    template<class U> friend bool operator< (const Channel<U>&, const Channel<U>&);

private:
    // Names/Types
    class Impl final : public Channel_operation::Interface {
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

        // Operation Selection
        Channel_operation make_sendable(const T* valuep);
        Channel_operation make_sendable(T* valuep);
        Channel_operation make_receivable(T* valuep);

        // Operation Implementation
        bool is_sendable() const override;
        void ready_send(const void* rvaluep) override;
        void ready_send(void* lvaluep) override;
        void enqueue_send(Goroutine::Handle, const void* rvaluep) override;
        void enqueue_send(Goroutine::Handle, void* lvaluep) override;
        bool is_receivable() const override;
        void ready_receive(void* valuep) override;
        void enqueue_receive(Goroutine::Handle, void* valuep) override;

        // Operation Synchronization
        void lock() override;
        void unlock() override;

    private:
        // Names/Types
        using Buffer            = Detail::Channel_buffer<T>;
        using Waiting_sender    = Detail::Waiting_sender<T>;
        using Waiting_receiver  = Detail::Waiting_receiver<T>;
        using Sender_queue      = Detail::Wait_queue<Waiting_sender>;
        using Receiver_queue    = Detail::Wait_queue<Waiting_receiver>;
        using Mutex             = std::mutex;
        using Lock              = std::unique_lock<Mutex>;

        // Operation Implementation
        template<class U> void  ready_send(U* valuep);
        template<class U> void  enqueue_send(Goroutine::Handle, U* valuep);
        void                    ready_receive(T* valuep);
        void                    enqueue_receive(Goroutine::Handle g, T* valuep);

        // Coordination
        static void                     pop_value(Buffer*, T* valuep, Sender_queue*);
        template<class U> static void   release_receiver(Receiver_queue*, U* valuep);
        static void                     release_sender(Sender_queue*, T* valuep);
        static void                     release_sender(Sender_queue*, optional<T>* valuepp);
        template<class U> static void   wait_for_receiver(Lock&, Sender_queue*, U* valuep);
        static void                     wait_for_sender(Lock&, Receiver_queue*, T* valuep);

        // Data
        Buffer          buffer;
        Sender_queue    senderq;
        Receiver_queue  receiverq;
        Mutex           mutex;
    };

    using Impl_ptr = std::shared_ptr<Impl>;

    // Friends
    template<class U> friend class Send_channel;
    template<class U> friend class Receive_channel;

    // Construct
    Channel(Impl_ptr);

    // Data
    Impl_ptr pimpl;
};


/*
    Channel Construction
*/
template<class T> Channel<T> make_channel(Channel_size capacity=0);


/*
    Channel Send Awaitable
*/
template<class T>
class Channel<T>::Send_awaitable {
public:
    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Goroutine::Handle);
    void await_resume();

private:
    // Friends
    friend class Impl;

    // Construct
    template<class U> Send_awaitable(Impl*, U* valuep);

    // Data
    Channel_operation ops[1];
};


/*
    Channel Receive Awaitable
*/
template<class T>
class Channel<T>::Receive_awaitable {
public:
    // Awaitable Operations
    bool    await_ready();
    bool    await_suspend(Goroutine::Handle);
    T&&     await_resume();

private:
    // Friends
    friend class Impl;

    // Construct
    explicit Receive_awaitable(Impl*);

    // Data
    T                   value;
    Channel_operation   ops[1];
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
    template<class U> friend void swap(Send_channel<U>&, Send_channel<U>&);

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
    Channel_operation make_sendable(const T&) const;
    Channel_operation make_sendable(T&&) const;

    // Conversions
    Send_channel(const Channel<T>&);
    Send_channel& operator=(const Channel<T>&);
    explicit operator bool() const;

    // Comparisons
    template<class U> friend bool operator==(const Send_channel<U>&, const Send_channel<U>&);
    template<class U> friend bool operator< (const Send_channel<U>&, const Send_channel<U>&);

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
    template<class U> friend void swap(Receive_channel<T>&, Receive_channel<T>&);

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable   receive() const;
    optional<T> try_receive() const;
    T           sync_receive() const;

    // Selection
    Channel_operation make_receivable(T*);

    // Conversions
    Receive_channel(const Channel<T>&);
    Receive_channel& operator=(const Channel<T>&);
    explicit operator bool() const;

    // Comparisons
    template<class U> friend bool operator==(const Receive_channel<U>&, const Receive_channel<U>&);
    template<class U> friend bool operator< (const Receive_channel<U>&, const Receive_channel<U>&);

private:
    // Data
    typename Channel<T>::Impl_ptr pimpl;
};


/*
    Channel Selection Awaitable
*/
class Channel_select_awaitable {
public:
    // Awaitable Operations
    bool            await_ready();
    bool            await_suspend(Goroutine::Handle);
    Channel_size    await_resume();

private:
    // Friends
    template<Channel_size N> friend Channel_select_awaitable select(Channel_operation (&ops)[N]);

    // Construct
    template<Channel_size N> Channel_select_awaitable(Channel_operation (&ops)[N]);

    // Data
    Detail::Channel_alternatives alternatives;
};


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

