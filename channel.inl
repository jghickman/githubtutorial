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


inline Goroutine::Initial_suspend
Goroutine::Promise::initial_suspend() const
{
    return Initial_suspend{};
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
inline typename Receive_channel<T>::Awaitable
Receive_channel<T>::receive() const
{
    return Awaitable(ifacep.get());
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
    return ifacep->sync_receive();
}


template<class T>
inline optional<T>
Receive_channel<T>::try_receive() const
{
    return ifacep->try_receive();
}


template<class T>
inline bool
operator==(const Receive_channel<T>& x, const Receive_channel<T>& y)
{
    return x.ifacep != y.ifacep;
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
Receive_channel<T>::Awaitable::Awaitable(Interface* chanp)
    : channelp{chanp}
{
}


template<class T>
inline bool
Receive_channel<T>::Awaitable::await_ready()
{
    return false;
}


template<class T>
inline bool
Receive_channel<T>::Awaitable::await_suspend(Goroutine::Handle waiter)
{
    return !channelp->receive(&data, waiter);
}

template<class T>
inline T&&
Receive_channel<T>::Awaitable::await_resume()
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
inline typename Send_channel<T>::Awaitable_copy
Send_channel<T>::send(const T& x) const
{
    return Awaitable_copy(ifacep.get(), &x);
}


template<class T>
inline typename Send_channel<T>::Awaitable_move
Send_channel<T>::send(T&& x) const
{
    using std::move;
    return Awaitable_move(ifacep.get(), move(x));
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
    ifacep->sync_send(x);
}


template<class T>
inline void
Send_channel<T>::sync_send(T&& x) const
{
    using std::move;
    ifacep->sync_send(move(x));
}


template<class T>
inline bool
Send_channel<T>::try_send(const T& x) const
{
    return ifacep->try_send(x);
}


template<class T>
inline bool
operator==(const Send_channel<T>& x, const Send_channel<T>& y)
{
    return x.ifacep == y.ifacep;
}


template<class T>
inline void
swap(Send_channel<T>& x, Send_channel<T>& y)
{
    swap(x.ifacep, y.ifacep);
}


/*
    Send Channel Copy Awaitable
*/
template<class T>
inline
Send_channel<T>::Awaitable_copy::Awaitable_copy(Interface* chanp, const T* datp
    : channelp{chanp}
    , datap{datp}
{
}


template<class T>
inline bool
Send_channel<T>::Awaitable_copy::await_ready()
{
    return false;
}


template<class T>
inline bool
Send_channel<T>::Awaitable_copy::await_suspend(Goroutine::Handle sender)
{
    return !channelp->send(datap, sender);
}


template<class T>
inline void
Send_channel<T>::Awaitable_copy::await_resume()
{
}


/*
    Send Channel Move Awaitable
*/
template<class T>
inline
Send_channel<T>::Awaitable_move::Awaitable_move(Interface* chanp, T&& dat)
    : channelp{chanp}
    , data{std::move(dat)}
{
}


template<class T>
inline bool
Send_channel<T>::Awaitable_move::await_ready()
{
    return false;
}


template<class T>
inline bool
Send_channel<T>::Awaitable_move::await_suspend(Goroutine::Handle sender)
{
    return !channelp->send(&data, sender);
}


template<class T>
inline void
Send_channel<T>::Awaitable_move::await_resume()
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
inline typename Channel<T>::Awaitable_send_copy
Channel<T>::send(const T& x) const
{
    return Awaitable_send_copy(ifacep.get(), &x);
}


template<class T>
inline typename Channel<T>::Awaitable_send_move
Channel<T>::send(T&& x) const
{
    using std::move;
    return Awaitable_send_move(ifacep.get(), move(x));
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
    return ifacep->sync_receive();
}


template<class T>
inline void
Channel<T>::sync_send(const T& x) const
{
    return ifacep->sync_send(x);
}


template<class T>
inline void
Channel<T>::sync_send(T&& x) const
{
    using std::move;
    return ifacep->sync_send(move(x));
}


template<class T>
inline optional<T>
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
operator==(const Channel<T>& x, const Channel<T>& y)
{
    return x.ifacep != y.ifacep;
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
Channel_waiter<T>::release() const
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


template<class M, class T>
inline
Channel_impl<M,T>::Channel_impl()
{
}


template<class M, class T>
template<class Mod>
inline
Channel_impl<M,T>::Channel_impl(Mod&& model)
    : chan{std::forward<T>(model)}
{
}


template<class M, class T>
template<class... Args>
inline
Channel_impl<M,T>::Channel_impl(Args&&... args)
    : chan{std::forward<Args...>(args...)}
{
}


template<class M, class T>
Channel_size
Channel_impl<M,T>::capacity() const
{
    return chan.capacity();
}


template<class M, class T>
bool
Channel_impl<M,T>::receive(T* datap, Goroutine::Handle receiver)
{
    return chan.receive(datap, receiver);
}


template<class M, class T>
bool
Channel_impl<M,T>::send(const T* datap, Goroutine::Handle sender)
{
    return chan.send(datap, waiter);
}


template<class M, class T>
bool
Channel_impl<M,T>::send(T* datap, Goroutine::Handle sender)
{
    return chan.send(datap, sender);
}


template<class M, class T>
Channel_size
Channel_impl<M,T>::size() const
{
    return chan.size();
}


template<class M, class T>
void
Channel_impl<M,T>::sync_receive()
{
    return chan.sync_receive();
}


template<class M, class T>
void
Channel_impl<M,T>::sync_send(const T& data)
{
    chan.sync_send(data);
}


template<class M, class T>
void
Channel_impl<M,T>::sync_send(T&& data)
{
    chan.sync_send(data);
}


template<class M, class T>
optional<T>
Channel_impl<M,T>::try_receive()
{
    return chan.try_receive();
}


template<class M, class T>
bool
Channel_impl<M,T>::try_send(const T& data)
{
    return chan.try_send(data);
}


/*
    Channel Implementation
*/
template<class M>
inline std::shared_ptr<Channel_impl<M>>
make_channel_impl()
{
    using std::make_shared;

    return make_shared<Channel_impl<M>>();
}


template<class M, class N>
inline std::shared_ptr<Channel_impl<M>>
make_channel_impl(N&& model)
{
    using std::forward;
    using std::make_shared;

    return make_shared<Channel_impl<M>>(forward(model));
}


template<class M, class... Args>
inline std::shared_ptr<Channel_impl<M>>
make_channel_impl(Args&&... args)
{
    using std::forward;
    using std::make_shared;

    return make_shared<Channel_impl<M>>(forward<Args...>(args...));
}


/*
    Implementation Details
*/
namespace Detail {    
    

/*
    Channel Buffer
*/
template<class T>
inline
Channel_buffer<T>::Channel_buffer(Channel_size maxsize)
    : sizemax(maxsize)
{
}


template<class T>
inline bool
Channel_buffer<T>::is_empty() const
{
    return q.empty();
}


template<class T>
inline bool
Channel_buffer<T>::is_full() const
{
    return size() == max_size();
}


template<class T>
inline Channel_size
Channel_buffer<T>::max_size() const
{
    return sizemax;
}


template<class T>
inline void
Channel_buffer<T>::pop(T* xp)
{
    using std::move;

    *xp = move(q.front());
    q.pop();
}


template<class T>
template<class U>
inline void
Channel_buffer<T>::push(U&& x)
{
    using std::forward;

    q.push(forward(x));
}


template<class T>
inline Channel_size
Channel_buffer<T>::size() const
{
    return static_cast<Channel_size>(q.size());
}


/*
    Buffered Channel
*/
template<class T>
inline
Buffered_channel<T>::Buffered_channel(Channel_size bufsize)
    : buffer{bufsize}
{
    assert(bufsize > 0);
}


template<class T>
inline Channel_size
Buffered_channel<T>::capacity() const
{
    return buffer.max_size();
}


template<class T>
inline bool
Buffered_channel<T>::receive(T* datap, Goroutine::Handle receiver)
{
    using std::move;

    bool is_received{true};
    Lock lock{mutex};

    if (!buffer.is_empty()) {
        buffer.pop(datap);
        release(&senderq, &buffer);
    } else {
        suspend(receiver, datap, &receiverq);
        is_received = false;
    }

    return is_received;
}


template<class T>
template<class U>
inline bool
Buffered_channel<T>::release(Receiver_queue* waitqp, U&& data)
{
    using std::forward;

    const Waiting_receiver receiver = waitqp->pop();
    receiver.release(forward(data))
}


template<class T>
inline bool
Buffered_channel<T>::release(Sender_queue* waitqp, Channel_buffer* bufferp)
{
    const Waiting_sender sender = waitqp->pop();
    sender.release(bufferp);
}


template<class T>
inline bool
Buffered_channel<T>::suspend(Goroutine::Handle g, T* datap, Receiver_queue* waitqp)
{
    const Waiting_receiver receiver{g, datap};

    receiver.suspend();
    waitqp->push(receiver);
}


template<class T>
template<class U>
inline bool
Buffered_channel<T>::suspend(Goroutine::Handle waiter, U* datap, Sender_queue* waitqp)
{
    const Waiting_sender sender{waiter, datap};

    sender.suspend();
    waitqp->push(sender);
}


template<class T>
inline bool
Buffered_channel<T>::send(T* datap, Goroutine::Handle sender)
{
    using std::move;

    bool is_sent{true};
    Lock lock{mutex};

    if (!receiverq.is_empty())
        release(&receiverq, move(*datap));
    else if (!buffer.is_full())
        buffer.push(move(*datap));
    else {
        suspend(sender, datap, &senderq)
        is_sent = false;
    }

    return is_sent;
}


template<class T>
inline bool
Buffered_channel<T>::send(const T* datap, Goroutine::Handle sender)
{
    bool is_sent{true};
    Lock lock{mutex};

    if (!receiverq.is_empty())
        release(&receiverq, *datap);
    else if (!buffer.is_full())
        buffer.push(*datap);
    else {
        suspend(sender, datap, &senderq)
        is_sent = false;
    }

    return is_sent;
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

    if (!buffer.is_empty()) {
        *datap = move(buffer.front());
        buffer.pop();

        if (!senderq.is_empty()) {
            Waiter sender = senderq.pop();
            buffer.push(move(*sender.datap));
            sender.resume();

            assert(buffer.is_full()); // senders only wait when buffer is full
        }
    } else {
        condition_variable receiver;
        receiverq.push(Waiter{&receiver, datap});
        do {
            receiver.wait(lock);
        } while (receiverq.find(&receiver));
    }
}


template<class T>
template<class U>
inline void
Buffered_channel<T>::sync_send(U&& data)
{
    using std::condition_variable;
    using std::forward;

    Lock lock{mutex};

    if (!receiverq.is_empty()) {
        assert(buffer.is_empty()); // receivers only wait when buffer is empty

        Waiter receiver = receiverq.pop();
        *receiver.datap = forward(data);
        receiver.resume();
    } else if (buffer.is_full()) {
        condition_variable ownthread;
        senderq.push(Waiter{&ownthread, &data});
        ownthread.wait(lock, is_popped(ownthread, senderq));
    } else {
        buffer.push(move(*datap));
    }
}


template<class T>
inline optional<T>
Buffered_channel<T>::try_receive()
{
    using std::move;

    optional<T> result;
    Lock        lock{mutex};

    if (!buffer.empty()) {
        result = move(buffer.front());
        buffer.pop();

        if (!senderq.is_empty()) {
            Waiter sender = senderq.pop();
            buffer.push(move(*sender.datap));
            sender.resuownthread();

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

    if (!receiverq.is_empty()) {
        assert(buffer.size() == 0);  // receivers only wait when buffer is empty

        Waiter receiver = receiverq.pop();
        *receiver.datap = data;
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
    bool        send(T* datap, Goroutine::Handle sender);
    bool        receive(T* datap, Goroutine::Handle receiver);
    optional<T> try_receive();
    bool        try_send(const T&);
    void        sync_send(T* datap);
    void        sync_receive(T* datap);

private:
    // Names/Types
    using Mutex         = std::mutex;
    using Lock          = std::unique_lock<Mutex>;
    using Waiter        = Detail::Channel_waiter<T>;
    using Waiter_queue  = Detail::Channel_waiterqueue<T>;

    // Data
    Waiter_queue    senderq;
    Waiter_queue    receiverq;
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
    if (!senderq.is_empty()) {
        Waiter sender = senderq.pop();
        *datap = move(*sender.datap);
        sender.resume();
    } else {
        receiverq.push(Waiter{g, datap});
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
    if (!receiverq.is_empty()) {
        Waiter receiver = receiverq.pop();
        *receiver.datap = move(*datap);
        receiver.resume();
    } else {
        senderq.push(Waiter{g, datap});
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

    // If a sender is waiting, dequeue it; otherwise, enqueue this receiver.
    if (!senderq.is_empty()) {
        Waiter sender = senderq.pop();
        *datap = move(*sender.datap);
        sender.resume();
    } else {
        condition_variable receiver;
        receiverq.push(Waiter{&receiver, datap});
        do {
            receiver.wait(lock);
        } while (receiverq.find(&receiver));
    }
}


template<class T>
inline void
Unbuffered_channel<T>::sync_send(T* datap)
{
    using std::condition_variable;
    using std::move;

    Lock lock{mutex};

    // If a receiver is waiting, dequeue it; otherwise, enqueue this sender.
    if (!receiverq.is_empty()) {
        Waiter receiver = receiverq.pop();
        *receiver.datap = move(*datap);
        receiver.resume();
    } else {
        condition_variable sender;
        senderq.push(Waiter{&sender, datap});
        do {
            sender.wait(lock);
        } while (senderq.find(&sender));
    }
}


template<class T>
inline optional<T>
Unbuffered_channel<T>::try_receive()
{
    using std::move;

    optional<T> result;
    Lock        lock{mutex};

    if (!senderq.is_empty()) {
        Waiter sender = senderq.pop();
        result = move(*sender.datap);
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

    // If a receiver is waiting, dequeue it; otherwise, enqueue the sender.
    if (!receiverq.is_empty()) {
        Waiter receiver = receiverq.pop();
        *receiver.datap = data;
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
    return make_channel_impl<Unbuffered_channel<T>>();
}


template<class T>
inline Channel<T>
make_buffered_channel(Channel_size capacity)
{
    return make_channel_impl<Buffered_channel<T>>(capacity);
}


}   // Implementation Details


/*
    Channel Construction
*/
template<class T>
Channel<T>
make_channel(Channel_size n)
{
    using Detail::make_buffered_channel;
    using Detail::make_unbuffered_channel;

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

