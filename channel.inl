//  $IAPPA_COPYRIGHT:2008$
//  $CUSTOM_HEADER$

//
//  isptech/concurrency/channel.inl
//

//
//  IAPPA CM Revision # : $Revision: 1.3 $
//  IAPPA CM Tag        : $Name:  $
//  Last user to change : $Author: hickmjg $
//  Date of change      : $Date: 2008/12/23 12:43:48 $
//  File Path           : $Source: //ftwgroups/data/IAPPA/CVSROOT/isptech/concurrency/channel.inl,v $
//
//  CAUTION:  CONTROLLED SOURCE.  DO NOT MODIFY ANYTHING ABOVE THIS LINE.
//


/*
    Information and Sensor Processing Technology Concurrency Library
*/
namespace Isptech       {
namespace Concurrency   {


/*
    Goroutine
*/
inline
Goroutine::Goroutine(Handle h)
    : coro{h}
    , isowner{true}
{
}


inline
Goroutine::Goroutine(Goroutine&& other)
{
    steal(&other);
}


inline Goroutine&
Goroutine::operator=(Goroutine&& other)
{
    destroy(this);
    steal(&other);
    return *this;
}


inline
Goroutine::~Goroutine()
{
    destroy(this);
}


inline void
Goroutine::destroy(Goroutine* gp)
{
    if (gp->coro && gp->isowner)
        gp->coro.destroy();
}


inline Goroutine::Handle
Goroutine::handle() const
{
    return coro;
}


inline bool
Goroutine::is_owner() const
{
    return isowner;
}


inline
Goroutine::operator bool() const
{
    return coro ? true : false;
}


inline void
Goroutine::release()
{
    coro    = nullptr;
    isowner = false;
}


inline void
Goroutine::reset(Handle h)
{
    destroy(this);
    coro = h;
    isowner = true;
}


inline void
Goroutine::run()
{
    coro.resume();
    if (!coro.promise().is_done())
        release();
}


inline void
Goroutine::steal(Goroutine* otherp)
{
    coro    = otherp->coro;
    isowner = otherp->isowner;
    otherp->release();
}


inline bool
operator==(const Goroutine& x, const Goroutine& y)
{
    if (x.coro != y.coro) return false;
    if (x.isowner != y.isowner) return false;
    return true;
}


inline bool
Goroutine::Final_suspend::await_ready()
{
    return false;
}


inline void
Goroutine::Final_suspend::await_resume()
{
}


inline bool
Goroutine::Final_suspend::await_suspend(Goroutine::Handle coro)
{
    coro.promise().done();
    return true;
}


/*
    Goroutine Promise
*/
inline
Goroutine::Promise::Promise()
    : isdone{false}
{
}


inline
Goroutine::Promise::~Promise()
{
    const char* msg = "We finally got here, so take a look around.";
}


inline void
Goroutine::Promise::done()
{
    isdone = true;
}


inline Goroutine::Final_suspend
Goroutine::Promise::final_suspend() const
{
    return Final_suspend{};
}


inline Goroutine
Goroutine::Promise::get_return_object()
{
    return Goroutine(Handle::from_promise(*this));
}


inline suspend_always
Goroutine::Promise::initial_suspend() const
{
    return suspend_always{};
}


inline bool
Goroutine::Promise::is_done() const
{
    return isdone;
}


inline void
Goroutine::Promise::return_void()
{
}


/*
    Receive Channel
*/
template<class T>
inline
Receive_channel<T>::Receive_channel(Interface_ptr ip)
    : ifacep{std::move(ip)}
{
}


template<class T>
inline Channel_size
Receive_channel<T>::capacity() const
{
    return ifacep->capacity();
}


template<class T>
inline Receive_channel<T>&
Receive_channel<T>::operator=(Receive_channel other)
{
    ifacep = std::move(other.ifacep);
    return *this;
}


template<class T>
inline
Receive_channel<T>::operator bool() const
{
    return ifacep;
}


template<class T>
inline typename Receive_channel<T>::Awaitable_receive
Receive_channel<T>::receive() const
{
    return Awaitable_receive(ifacep.get());
}


template<class T>
inline Channel_size
Receive_channel<T>::size() const
{
    return ifacep->size();
}


template<class T>
inline T
Receive_channel<T>::sync_receive() const
{
    T x;
    ifacep->sync_receive(&x);
    return x;
}


template<class T>
inline tuple<T,bool>
Receive_channel<T>::try_receive() const
{
    return ifacep->try_receive();
}


template<class T>
inline bool
operator==(const Receive_channel<T>& x, const Receive_channel<T>& y)
{
    if (x.ifacep != y.ifacep) return false;
    return true;
}


template<class T>
inline void
swap(Receive_channel<T>& x, Receive_channel<T>& y)
{
    using std::swap;

    swap(x.ifacep, y.ifacep);
}


/*
    Awaitable Receive Channel Receive
*/
template<class T>
inline
Receive_channel<T>::Awaitable_receive::Awaitable_receive(Interface* chanp)
    : channelp{chanp}
{
}


template<class T>
inline bool
Receive_channel<T>::Awaitable_receive::await_ready()
{
    return false;
}


template<class T>
inline bool
Receive_channel<T>::Awaitable_receive::await_suspend(Goroutine::Handle receiver)
{
    return !channelp->receive(&data, receiver);
}

template<class T>
inline T&&
Receive_channel<T>::Awaitable_receive::await_resume()
{
    return std::move(data);
}


/*
    Send Channel
*/
template<class T>
inline
Send_channel<T>::Send_channel(Interface_ptr p)
    : ifacep{std::move(p)}
{
}


template<class T>
inline Channel_size
Send_channel<T>::capacity() const
{
    return ifacep->capacity();
}


template<class T>
inline Send_channel<T>&
Send_channel<T>::operator=(Send_channel other)
{
    ifacep = std::move(other.ifacep);
    return *this;
}


template<class T>
inline
Send_channel<T>::operator bool() const
{
    return ifacep;
}


template<class T>
inline typename Send_channel<T>::Awaitable_send
Send_channel<T>::send(const T& x) const
{
    return Awaitable_send(ifacep.get(), x);
}


template<class T>
inline typename Send_channel<T>::Awaitable_send
Send_channel<T>::send(T&& x) const
{
    return Awaitable_send(ifacep.get(), std::move(x));
}


template<class T>
inline Channel_size
Send_channel<T>::size() const
{
    return ifacep->size();
}


template<class T>
inline void
Send_channel<T>::sync_send(const T& x) const
{
    T xx = x;
    ifacep->sync_send(&x);
}


template<class T>
inline void
Send_channel<T>::sync_send(T&& x) const
{
    ifacep->sync_send(&x);
}


template<class T>
inline bool
Send_channel<T>::try_send(const T& x) const
{
    return ifacep->try_send(x);
}


template<class T>
inline bool
Send_channel<T>::try_send(T&& x) const
{
    using std::move;

    return ifacep->try_send(move(x));
}


template<class T>
inline void
swap(Send_channel<T>& x, Send_channel<T>& y)
{
    swap(x.ifacep, y.ifacep);
}


template<class T>
inline bool
operator==(const Send_channel<T>& x, const Send_channel<T>& y)
{
    if (x.ifacep == y.ifacep) return true;
    return false;
}


/*
    Awaitable Send Channel Send Operation
*/
template<class T>
inline
Send_channel<T>::Awaitable_send::Awaitable_send(Interface* chanp, const T& dat)
    : channelp{chanp}
    , data{dat}
{
}


template<class T>
inline
Send_channel<T>::Awaitable_send::Awaitable_send(Interface* chanp, T&& dat)
    : channelp{chanp}
    , data{std::move(dat)}
{
}


template<class T>
inline bool
Send_channel<T>::Awaitable_send::await_ready()
{
    return false;
}


template<class T>
inline bool
Send_channel<T>::Awaitable_send::await_suspend(Goroutine::Handle sender)
{
    return !channelp->send(&data, sender);
}


template<class T>
inline void
Send_channel<T>::Awaitable_send::await_resume()
{
}


/*
    Channel
*/
template<class T>
inline
Channel<T>::Channel(Interface_ptr facep)
    : ifacep{std::move(facep)}
{
}


template<class T>
inline Channel_size
Channel<T>::capacity() const
{
    return ifacep->capacity();
}


template<class T>
inline Channel<T>&
Channel<T>::operator=(Channel other)
{
    ifacep = std::move(other.ifacep);
    return *this;
}


template<class T>
inline
Channel<T>::operator bool() const
{
    return ifacep;
}


template<class T>
inline
Channel<T>::operator Receive_channel<T>() const
{
    return ifacep;
}


template<class T>
inline
Channel<T>::operator Send_channel<T>() const
{
    return ifacep;
}


template<class T>
inline typename Channel<T>::Awaitable_receive
Channel<T>::receive() const
{
    return Awaitable_receive(ifacep.get());
}


template<class T>
inline typename Send_channel<T>::Awaitable_send
Channel<T>::send(const T& x) const
{
    return Awaitable_send(ifacep.get(), x);
}


template<class T>
inline Channel_size
Channel<T>::size() const
{
    return ifacep->size();
}


template<class T>
inline T
Channel<T>::sync_receive() const
{
    T x;
    ifacep->sync_receive(&x);
    return x;
}


template<class T>
inline void
Channel<T>::sync_send(const T& x) const
{
    T xx = x;
    ifacep->sync_send(&x);
}


template<class T>
inline void
Channel<T>::sync_send(T&& x) const
{
    ifacep->sync_send(&x);
}


template<class T>
inline tuple<T,bool>
Channel<T>::try_receive() const
{
    return ifacep->try_receive();
}


template<class T>
inline bool
Channel<T>::try_send(const T& x) const
{
    return ifacep->try_send(x);
}


template<class T>
inline bool
Channel<T>::try_send(T&& x) const
{
    using std::move;

    return ifacep->try_send(move(x));
}


template<class T>
inline bool
operator==(const Channel<T>& x, const Channel<T>& y)
{
    if (x.ifacep != y.ifacep) return false;
    return true;
}


template<class T>
inline void
swap(Channel<T>& x, Channel<T>& y)
{
    using std::swap;

    swap(x.ifacep, y.ifacep);
}


/*
    Implementation Details
*/
namespace Detail {
    
    
/*
    Channel Waiter
*/
template<class T>
inline
Channel_waiter<T>::Channel_waiter(Goroutine::Handle h, T* datp)
    : datap{datp}
    , g{h}
{
}


template<class T>
inline
Channel_waiter<T>::Channel_waiter(condition_variable* foreignerp, T* datp)
    : datap{datp}
    , g{nullptr}
    , foreignp{foreignerp}
{
}


template<class T>
inline condition_variable*
Channel_waiter<T>::foreign() const
{
    return foreignp;
}


template<class T>
inline Goroutine::Handle
Channel_waiter<T>::internal() const
{
    return g;
}


template<class T>
inline void
Channel_waiter<T>::resume()
{
    if (g)
        scheduler.resume(g);
    else
        foreignp->notify_one();
}


/*
    Channel Waiter Queue
*/
template<class T>
inline bool
Channel_waiterqueue<T>::is_empty() const
{
    return waiters.empty();
}


template<class T>
inline bool
Channel_waiterqueue<T>::find(const condition_variable* foreignp) const
{
    using std::find_if;

    const auto p = find_if(waiters.begin(), waiters.end(), waiter_equal(foreignp));
    return p != waiters.end();
}


template<class T>
inline Channel_waiter<T>
Channel_waiterqueue<T>::pop()
{
    const Channel_waiter<T> w = waiters.front();

    waiters.pop_front();
    return w;
}


template<class T>
inline void
Channel_waiterqueue<T>::push(const Channel_waiter<T>& w)
{
    waiters.push_back(w);
}

template<class T>
inline
Channel_waiterqueue<T>::waiter_equal::waiter_equal(const condition_variable* cvp)
    : foreignp{cvp}
{
}


template<class T>
inline bool
Channel_waiterqueue<T>::waiter_equal::operator()(const Channel_waiter<T>& w) const
{
    return w.foreign() == foreignp;
}


}   // Implementation Details


/*
    Channel Implementation
*/
template<class T, class U = typename T::Value>
struct Channel_impl : Channel<U>::Interface {
    // Names/Types
    using Model = T;
    using Value = U;

