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
    return !channelp->receive(&value, waiter);
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
    Channel Implementation
*/
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
T
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
    Names/Types
*/
using std::forward;
using std::move;
    
    
#if 0
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
#endif


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
template<class W, class Q>
inline typename Basic_channel<T>::is_waiter_removed<W,Q>
Basic_channel<T>::is_removed(const W& w, const Q& q)
{
    return is_waiter_removed<W,Q>(w,q);
}


template<class T>
inline T
Basic_channel<T>::pop(Buffer* bufp)
{
    T x;

    bufp->pop(&x);
    return x;
}


template<class T>
inline bool
Basic_channel<T>::receive(T* valuep, Goroutine::Handle receiver)
{
    bool is_received{true};
    Lock lock{mutex};

    if (!senderq.is_empty())
        release(&senderq, valuep);
    else if (!buffer.is_empty())
        buffer.pop(valuep);
    else {
        suspend(receiver, valuep, &receiverq);
        is_received = false;
    }

    return is_received;
}


template<class T>
void
Basic_channel<T>::receiver_wait(Lock& lock, Receiver_queue* waitqp, T* valuep)
{
    condition_variable  signal;
    const Receiver      receiver{&signal, valuep};

    waitqp->push(receiver);
    signal.wait(lock, is_released(receiver, *waitqp));
}


template<class T>
inline void
Basic_channel<T>::release(Sender_queue* waitqp, T* valuep)
{
    const Sender sender = waitqp->pop();
    sender.release(valuep);
}


template<class T>
inline T
Basic_channel<T>::release(Sender_queue* waitqp)
{
    Value           x;
    const Sender    sender = waitqp->pop();

    sender.release(&x);
    return x;
}


template<class T>
template<class U>
inline void
Basic_channel<T>::release(Receiver_queue* waitqp, U&& value)
{
    using std::forward;

    const Receiver receiver = waitqp->pop();
    receiver.release(forward(data))
}


template<class T>
template<class U>
inline bool
Basic_channel<T>::send(U* valuep, Goroutine::Handle sender)
{
    bool is_sent{true};
    Lock lock{mutex};

    if (!receiverq.is_empty())
        release(&receiverq, move(*valuep));
    else if (!buffer.is_full())
        buffer.push(move(*valuep));
    else {
        suspend(sender, valuep, &senderq)
        is_sent = false;
    }

    return is_sent;
}


template<class T>
template<class U>
void
Basic_channel<T>::sender_wait(Lock& lock, Sender_queue* waitqp, U* valuep)
{
    condition_variable  signal;
    const Sender        sender{&signal, valuep};

    waitqp->push(sender);
    signal.wait(lock, is_removed(sender, *waitqp));
}


template<class T>
inline Channel_size
Basic_channel<T>::size() const
{
    return buffer.size();
}


template<class T>
inline void
Basic_channel<T>::suspend(Goroutine::Handle g, T* valuep, Receiver_queue* waitqp)
{
    const Receiver receiver{g, valuep};

    receiver.suspend();
    waitqp->push(receiver);
}


template<class T>
template<class U>
inline void
Basic_channel<T>::suspend(Goroutine::Handle g, U* valuep, Sender_queue* waitqp)
{
    const Sender sender{g, valuep};

    sender.suspend();
    waitqp->push(sender);
}


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
        receiver_wait(lock, &receiverq, &value);

    return value;
}


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
        sender_wait(lock, &senderq, &value);
}


template<class T>
inline optional<T>
Basic_channel<T>::try_receive()
{
    optional<T> result;
    Lock        lock{mutex};

    if (!senderq.is_empty())
        result = release(&senderq);
    else if (!buffer.is_empty())
        result = pop(&buffer);

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


/*
    Basic Channel Buffer
*/
template<class T>
inline
Basic_channel<T>::Buffer::Buffer(Channel_size maxsize)
    : sizemax(maxsize >=0 ? maxsize : 0)
{
    assert(maxsize >= 0)
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
inline void
Basic_channel<T>::Buffer::pop(T* xp)
{
    using std::move;

    *xp = move(q.front());
    q.pop();
}


template<class T>
template<class U>
inline void
Basic_channel<T>::Buffer::push(U&& x)
{
    using std::forward;

    q.push(forward(x));
}


template<class T>
inline Channel_size
Basic_channel<T>::Buffer::size() const
{
    return static_cast<Channel_size>(q.size());
}


/*
    Basic Channel Waiter Removed Predicate
*/
template<class T>
template<class W, class Q>
inline
Basic_channel<T>::is_waiter_removed<W,Q>::is_waiter_removed(const W& waiter, const Q& queue)
    : w{waiter}
    , q{queue}
{
}


template<class T>
template<class W, class Q>
inline bool
Basic_channel<T>::is_waiter_removed<W,Q>::operator()() const
{
    return !q.find(w);
}


/*
    Waiting Sender
*/
template<class T>
inline
Waiting_sender<T>::Waiting_sender(Goroutine::Handle sender, const T* valuep)
    : g{sender}
    , sigp{nullptr}
    , readonlyp{valuep}
    , movablep{nullptr}
{
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(Goroutine::Handle sender, T* valuep)
    : g{sender}
    , sigp{nullptr}
    , readonlyp(nullptr)
    , movablep{valuep}
{
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(condition_variable* signalp, const T* valuep)
    : sigp{signalp}
    , readonlyp{valuep}
    , movablep{nullptr}
{
}


template<class T>
inline
Waiting_sender<T>::Waiting_sender(condition_variable* signalp, T* valuep)
    : sigp{sigp}
    , readonlyp(nullptr)
    , movablep{valuep}
{
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
    if (movablep)
        *valuep = move(*movablep);
    else
        *valuep = *readonlyp;
}


template<class T>
inline condition_variable*
Waiting_sender<T>::signal() const
{
    return sigp;
}


template<class T>
inline bool
operator==(const Waiting_sender<T>& x, const Waiting_sender<T>& y)
{
    if (x.sigp == y.sigp) return true;
    if (x.g == y.g) return true;
    return false;
}


}   // Implementation Details


/*
    Channel
*/
template<class T>
Channel<T>
make_channel(Channel_size n)
{
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

