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
#include <atomic>
#include <cassert>
#include <cstddef>
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
using Channel_size = std::ptrdiff_t;
using boost::optional;


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
    class Final_suspend;
    using Initial_suspend = std::experimental::suspend_always;

    class Promise {
    public:
        // Construct
        Promise();
    
        // Coroutine GoFunctions
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
    class Interface;
    class Awaitable_copy;
    class Awaitable_move;
    using Interface_ptr = std::shared_ptr<Interface>;
    using Value         = T;

    // Construct/Move/Copy
    Send_channel(Interface_ptr = Interface_ptr());
    Send_channel& operator=(Send_channel);
    template<class U> friend void swap(Send_channel<U>&, Send_channel<U>&);

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable_copy  send(const T&) const;
    Awaitable_move  send(T&&) const;
    bool            try_send(const T&) const;
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

    // Size and Capacity
    virtual Channel_size size() const = 0;
    virtual Channel_size capacity() const = 0;

    // Channel Operations
    virtual bool send(const T*, Goroutine::Handle sender) = 0;
    virtual bool send(T*, Goroutine::Handle sender) = 0;
    virtual bool try_send(const T&) = 0;
    virtual void sync_send(const T&) = 0;
    virtual void sync_send(T&&) = 0;
};


/*
    Send Channel Awaitable Copy
*/
template<class T>
class Send_channel<T>::Awaitable_copy {
public:
    // Construct
    Awaitable_copy(Interface*, const T* argp);

    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Goroutine::Handle sender);
    void await_resume();

private:
    // Data
    Interface*  channelp;
    const T*    valuep;
};


/*
    Send Channel Awaitable Move
*/
template<class T>
class Send_channel<T>::Awaitable_move {
public:
    // Construct
    Awaitable_move(Interface*, T&& arg);

    // Awaitable Operations
    bool await_ready();
    bool await_suspend(Goroutine::Handle sender);
    void await_resume();

private:
    // Data
    Interface*  channelp;
    T&          value;
};


/*
    Receive Channel
*/
template<class T>
class Receive_channel : boost::equality_comparable<Receive_channel<T>> {
public:
    // Names/Types
    class Awaitable;
    class Interface;
    using Interface_ptr = std::shared_ptr<Interface>;
    using Value         = T;

    // Construct/Move/Copy
    Receive_channel(Interface_ptr = Interface_ptr());
    Receive_channel& operator=(Receive_channel);
    template<class U> friend void swap(Receive_channel<T>&, Receive_channel<T>&);

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable   receive() const;
    optional<T> try_receive() const;
    T           sync_receive() const;

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
    virtual bool        receive(T* datap, Goroutine::Handle receiver) = 0;
    virtual optional<T> try_receive() = 0;
    virtual T           sync_receive() = 0;
};


/*
    Receive Channel Awaitable
*/
template<class T>
class Receive_channel<T>::Awaitable {
public:
    // Construct
    explicit Awaitable(Interface*);

    // Awaitable Operations
    bool    await_ready();
    bool    await_suspend(Goroutine::Handle receiver);
    T&&     await_resume();

private:
    // Data
    Interface*  channelp;
    T           value;
};


/*
    Channel
*/
template<class T>
class Channel : boost::equality_comparable<Channel<T>> {
public:
    // Names/Types
    class Interface;
    using Interface_ptr         = std::shared_ptr<Interface>;
    using Awaitable_send_copy   = typename Send_channel<T>::Awaitable_copy;
    using Awaitable_send_move   = typename Send_channel<T>::Awaitable_move;
    using Awaitable_receive     = typename Receive_channel<T>::Awaitable;
    using Value                 = T;

    // Construct/Move/Copy
    Channel(Interface_ptr = Interface_ptr());
    Channel& operator=(Channel);
    template<class U> friend void swap(Channel<U>&, Channel<U>&);

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    Awaitable_send_copy send(const T&) const;
    Awaitable_send_move send(T&& x) const;
    Awaitable_receive   receive() const;
    bool                try_send(const T&) const;
    optional<T>         try_receive() const;
    void                sync_send(const T&) const;
    void                sync_send(T&&) const;
    T                   sync_receive() const;
    
    // Conversions
    operator Send_channel<T>() const;
    operator Receive_channel<T>() const;
    explicit operator bool() const;

    // Comparisons
    template<class U> friend bool operator==(const Channel<U>&, const Channel<U>&);

private:
    // Data
    Interface_ptr ifacep;
};


// Channel Factory
template<class T> Channel<T> make_channel(Channel_size capacity=0);


/*
    Channel Interface
*/
template<class T>
class Channel<T>::Interface
    : public Send_channel<T>::Interface
    , public Receive_channel<T>::Interface {
public:
};


