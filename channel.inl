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
Receive_channel<T>::Awaitable::await_suspend(Goroutine::Handle receiver)
{
    Channel_operation_array<T, 1>   ops;
    Channel_receive<T>              receive(channelp, &value, receiver)

    ops.push_back(receive);
    return !channelp->receive(&value, receiver);
}

template<class T>
inline T&&
Receive_channel<T>::Awaitable::await_resume()
{
    return std::move(value);
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
Send_channel<T>::send(const T& value) const
{
    return Awaitable_copy(ifacep.get(), &value);
}


template<class T>
inline typename Send_channel<T>::Awaitable_move
Send_channel<T>::send(T&& value) const
{
    using std::move;
    return Awaitable_move(ifacep.get(), move(value));
}


template<class T>
inline Channel_size
Send_channel<T>::size() const
{
    return ifacep->size();
}


template<class T>
inline void
Send_channel<T>::sync_send(const T& value) const
{
    ifacep->sync_send(value);
}


template<class T>
inline void
Send_channel<T>::sync_send(T&& value) const
{
    using std::move;
    ifacep->sync_send(move(value));
}


template<class T>
inline bool
Send_channel<T>::try_send(const T& value) const
{
    return ifacep->try_send(value);
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
Send_channel<T>::Awaitable_copy::Awaitable_copy(Interface* ifacep, const T* argp)
    : channelp(ifacep)
    , valuep(argp)
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
    return !channelp->send(valuep, sender);
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
Send_channel<T>::Awaitable_move::Awaitable_move(Interface* ifacep, T&& arg)
    : channelp(ifacep)
    , value{std::move(arg)}
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
    return !channelp->send(&value, sender);
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
Channel<T>::send(const T& value) const
{
    return Awaitable_send_copy(ifacep.get(), &value);
}


template<class T>
inline typename Channel<T>::Awaitable_send_move
Channel<T>::send(T&& value) const
{
    using std::move;
    return Awaitable_send_move(ifacep.get(), move(value));
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
Channel<T>::sync_send(const T& value) const
{
    return ifacep->sync_send(value);
}


template<class T>
inline void
Channel<T>::sync_send(T&& value) const
{
    using std::move;
    return ifacep->sync_send(move(value));
}


template<class T>
inline optional<T>
Channel<T>::try_receive() const
{
    return ifacep->try_receive();
}


template<class T>
inline bool
Channel<T>::try_send(const T& value) const
{
    return ifacep->try_send(value);
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
    Channel Implementation
*/
template<class M, class T>
inline
Channel_impl<M,T>::Channel_impl(Channel_size n)
    : chan(n)
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
Channel_impl<M,T>::receive(T* valuep, Goroutine::Handle receiver)
{
    return chan.receive(valuep, receiver);
}


template<class M, class T>
bool
Channel_impl<M,T>::send(const T* valuep, Goroutine::Handle sender)
{
    return chan.send(valuep, sender);
}


template<class M, class T>
bool
Channel_impl<M,T>::send(T* valuep, Goroutine::Handle sender)
{
    return chan.send(valuep, sender);
}


template<class M, class T>
Channel_size
Channel_impl<M,T>::size() const
{
    return chan.size();
}


template<class M, class T>
T
Channel_impl<M,T>::sync_receive()
{
    return chan.sync_receive();
}


template<class M, class T>
void
Channel_impl<M,T>::sync_send(const T& value)
{
    chan.sync_send(value);
}


template<class M, class T>
void
Channel_impl<M,T>::sync_send(T&& value)
{
    chan.sync_send(value);
}


template<class M, class T>
optional<T>
Channel_impl<M,T>::try_receive()
{
    return chan.try_receive();
}


template<class M, class T>
bool
Channel_impl<M,T>::try_send(const T& value)
{
    return chan.try_send(value);
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


template<class M>
inline std::shared_ptr<Channel_impl<M>>
make_channel_impl(M&& model)
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
    Names/Types
*/
using std::move;
    
    
/*
    Basic Channel
*/
template<class T>
inline
Basic_channel<T>::Basic_channel(Channel_size bufsize)
    : buffer{bufsize}
{
}


template<class T>
inline Channel_size
Basic_channel<T>::capacity() const
{
    return buffer.max_size();
}


template<class T>
inline bool
Basic_channel<T>::receive(T* valuep, Goroutine::Handle receiver)
{
    bool is_received{true};
    Lock lock{mutex};

    if (!receiverq.is_empty()) {
        suspend(&receiverq, receiver, valuep);
        is_received = false;
    } else if (!buffer.is_empty()) {
        buffer.pop(valuep);
        if (!senderq.is_empty())
            release(&senderq, &buffer);
    } else if (!senderq.is_empty()) {
        release(&senderq, valuep);
    } else {
        suspend(&receiverq, receiver, valuep);
        is_received = false;
    }

    return is_received;
}


template<class T>
inline void
Basic_channel<T>::release(Sender_queue* waitqp, T* valuep)
{
    const Sender sender = waitqp->pop();
    sender.release(valuep);
}


template<class T>
inline void
Basic_channel<T>::release(Sender_queue* waitqp, Buffer* bufp)
{
    const Sender sender = waitqp->pop();
    bufp->push(sender.release());
}


template<class T>
inline T
Basic_channel<T>::release(Sender_queue* waitqp)
{
    const Sender    sender = waitqp->pop();
    T               value;

    sender.release(&value);
    return value;
}


template<class T>
template<class U>
inline void
Basic_channel<T>::release(Receiver_queue* waitqp, U&& value)
{
    const Receiver receiver = waitqp->pop();
    receiver.release(move(value));
}


template<class T>
template<class U>
inline bool
Basic_channel<T>::send(U* valuep, Goroutine::Handle sender)
{
    bool is_sent{true};
    Lock lock{mutex};

    if (!senderq.is_empty()) {
        suspend(&senderq, sender, valuep);
        is_sent = false;
    } else if (!receiverq.is_empty()) {
        release(&receiverq, move(*valuep));
    } else if (!buffer.is_full()) {
        buffer.push(move(*valuep));
    } else {
        suspend(&senderq, sender, valuep);
        is_sent = false;
    }

    return is_sent;
}


template<class T>
inline Channel_size
Basic_channel<T>::size() const
{
    return buffer.size();
}


template<class T>
inline void
Basic_channel<T>::suspend(Receiver_queue* waitqp, Goroutine::Handle receiver, T* valuep)
{
    const Receiver waiter{receiver, valuep};

    waitqp->push(waiter);
    scheduler.suspend(receiver);
}


template<class T>
template<class U>
inline void
Basic_channel<T>::suspend(Sender_queue* waitqp, Goroutine::Handle sender, U* valuep)
{
    const Sender waiter{sender, valuep};

    waitqp->push(waiter);
    scheduler.suspend(sender);
}


#if 0
template<class T>
inline T
Basic_channel<T>::sync_receive()
{
    T       value;
    Lock    lock{mutex};

    if (!senderq.is_empty())
        release(&senderq, &value);
    else if (!buffer.is_empty())
        buffer.pop(&value);
    else
        wait_for_sender(lock, &receiverq, &value);

    return value;
}
#endif


template<class T>
inline T
Basic_channel<T>::sync_receive()
{
    T       value;
    Lock    lock{mutex};

    if (!receiverq.is_empty()) {
        wait_for_sender(lock, &receiverq, &value);
    } else if (!buffer.is_empty()) {
        buffer.pop(&value);
        if (!senderq.is_empty())
            release(&senderq, &buffer);
    } else if (!senderq.is_empty()) {
        release(&senderq, &value);
    } else {
        wait_for_sender(lock, &receiverq, &value);
    }

    return value;
}


template<class T>
template<class U>
inline void
Basic_channel<T>::sync_send(U&& value)
{
    Lock lock{mutex};

    if (!senderq.is_empty())
        wait_for_receiver(lock, &senderq, &value);
    else if (!receiverq.is_empty())
         release(&receiverq, move(value));
    else if (!buffer.is_full())
        buffer.push(move(value));
    else
        wait_for_receiver(lock, &senderq, &value);
}


#if 0
template<class T>
template<class U>
inline void
Basic_channel<T>::sync_send(U&& value)
{
    bool is_sent{true};
    Lock lock{mutex};

    if (!receiverq.is_empty())
        release(&receiverq, move(value));
    else if (!buffer.is_full())
        buffer.push(move(value));
    else
        wait_for_receiver(lock, &senderq, &value);
}
#endif


template<class T>
inline optional<T>
Basic_channel<T>::try_receive()
{
    optional<T> result;
    Lock        lock{mutex};

    if (!buffer.is_empty())
        result = buffer.pop();
    else if (!senderq.is_empty())
        result = release(&senderq);

    return result;
}


template<class T>
inline bool
Basic_channel<T>::try_send(const T& value)
{
    bool is_sent{true};
    Lock lock{mutex};

    if (!receiverq.is_empty())
        release(&receiverq, value);
    else if (!buffer.is_full())
        buffer.push(value);
    else
        is_sent = false;

    return is_sent;
}


template<class T>
template<class U>
void
Basic_channel<T>::wait_for_receiver(Lock& lock, Sender_queue* waitqp, U* valuep)
{
    condition_variable  signal;
    const Sender        sender{&signal, valuep};

    waitqp->push(sender);
    signal.wait(lock, [&]{ return !waitqp->find(sender); });
}


template<class T>
void
Basic_channel<T>::wait_for_sender(Lock& lock, Receiver_queue* waitqp, T* valuep)
{
    condition_variable  signal;
    const Receiver      receiver{&signal, valuep};

    waitqp->push(receiver);
    signal.wait(lock, [&]{ return !waitqp->find(receiver); });
}


/*
    Basic Channel Buffer
*/
template<class T>
inline
Basic_channel<T>::Buffer::Buffer(Channel_size maxsize)
    : sizemax(maxsize >=0 ? maxsize : 0)
{
    assert(maxsize >= 0);
}


template<class T>
inline bool
Basic_channel<T>::Buffer::is_empty() const
{
    return q.empty();
}


template<class T>
inline bool
Basic_channel<T>::Buffer::is_full() const
{
    return size() == max_size();
}


template<class T>
inline Channel_size
Basic_channel<T>::Buffer::max_size() const
{
    return sizemax;
}


template<class T>
inline T
Basic_channel<T>::Buffer::pop()
{
    T value;

    pop(&value);
    return value;
}


template<class T>
inline void
Basic_channel<T>::Buffer::pop(T* valuep)
{
    using std::move;

    *valuep = move(q.front());
    q.pop();
}


template<class T>
template<class U>
inline void
Basic_channel<T>::Buffer::push(U&& value)
{
    using std::move;

    q.push(move(value));
}


template<class T>
inline Channel_size
Basic_channel<T>::Buffer::size() const
{
    return static_cast<Channel_size>(q.size());
}


/*
    Wait Queue
*/
template<class T>
inline bool
Wait_queue<T>::is_empty() const
{
    return ws.empty();
}


template<class T>
inline bool
Wait_queue<T>::find(const Waiter& w) const
{
    using std::find;

    const auto p = find(ws.begin(), ws.end(), w);
    return p != ws.end();
}


template<class T>
inline T
Wait_queue<T>::pop()
{
    const T w = ws.front();

    ws.pop_front();
    return w;
}


template<class T>
inline void
Wait_queue<T>::push(const Waiter& w)
{
    ws.push_back(w);
}


/*
    Waiting Receiver
*/
template<class T>
inline
Waiting_receiver<T>::Waiting_receiver(Goroutine::Handle receiver, T* valuep)
    : g{receiver}
    , threadsigp{nullptr}
    , bufferp(valuep)
{
}


template<class T>
inline
Waiting_receiver<T>::Waiting_receiver(condition_variable* signalp, T* valuep)
    : threadsigp(signalp)
    , bufferp(valuep)
{
}


template<class T>
inline condition_variable*
Waiting_receiver<T>::signal() const
{
    return threadsigp;
}


template<class T>
inline Goroutine::Handle
Waiting_receiver<T>::goroutine() const
{
    return g;
}


template<class T>
template<class U>
inline void
Waiting_receiver<T>::release(U&& sent) const
{
    *bufferp = move(sent);
    if (g)
        scheduler.resume(g);
    else if (threadsigp)
        threadsigp->notify_one();
}


template<class T>
inline bool
operator==(const Waiting_receiver<T>& x, const Waiting_receiver<T>& y)
{
    return x.threadsigp ? (x.threadsigp == y.threadsigp) : (x.g == y.g);
}


/*
    Waiting Sender
*/
template<class T>
inline
Waiting_sender<T>::Waiting_sender(Goroutine::Handle sender, const T* valuep)
    : g{sender}
    , threadsigp{nullptr}
    , readablep{valuep}
    , writablep{nullptr}
{
    assert(sender);
    assert(valuep != nullptr);
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(Goroutine::Handle sender, T* valuep)
    : g{sender}
    , threadsigp{nullptr}
    , readablep(nullptr)
    , writablep{valuep}
{
    assert(sender);
    assert(valuep != nullptr);
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(condition_variable* signalp, const T* valuep)
    : threadsigp{signalp}
    , readablep{valuep}
    , writablep{nullptr}
{
    assert(signalp != nullptr);
    assert(valuep != nullptr);
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(condition_variable* signalp, T* valuep)
    : threadsigp{signalp}
    , readablep(nullptr)
    , writablep{valuep}
{
    assert(signalp != nullptr);
    assert(valuep != nullptr);
}


template<class T>
inline Goroutine::Handle
Waiting_sender<T>::goroutine() const
{
    return g;
}


template<class T>
inline void
Waiting_sender<T>::release(T* valuep) const
{
    *valuep = writablep ? move(*writablep) : *readablep;

    if (g)
        scheduler.resume(g);
    else if (threadsigp)
        threadsigp->notify_one();
}


template<class T>
inline T
Waiting_sender<T>::release() const
{
    T value;

    release(&value);
    return value;
}


template<class T>
inline condition_variable*
Waiting_sender<T>::signal() const
{
    return threadsigp;
}


template<class T>
inline bool
operator==(const Waiting_sender<T>& x, const Waiting_sender<T>& y)
{
    return x.threadsigp ? (x.threadsigp == y.threadsigp) : (x.g == y.g);
}


}   // Implementation Details


/*
    Channel
*/
template<class T>
Channel<T>
make_channel(Channel_size n)
{
    using Detail::Basic_channel;

    return make_channel_impl<Basic_channel<T>>(n);
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