    // Construct
    Channel_impl();
    explicit Channel_impl(T&& chan);
    template<class... Args> explicit Channel_impl(Args&&...);

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    bool            send(U*, Goroutine::Handle sender);
    bool            receive(U*, Goroutine::Handle receiver);
    tuple<U,bool>   try_receive();
    bool            try_send(const U&);
    bool            try_send(U&&);
    void            sync_send(U* datap);
    void            sync_receive(U* datap);

    // Data
    T model;
};


template<class T, class U>
inline
Channel_impl<T,U>::Channel_impl()
{
}


template<class T, class U>
inline
Channel_impl<T,U>::Channel_impl(T&& chan)
    : model{std::forward<T>(chan)}
{
}


template<class T, class U>
template<class... Args>
inline
Channel_impl<T,U>::Channel_impl(Args&&... args)
    : model{std::forward<Args...>(args...)}
{
}


template<class T, class U>
Channel_size
Channel_impl<T,U>::capacity() const
{
    return model.capacity();
}


template<class T, class U>
bool
Channel_impl<T,U>::receive(U* datap, Goroutine::Handle receiver)
{
    return model.receive(datap, receiver);
}


template<class T, class U>
bool
Channel_impl<T,U>::send(U* datap, Goroutine::Handle sender)
{
    return model.send(datap, sender);
}


template<class T, class U>
Channel_size
Channel_impl<T,U>::size() const
{
    return model.size();
}


template<class T, class U>
void
Channel_impl<T,U>::sync_receive(U* datap)
{
    model.sync_receive(datap);
}


template<class T, class U>
void
Channel_impl<T,U>::sync_send(U* datap)
{
    model.sync_send(datap);
}


template<class T, class U>
tuple<U,bool>
Channel_impl<T,U>::try_receive()
{
    return model.try_receive();
}


template<class T, class U>
bool
Channel_impl<T,U>::try_send(const U& x)
{
    return model.try_send(x);
}


template<class T, class U>
bool
Channel_impl<T,U>::try_send(U&& x)
{
    using std::move;

    return model.try_send(move(x));
}


/*
    Channel Implementation Construction
*/
template<class T>
inline std::shared_ptr<Channel_impl<T>>
make_channel_impl()
{
    return std::make_shared<Channel_impl<T>>();
}


template<class T, class... Args>
inline std::shared_ptr<Channel_impl<T>>
make_channel_impl(Args&&... args)
{
    using std::forward;
    using std::make_shared;

    return make_shared<Channel_impl<T>>(forward<Args...>(args...));
}


/*
    Buffered Channel

        (Does not currently work for buffer capacity == 0; use Unbuffered_channel for that.)

        TODO:   Address redundant implementations in send/try_send, receive/try_receive, and
                try_send's.
*/
template<class T>
class Buffered_channel {
public:
    // Names/Types
    using Interface = typename Channel<T>::Interface;
    using Value     = T;