/*
    Channel Implementation
*/
template<class M, class T = typename M::Value>
class Channel_impl : public Channel<T>::Interface {
public:
    // Names/Types
    using Model = M;
    using Value = T;

    // Construct
    Channel_impl();
    template<class Mod> explicit Channel_impl(Mod&& model);
    template<class... Args> explicit Channel_impl(Args&&...);

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    bool        send(const T*, Goroutine::Handle sender) override;
    bool        send(T*, Goroutine::Handle sender) override;
    bool        receive(T*, Goroutine::Handle receiver) override;
    optional<T> try_receive() override;
    bool        try_send(const T&) override;
    void        sync_send(const T&) override;
    void        sync_send(T&&) override;
    T           sync_receive() override;

private:
    // Data
    M chan;
};


// Channel Implementation Factories
template<class M> std::shared_ptr<Channel_impl<M>>                  make_channel_impl();
template<class M, class N> std::shared_ptr<Channel_impl<M>>         make_channel_impl(N&& model);
template<class M, class... Args> std::shared_ptr<Channel_impl<M>>   make_channel_impl(Args&&...);


/*
    Implementation Details
*/
namespace Detail {


/*
    Names/Types
*/
using std::condition_variable;
using std::tuple;


/*
    Waiting Sender

        A Goroutine or thread waiting on data to be accepted by a Send Channel.
*/
template<class T>
class Waiting_sender : boost::equality_comparable<Waiting_sender<T>> {
public:
    // Construct
    Waiting_sender(Goroutine::Handle, const T* valuep);
    Waiting_sender(Goroutine::Handle, T* valuep);
    Waiting_sender(condition_variable*, T* valuep);
    Waiting_sender(condition_variable*, const T* valuep);

    // Observers
    Goroutine::Handle   goroutine() const;
    condition_variable* signal() const;

    // Waiting
    void release(T* valuep) const;

    // Comparisons
    friend bool operator==(const Waiting_sender<T>&, const Waiting_sender<T>&);

private:
    // Data
    Goroutine::Handle   g;
    condition_variable* signalp{nullptr};
    const T*            readonlyp; // readonly or movable, but not both
    T*                  movablep;
};


/*
    Waiting Receiver

        A Goroutine or thread waiting on data from a Receive Channel.
*/
template<class T>
class Waiting_receiver {
public:
    // Construct
    Waiting_receiver(Goroutine::Handle, T* datap);
    Waiting_receiver(condition_variable* threadp, T* datp);

    // Observers
    Goroutine::Handle   goroutine() const;
    condition_variable* thread() const;

    // Waiting
    template<class U> void release(U&& value) const;

private:
    // Data
    Goroutine::Handle   g;
    condition_variable* threadsigp{nullptr};
    T*                  recvbufp;
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
    Basic Channel
*/
template<class T>
class Basic_channel {
public:
    // Names/Types
    using Value = T;

    // Construct
    explicit Basic_channel(Channel_size maxsize);

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    template<class U> bool  send(U* valuep, Goroutine::Handle sender);
    bool                    receive(T*, Goroutine::Handle receiver);
    optional<T>             try_receive();
    bool                    try_send(const T&);
    template<class U>  void sync_send(U&&);
    T                       sync_receive();

private:
    // Names/Types
    using Sender            = Waiting_sender<T>;
    using Receiver          = Waiting_receiver<T>;
    using Sender_queue      = Wait_queue<Sender>;
    using Receiver_queue    = Wait_queue<Receiver>;
    using Mutex             = std::mutex;
    using Lock              = std::unique_lock<Mutex>;

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
        void                    pop(T*);
    
    private:
        // Data
        std::queue<T>   q;
        Channel_size    sizemax;
    };
    
    template<class W, class Q> struct is_waiter_removed {
        // Names/Types
        is_waiter_removed(const W&, const Q&);
        bool operator()() const;
        const W& w;
        const Q& q;
    };

    // Syncronization
    template<class W, class Q> static is_waiter_removed<W,Q>    is_removed(const W&, const Q&);
    static T                                                    pop(Buffer*);
    template<class U> static void                               release(Receiver_queue*, U&& value);
    static void                                                 release(Sender_queue*, T* valuep);
    static T                                                    release(Sender_queue* waitqp);
    static void                                                 suspend(Goroutine::Handle, T* valuep, Receiver_queue*);
    template<class U> static void                               suspend(Goroutine::Handle, U* valuep, Sender_queue*);
    template<class U> static void                               sender_wait(Lock&, Sender_queue*, U* valuep);
    static void                                                 receiver_wait(Lock&, Receiver_queue*, T* valuep);

    // Data
    Buffer          buffer;
    Sender_queue    senderq;
    Receiver_queue  receiverq;
    Mutex           mutex;
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


}   // Implementation Details


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