    // Construct
    explicit Buffered_channel(Channel_size bufsize);

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    bool            send(T* datap, Goroutine::Handle sender);
    bool            receive(T* datap, Goroutine::Handle receiver);
    tuple<T,bool>   try_receive();
    bool            try_send(const T&);
    bool            try_send(T&&);
    void            sync_send(T* datap);
    void            sync_receive(T* datap);

private:
    // Names/Types
    using Queue         = std::queue<T>;
    using Mutex         = std::mutex;
    using Lock          = std::unique_lock<Mutex>;
    using Waiter        = Detail::Channel_waiter<T>;
    using Waiter_queue  = Detail::Channel_waiterqueue<T>;

    // Data
    Queue           buffer;
    Channel_size    maxsize;
    Waiter_queue    senders;
    Waiter_queue    receivers;
    Mutex           mutex;
};


template<class T>
inline
Buffered_channel<T>::Buffered_channel(Channel_size bufsize)
    : maxsize{bufsize}
{
    assert(bufsize > 0);
}


template<class T>
inline Channel_size
Buffered_channel<T>::capacity() const
{
    return maxsize;
}


template<class T>
inline bool
Buffered_channel<T>::receive(T* datap, Goroutine::Handle g)
{
    using std::move;

    bool is_received{true};
    Lock lock{mutex};

    if (!buffer.empty()) {
        *datap = move(buffer.front());
        buffer.pop();

        if (!senders.is_empty()) {
            assert(buffer.size() == maxsize); // senders wait only when buffer is full

            Waiter sender = senders.pop();
            buffer.push(move(*sender.datap));
            sender.resume();
        }
    } else {
        receivers.push(Waiter{g, datap});
        scheduler.suspend(g);
        is_received = false;
    }

    return is_received;
}


template<class T>
inline bool
Buffered_channel<T>::send(T* datap, Goroutine::Handle g)
{
    using std::move;

    bool is_received{true};
    Lock lock{mutex};

    if (!receivers.is_empty()) {
        assert(buffer.size() == 0); // receivers wait only when buffer is empty

        Waiter receiver = receivers.pop();
        *receiver.datap = move(*datap);
        receiver.resume();
    } else if (buffer.size() == maxsize) {
        senders.push(Waiter{g, datap});
        scheduler.suspend(g);
        is_received = false;
    } else {
        buffer.push(move(*datap));
    }

    return is_received;
}


template<class T>
inline Channel_size
Buffered_channel<T>::size() const
{
    return buffer.size();
}


template<class T>
inline void
Buffered_channel<T>::sync_receive(T* datap)
{
    using std::condition_variable;
    using std::move;

    Lock lock{mutex};

    if (!buffer.empty()) {
        *datap = move(buffer.front());
        buffer.pop();

        if (!senders.is_empty()) {
            Waiter sender = senders.pop();
            buffer.push(move(*sender.datap));
            sender.resume();
            assert(buffer.size() == maxsize); // senders only wait when buffer is full
        }
    } else {
        condition_variable self;
        receivers.push(Waiter{&self, datap});
        do {
            self.wait(lock);
        } while (receivers.find(&self));
    }
}


template<class T>
inline void
Buffered_channel<T>::sync_send(T* datap)
{
    using std::condition_variable;
    using std::move;

    Lock lock{mutex};

    if (!receivers.is_empty()) {
        assert(buffer.size() == 0); // receivers only wait when buffer is empty
        Waiter receiver = receivers.pop();
        *receiver.datap = move(*datap);
        receiver.resume();
    } else if (buffer.size() == maxsize) {
        condition_variable self;
        senders.push(Waiter{&self, datap});
        do {
            self.wait(lock);
        } while (senders.find(&self));
    } else {
        buffer.push(move(*datap));
    }
}


template<class T>
inline tuple<T,bool>
Buffered_channel<T>::try_receive()
{
    using std::get;
    using std::move;

    tuple<T,bool>   result;
    Lock            lock{mutex};

    if (!buffer.empty()) {
        get<0>(result) = move(buffer.front());
        get<1>(result) = true;
        buffer.pop();

        if (!senders.is_empty()) {
            Waiter sender = senders.pop();
            buffer.push(move(*sender.datap));
            sender.resume();
            assert(buffer.size() == maxsize); // senders only wait when buffer is full
        }
    }

    return result;
}


template<class T>
inline bool
Buffered_channel<T>::try_send(const T& data)
{
    bool is_received{false};
    Lock lock{mutex};

    if (!receivers.is_empty()) {
        assert(buffer.size() == 0);  // receivers only wait when buffer is empty
        Waiter receiver = receivers.pop();
        *receiver.datap = data;
        receiver.resume();
        is_received = true;
    } else if (buffer.size() < maxsize) {
        buffer.push(data);
        is_received = true;
    }

    return is_received;
}


template<class T>
inline bool
Buffered_channel<T>::try_send(T&& data)
{
    using std::move;

    bool is_received{false};
    Lock lock{mutex};

    if (!receivers.is_empty()) {
        assert(buffer.size() == 0); // receivers only wait when buffer is empty
        Waiter receiver = receivers.pop();
        *receiver.datap = move(data);
        receiver.resume();
        is_received = true;
    } else if (buffer.size() < maxsize) {
        buffer.push(data);
        is_received = true;
    }

    return is_received;
}


/*
    Unbuffered Channel

        TODO:   Address redundant implementations in send/try_send, receive/try_receive, and
                try_send's.
*/
template<class T>
class Unbuffered_channel {
public:
    // Names/Types
    using Interface = typename Channel<T>::Interface;
    using Value     = T;

    // Size and Capacity
    Channel_size size() const;
    Channel_size capacity() const;

    // Channel Operations
    bool            send(T* datap, Goroutine::Handle sender);
    bool            receive(T* datap, Goroutine::Handle receiver);
    tuple<T,bool>   try_receive();
    bool            try_send(const T&);
    bool            try_send(T&&);
    void            sync_send(T* datap);
    void            sync_receive(T* datap);

private:
    // Names/Types
    using Mutex         = std::mutex;
    using Lock          = std::unique_lock<Mutex>;
    using Waiter        = Detail::Channel_waiter<T>;
    using Waiter_queue  = Detail::Channel_waiterqueue<T>;

    // Data
    Waiter_queue    senders;
    Waiter_queue    receivers;
    Mutex           mutex;
};


template<class T>
inline Channel_size
Unbuffered_channel<T>::capacity() const
{
    return 0;
}


template<class T>
inline bool
Unbuffered_channel<T>::receive(T* datap, Goroutine::Handle g)
{
    using std::move;

    bool is_received{true};
    Lock lock{mutex};

    // if a sender is waiting, dequeue it; otherwise, enqueue this receiver
    if (!senders.is_empty()) {
        Waiter sender = senders.pop();
        *datap = move(*sender.datap);
        sender.resume();
    } else {
        receivers.push(Waiter{g, datap});
        scheduler.suspend(g);
        is_received = false;
    }

    return is_received;
}

    
template<class T>
inline bool
Unbuffered_channel<T>::send(T* datap, Goroutine::Handle g)
{
    using std::move;

    bool is_received{true};
    Lock lock{mutex};

    // if a receiver is waiting, dequeue it; otherwise, enqueue this sender
    if (!receivers.is_empty()) {
        Waiter receiver = receivers.pop();
        *receiver.datap = move(*datap);
        receiver.resume();
    } else {
        senders.push(Waiter{g, datap});
        scheduler.suspend(g);
        is_received = false;
    }

    return is_received;
}


template<class T>
inline Channel_size
Unbuffered_channel<T>::size() const
{
    return 0;
}


template<class T>
inline void
Unbuffered_channel<T>::sync_receive(T* datap)
{
    using std::condition_variable;
    using std::move;

    Lock lock{mutex};

    // if a sender is waiting, dequeue it; otherwise, enqueue this receiver
    if (!senders.is_empty()) {
        Waiter sender = senders.pop();
        *datap = move(*sender.datap);
        sender.resume();
    } else {
        condition_variable receiver;
        receivers.push(Waiter{&receiver, datap});
        do {
            receiver.wait(lock);
        } while (receivers.find(&receiver));
    }
}


template<class T>
inline void
Unbuffered_channel<T>::sync_send(T* datap)
{
    using std::condition_variable;
    using std::move;

    Lock lock{mutex};

    // if a receiver is waiting, dequeue it; otherwise, enqueue this sender
    if (!receivers.is_empty()) {
        Waiter receiver = receivers.pop();
        *receiver.datap = move(*datap);
        receiver.resume();
    } else {
        condition_variable sender;
        senders.push(Waiter{&sender, datap});
        do {
            sender.wait(lock);
        } while (senders.find(&sender));
    }
}


template<class T>
inline tuple<T,bool>
Unbuffered_channel<T>::try_receive()
{
    using std::get;
    using std::move;

    tuple<T,bool>   result;
    Lock            lock{mutex};

    if (!senders.is_empty()) {
        Waiter sender = senders.pop();
        get<0>(result) = move(*sender.datap);
        get<1>(result) = true;
        sender.resume();
    }

    return result;
}


template<class T>
inline bool
Unbuffered_channel<T>::try_send(const T& data)
{
    using std::move;

    bool is_received{false};
    Lock lock{mutex};

    // if a receiver is waiting, dequeue it; otherwise, enqueue the sender
    if (!receivers.is_empty()) {
        Waiter receiver = receivers.pop();
        *receiver.datap = data;
        receiver.resume();
        is_received = true;
    }

    return is_received;
}


template<class T>
inline bool
Unbuffered_channel<T>::try_send(T&& data)
{
    using std::move;

    bool is_received{false};
    Lock lock{mutex};

    // if a receiver is waiting, dequeue it; otherwise, enqueue the sender
    if (!receivers.is_empty()) {
        Waiter receiver = receivers.pop();
        *receiver.datap = move(data);
        receiver.resume();
        is_received = true;
    }

    return is_received;
}


/*
    Channel Construction
*/
template<class T>
inline Channel<T>
make_unbuffered_channel()
{
    return Channel<T>(make_channel_impl<Unbuffered_channel<T>>());
}


template<class T>
inline Channel<T>
make_buffered_channel(Channel_size capacity)
{
    return Channel<T>(make_channel_impl<Buffered_channel<T>>(capacity));
}


template<class T>
Channel<T>
make_channel(Channel_size n)
{
    return (n > 0) ? make_buffered_channel<T>(n) : make_unbuffered_channel<T>();
}


/*
    Goroutine Launcher
*/
template<class GoFun, class... ArgTypes>
inline void
go(GoFun&& fun, ArgTypes&&... args)
{
    using std::forward;
    using std::move;

    Goroutine g = fun(forward<ArgTypes>(args)...);
    scheduler.submit(move(g));
}


}  // Concurrency
}  // Isptech

